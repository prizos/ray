#include "game.h"
#include <math.h>
#include <stdlib.h>

// ============ INITIALIZATION ============

static void game_init_common(GameState *state)
{
    float grid_center_x = (GRID_WIDTH * CELL_SIZE) / 2.0f;
    float grid_center_z = (GRID_HEIGHT * CELL_SIZE) / 2.0f;

    // Camera setup (further back for larger map)
    state->camera.position = (Vector3){ grid_center_x - 160.0f, 120.0f, grid_center_z + 160.0f };
    state->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    state->camera.fovy = 60.0f;
    state->camera.projection = CAMERA_PERSPECTIVE;

    float dx = grid_center_x - state->camera.position.x;
    float dy = 30.0f - state->camera.position.y;
    float dz = grid_center_z - state->camera.position.z;
    state->camera_yaw = atan2f(dx, dz);
    state->camera_pitch = atan2f(dy, sqrtf(dx * dx + dz * dz));

    float cos_pitch = cosf(state->camera_pitch);
    state->camera.target = (Vector3){
        state->camera.position.x + sinf(state->camera_yaw) * cos_pitch,
        state->camera.position.y + sinf(state->camera_pitch),
        state->camera.position.z + cosf(state->camera_yaw) * cos_pitch
    };

    // Game state
    state->running = true;
    state->growth_timer = 0;
    state->burn_timer = 0;
    state->regen_timer = 0;
    state->paused = false;
    state->current_tool = TOOL_TREE;

    // Initialize terrain
    terrain_generate(state->terrain_height);
    terrain_burn_init(state->terrain_burn, state->terrain_burn_timer);

    // Initialize beavers
    beaver_init_all(state->beavers, &state->beaver_count);

    // Allocate trees
    if (state->trees == NULL) {
        state->trees = (Tree *)malloc(sizeof(Tree) * MAX_TREES);
        if (state->trees == NULL) {
            TraceLog(LOG_ERROR, "Failed to allocate trees!");
            state->running = false;
            return;
        }
        TraceLog(LOG_INFO, "Allocated %zu bytes for %d trees", sizeof(Tree) * MAX_TREES, MAX_TREES);
    }
    state->tree_count = 0;
}

void game_init(GameState *state)
{
    game_init_common(state);
    if (!state->running) return;

    // Default: grid-spaced trees
    int spacing = 10;
    for (int x = 5; x < GRID_WIDTH - 5; x += spacing) {
        for (int z = 5; z < GRID_HEIGHT - 5; z += spacing) {
            if (state->tree_count >= MAX_TREES) break;

            int terrain_x = (int)(x * CELL_SIZE / TERRAIN_SCALE);
            int terrain_z = (int)(z * CELL_SIZE / TERRAIN_SCALE);
            if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
            if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
            int ground_height = state->terrain_height[terrain_x][terrain_z];

            if (ground_height < WATER_LEVEL) continue;

            tree_init(&state->trees[state->tree_count], x, ground_height, z, TREE_SPACE_COLONIZATION);
            state->tree_count++;
        }
    }
}

void game_init_with_trees(GameState *state, int num_trees)
{
    game_init_common(state);
    if (!state->running) return;

    if (num_trees > MAX_TREES) num_trees = MAX_TREES;
    if (num_trees < 0) num_trees = 0;

    // Place trees in a grid pattern to fit requested count
    int placed = 0;
    int attempts = 0;
    int max_attempts = num_trees * 10;

    while (placed < num_trees && attempts < max_attempts) {
        int x = 2 + (attempts * 7) % (GRID_WIDTH - 4);
        int z = 2 + (attempts * 11) % (GRID_HEIGHT - 4);

        int terrain_x = (int)(x * CELL_SIZE / TERRAIN_SCALE);
        int terrain_z = (int)(z * CELL_SIZE / TERRAIN_SCALE);
        if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
        if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
        int ground_height = state->terrain_height[terrain_x][terrain_z];

        if (ground_height >= WATER_LEVEL) {
            tree_init(&state->trees[state->tree_count], x, ground_height, z, TREE_SPACE_COLONIZATION);
            state->tree_count++;
            placed++;
        }
        attempts++;
    }

    TraceLog(LOG_INFO, "Initialized %d trees (requested %d)", placed, num_trees);
}

// ============ UPDATE ============

void game_update(GameState *state)
{
    float delta = GetFrameTime();

    // ========== INPUT HANDLING ==========
    if (IsKeyPressed(KEY_ESCAPE)) {
        state->running = false;
    }

    if (IsKeyPressed(KEY_SPACE)) {
        state->paused = !state->paused;
    }

    if (IsKeyPressed(KEY_R)) {
        game_init(state);
        return;
    }

    // Tool switching
    if (IsKeyPressed(KEY_ONE)) {
        state->current_tool = TOOL_BURN;
    }
    if (IsKeyPressed(KEY_TWO)) {
        state->current_tool = TOOL_TREE;
    }

    // ========== CLICK HANDLING ==========
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Ray ray = GetMouseRay(GetMousePosition(), state->camera);

        float avg_height = 5.0f * TERRAIN_SCALE;
        if (ray.direction.y != 0) {
            float t = (avg_height - ray.position.y) / ray.direction.y;
            if (t > 0) {
                float hit_x = ray.position.x + ray.direction.x * t;
                float hit_z = ray.position.z + ray.direction.z * t;

                int terrain_x = (int)(hit_x / TERRAIN_SCALE);
                int terrain_z = (int)(hit_z / TERRAIN_SCALE);
                if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
                if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
                if (terrain_x < 0) terrain_x = 0;
                if (terrain_z < 0) terrain_z = 0;

                if (state->current_tool == TOOL_TREE) {
                    int grid_x = (int)(hit_x / CELL_SIZE);
                    int grid_z = (int)(hit_z / CELL_SIZE);

                    if (grid_x >= 0 && grid_x < GRID_WIDTH &&
                        grid_z >= 0 && grid_z < GRID_HEIGHT &&
                        state->tree_count < MAX_TREES) {

                        int ground_height = state->terrain_height[terrain_x][terrain_z];

                        if (ground_height >= WATER_LEVEL) {
                            tree_init(&state->trees[state->tree_count], grid_x, ground_height, grid_z, TREE_SPACE_COLONIZATION);
                            state->tree_count++;
                        }
                    }
                } else if (state->current_tool == TOOL_BURN) {
                    if (state->terrain_burn[terrain_x][terrain_z] == TERRAIN_NORMAL) {
                        state->terrain_burn[terrain_x][terrain_z] = TERRAIN_BURNING;
                        state->terrain_burn_timer[terrain_x][terrain_z] = BURN_DURATION;
                    }
                }
            }
        }
    }

    // ========== CAMERA CONTROLS ==========
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouse_delta = GetMouseDelta();
        state->camera_yaw += mouse_delta.x * LOOK_SPEED * 0.003f;
        state->camera_pitch -= mouse_delta.y * LOOK_SPEED * 0.003f;

        if (state->camera_pitch > 1.4f) state->camera_pitch = 1.4f;
        if (state->camera_pitch < -1.4f) state->camera_pitch = -1.4f;
    }

    float cos_yaw = cosf(state->camera_yaw);
    float sin_yaw = sinf(state->camera_yaw);
    Vector3 forward = { sin_yaw, 0, cos_yaw };
    Vector3 right = { cos_yaw, 0, -sin_yaw };

    float speed = MOVE_SPEED * delta;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        speed *= 2.5f;
    }

    if (IsKeyDown(KEY_W)) {
        state->camera.position.x += forward.x * speed;
        state->camera.position.z += forward.z * speed;
    }
    if (IsKeyDown(KEY_S)) {
        state->camera.position.x -= forward.x * speed;
        state->camera.position.z -= forward.z * speed;
    }
    if (IsKeyDown(KEY_A)) {
        state->camera.position.x -= right.x * speed;
        state->camera.position.z -= right.z * speed;
    }
    if (IsKeyDown(KEY_D)) {
        state->camera.position.x += right.x * speed;
        state->camera.position.z += right.z * speed;
    }
    if (IsKeyDown(KEY_Q)) state->camera.position.y -= speed;
    if (IsKeyDown(KEY_E)) state->camera.position.y += speed;

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        float cos_pitch = cosf(state->camera_pitch);
        float zoom_speed = wheel * 10.0f;
        state->camera.position.x += sin_yaw * cos_pitch * zoom_speed;
        state->camera.position.y += sinf(state->camera_pitch) * zoom_speed;
        state->camera.position.z += cos_yaw * cos_pitch * zoom_speed;
    }

    float cos_pitch = cosf(state->camera_pitch);
    state->camera.target = (Vector3){
        state->camera.position.x + sin_yaw * cos_pitch,
        state->camera.position.y + sinf(state->camera_pitch),
        state->camera.position.z + cos_yaw * cos_pitch
    };

    // ========== FIRE SPREAD AND BURN ==========
    state->burn_timer += delta;
    if (state->burn_timer >= BURN_SPREAD_INTERVAL) {
        state->burn_timer = 0;
        terrain_burn_update(state->terrain_burn, state->terrain_burn_timer,
                           state->terrain_height, state->trees, state->tree_count);
    }

    // ========== TREE REGENERATION ==========
    state->regen_timer += delta;
    if (state->regen_timer >= REGEN_INTERVAL) {
        state->regen_timer = 0;
        terrain_regenerate(state->terrain_burn, state->trees, state->tree_count);
    }

    // ========== BEAVER UPDATE ==========
    beaver_update(state->beavers, &state->beaver_count,
                  state->trees, state->tree_count,
                  state->terrain_height, delta);

    // ========== TREE GROWTH ==========
    if (!state->paused) {
        state->growth_timer += delta;
        if (state->growth_timer >= GROWTH_INTERVAL) {
            state->growth_timer = 0;

            for (int i = 0; i < state->tree_count; i++) {
                tree_grow(&state->trees[i]);
            }
        }
    }
}

// ============ CLEANUP ============

void game_cleanup(GameState *state)
{
    if (state->trees != NULL) {
        free(state->trees);
        state->trees = NULL;
    }
}
