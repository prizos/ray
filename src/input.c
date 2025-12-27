#include "input.h"
#include "audio.h"
#include "raymath.h"
#include <math.h>

void input_update_player(Player *player, float delta_time)
{
    // Thrust (Space bar)
    player->is_thrusting = IsKeyDown(KEY_SPACE);

    // Calculate forward direction based on yaw (horizontal only)
    float forward_x = sinf(player->yaw);
    float forward_z = -cosf(player->yaw);

    // Right vector (perpendicular to forward)
    float right_x = cosf(player->yaw);
    float right_z = sinf(player->yaw);

    // Horizontal movement (WASD)
    float move_x = 0.0f;
    float move_z = 0.0f;

    if (IsKeyDown(KEY_W)) {
        move_x += forward_x;
        move_z += forward_z;
    }
    if (IsKeyDown(KEY_S)) {
        move_x -= forward_x;
        move_z -= forward_z;
    }
    if (IsKeyDown(KEY_A)) {
        move_x -= right_x;
        move_z -= right_z;
    }
    if (IsKeyDown(KEY_D)) {
        move_x += right_x;
        move_z += right_z;
    }

    // Normalize and apply horizontal movement
    float move_len = sqrtf(move_x * move_x + move_z * move_z);
    if (move_len > 0.0f) {
        move_x /= move_len;
        move_z /= move_len;
        player->velocity.x = move_x * HORIZONTAL_SPEED;
        player->velocity.z = move_z * HORIZONTAL_SPEED;
    } else {
        // Friction when not moving horizontally
        player->velocity.x *= 0.9f;
        player->velocity.z *= 0.9f;
    }

    // Look rotation (Arrow keys)
    if (IsKeyDown(KEY_LEFT)) {
        player->yaw -= LOOK_SPEED * delta_time;
    }
    if (IsKeyDown(KEY_RIGHT)) {
        player->yaw += LOOK_SPEED * delta_time;
    }
    if (IsKeyDown(KEY_UP)) {
        player->pitch += LOOK_SPEED * delta_time;
        if (player->pitch > 1.4f) player->pitch = 1.4f;
    }
    if (IsKeyDown(KEY_DOWN)) {
        player->pitch -= LOOK_SPEED * delta_time;
        if (player->pitch < -1.4f) player->pitch = -1.4f;
    }

    // Toggle music (M key)
    if (IsKeyPressed(KEY_M)) {
        audio_toggle_music();
    }
}

void input_update_camera(Camera3D *camera, const Player *player)
{
    // Camera follows player position
    camera->position = player->position;

    // Calculate look direction from yaw and pitch
    float cos_pitch = cosf(player->pitch);
    Vector3 look_dir = {
        sinf(player->yaw) * cos_pitch,
        sinf(player->pitch),
        -cosf(player->yaw) * cos_pitch
    };

    camera->target = Vector3Add(camera->position, look_dir);
    camera->up = (Vector3){ 0.0f, 1.0f, 0.0f };
}

bool input_quit_requested(void)
{
    return IsKeyPressed(KEY_ESCAPE);
}

bool input_reset_requested(void)
{
    return IsKeyPressed(KEY_R);
}
