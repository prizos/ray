#include "game.h"
#include "input.h"
#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Random float between min and max
static float random_float(float min, float max)
{
    return min + (float)GetRandomValue(0, 10000) / 10000.0f * (max - min);
}

static void reset_player(Player *player)
{
    player->position = (Vector3){ 0.0f, 5.0f, 15.0f };
    player->velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    player->yaw = 0.0f;
    player->pitch = 0.0f;
    player->fuel = STARTING_FUEL;
    player->is_thrusting = false;
    player->is_grounded = false;
}

void game_spawn_letter(GameState *state)
{
    // Find an inactive letter slot
    for (int i = 0; i < MAX_LETTERS; i++) {
        if (!state->letters[i].active) {
            Letter *letter = &state->letters[i];

            // Random position on the left side
            letter->position.x = random_float(SPAWN_X_MIN, SPAWN_X_MAX);
            letter->position.y = random_float(SPAWN_Y_MIN, SPAWN_Y_MAX);
            letter->position.z = random_float(SPAWN_Z_MIN, SPAWN_Z_MAX);

            // Velocity moving right (positive X)
            float speed = random_float(LETTER_SPEED_MIN, LETTER_SPEED_MAX);
            letter->velocity = (Vector3){ speed, 0.0f, 0.0f };

            // Random character
            const char *chars = LETTER_CHARS;
            int len = (int)strlen(chars);
            letter->character = chars[GetRandomValue(0, len - 1)];

            // Determine if dangerous or safe
            letter->is_dangerous = (GetRandomValue(0, 99) < DANGEROUS_LETTER_CHANCE);

            // Color based on type: red = dangerous, green = safe
            if (letter->is_dangerous) {
                letter->color = RED;
            } else {
                letter->color = GREEN;
            }

            letter->active = true;
            letter->was_hit = false;

            break;
        }
    }
}

bool game_check_letter_collision(Vector3 player_pos, Letter *letter)
{
    if (!letter->active || letter->was_hit) return false;

    // Simple sphere collision
    float dx = player_pos.x - letter->position.x;
    float dy = player_pos.y - letter->position.y;
    float dz = player_pos.z - letter->position.z;
    float dist_sq = dx * dx + dy * dy + dz * dz;

    float combined_radius = LETTER_COLLISION_RADIUS + PLAYER_RADIUS;
    return dist_sq < (combined_radius * combined_radius);
}

void game_init(GameState *state)
{
    // Initialize player
    reset_player(&state->player);

    // Setup camera - must initialize position and target!
    state->camera.position = state->player.position;
    state->camera.target = (Vector3){ 0.0f, 3.0f, 0.0f };
    state->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    state->camera.fovy = 60.0f;
    state->camera.projection = CAMERA_PERSPECTIVE;

    state->running = true;
    state->score = 0;

    // Initialize letters as inactive
    for (int i = 0; i < MAX_LETTERS; i++) {
        state->letters[i].active = false;
    }

    // Spawn timing
    state->spawn_timer = 0.0f;
    state->spawn_interval = 1.0f;

    // Spawn a few letters to start
    for (int i = 0; i < 3; i++) {
        game_spawn_letter(state);
    }

    // Initialize adversary (starts behind player)
    state->adversary.position = (Vector3){ 0.0f, 3.0f, 30.0f };
    state->adversary.active = true;
    state->adversary.hit_cooldown = 0.0f;
}

void game_update(GameState *state)
{
    float delta_time = GetFrameTime();
    Player *player = &state->player;

    // Handle reset
    if (input_reset_requested()) {
        reset_player(player);
    }

    // Process player input
    input_update_player(player, delta_time);

    // Apply gravity
    if (!player->is_grounded) {
        player->velocity.y -= GRAVITY * delta_time;

        // Clamp fall speed
        if (player->velocity.y < -MAX_FALL_SPEED) {
            player->velocity.y = -MAX_FALL_SPEED;
        }
    }

    // Apply thrust (only if we have fuel)
    if (player->is_thrusting && player->fuel > 0) {
        player->velocity.y += THRUST_POWER * delta_time;
        player->fuel -= FUEL_CONSUMPTION_RATE * delta_time;
        if (player->fuel < 0) player->fuel = 0;
    } else if (player->fuel <= 0) {
        player->is_thrusting = false;  // Can't thrust without fuel
    }

    // Update position
    player->position.x += player->velocity.x * delta_time;
    player->position.y += player->velocity.y * delta_time;
    player->position.z += player->velocity.z * delta_time;

    // Ground collision
    if (player->position.y <= GROUND_LEVEL) {
        player->position.y = GROUND_LEVEL;
        player->velocity.y = 0.0f;
        player->is_grounded = true;
    } else {
        player->is_grounded = false;
    }

    // Update camera to follow player
    input_update_camera(&state->camera, player);

    // Update spawn timer
    state->spawn_timer += delta_time;
    if (state->spawn_timer >= state->spawn_interval) {
        state->spawn_timer = 0.0f;
        game_spawn_letter(state);
    }

    // Update letters
    for (int i = 0; i < MAX_LETTERS; i++) {
        Letter *letter = &state->letters[i];
        if (!letter->active) continue;

        // Move letter
        letter->position.x += letter->velocity.x * delta_time;
        letter->position.y += letter->velocity.y * delta_time;
        letter->position.z += letter->velocity.z * delta_time;

        // Check if letter went off screen (despawn)
        if (letter->position.x > DESPAWN_X) {
            letter->active = false;
            continue;
        }

        // Check collision with player
        if (game_check_letter_collision(player->position, letter)) {
            letter->was_hit = true;
            letter->active = false;

            if (letter->is_dangerous) {
                // Dangerous letter: costs fuel but still gives score
                player->fuel -= FUEL_COST_PER_LETTER;
                if (player->fuel < 0) player->fuel = 0;
                state->score++;
            } else {
                // Safe letter: gives fuel and score
                player->fuel += FUEL_GAIN_PER_LETTER;
                if (player->fuel > MAX_FUEL) player->fuel = MAX_FUEL;
                state->score += 2;  // Bonus for safe letters
                audio_on_green_hit();  // Trigger hardcore music!
            }
        }
    }

    // Update adversary (chasing X)
    if (state->adversary.active) {
        Adversary *adv = &state->adversary;

        // Update hit cooldown
        if (adv->hit_cooldown > 0) {
            adv->hit_cooldown -= delta_time;
        }

        // Calculate direction to player
        float dx = player->position.x - adv->position.x;
        float dy = player->position.y - adv->position.y;
        float dz = player->position.z - adv->position.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);

        // Move towards player
        if (dist > 0.1f) {
            float speed = ADVERSARY_SPEED * delta_time;
            adv->position.x += (dx / dist) * speed;
            adv->position.y += (dy / dist) * speed;
            adv->position.z += (dz / dist) * speed;
        }

        // Keep adversary above ground
        if (adv->position.y < GROUND_LEVEL + 1.0f) {
            adv->position.y = GROUND_LEVEL + 1.0f;
        }

        // Check collision with player
        float combined_radius = ADVERSARY_RADIUS + PLAYER_RADIUS;
        if (dist < combined_radius && adv->hit_cooldown <= 0) {
            // Hit! Reduce score and stop hardcore music
            state->score -= ADVERSARY_SCORE_PENALTY;
            if (state->score < 0) state->score = 0;
            audio_on_adversary_hit();
            adv->hit_cooldown = 2.0f;  // 2 second cooldown before next hit
        }
    }

    // Check for ESC key quit
    if (input_quit_requested()) {
        state->running = false;
    }
}

void game_cleanup(GameState *state)
{
    (void)state;
}
