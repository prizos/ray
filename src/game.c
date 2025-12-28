#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

// ============ INITIALIZATION ============

static void game_init_common(GameState *state, uint32_t seed)
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
    state->paused = false;
    state->current_tool = TOOL_TREE;
    state->target_valid = false;

    // Initialize terrain with seed
    uint32_t actual_seed = seed;
    if (actual_seed == 0) {
        actual_seed = (uint32_t)time(NULL);  // Generate random seed
    }
    TerrainConfig config = terrain_config_default(actual_seed);
    terrain_generate_seeded(state->terrain_height, &config);

    // Initialize water simulation from terrain
    water_init(&state->water, state->terrain_height);

    // Add some initial test water in the center of the map
    for (int z = 75; z < 85; z++) {
        for (int x = 75; x < 85; x++) {
            water_add(&state->water, x, z, FLOAT_TO_FIXED(2.0f));
        }
    }

    // Initialize matter simulation (thermodynamic vegetation, fire, nutrients)
    // Uses same seed for reproducible sparse vegetation patterns
    matter_init(&state->matter, state->terrain_height, actual_seed);

    // Initialize beavers
    beaver_init_all(state->beavers, &state->beaver_count);

    // Allocate trees (start small, grow as needed)
    if (state->trees == NULL) {
        state->trees = (Tree *)malloc(sizeof(Tree) * INITIAL_TREES);
        if (state->trees == NULL) {
            TraceLog(LOG_ERROR, "Failed to allocate trees!");
            state->running = false;
            return;
        }
        state->tree_capacity = INITIAL_TREES;
        TraceLog(LOG_INFO, "Allocated initial capacity for %d trees", INITIAL_TREES);
    }
    state->tree_count = 0;
}

// Grow trees array if needed, returns true on success
static bool game_grow_trees(GameState *state) {
    if (state->tree_count < state->tree_capacity) {
        return true;  // No growth needed
    }
    if (state->tree_count >= MAX_TREES) {
        TraceLog(LOG_WARNING, "Cannot add more trees: at maximum (%d)", MAX_TREES);
        return false;
    }

    int new_capacity = state->tree_capacity * 2;
    if (new_capacity > MAX_TREES) {
        new_capacity = MAX_TREES;
    }

    Tree *new_trees = (Tree *)realloc(state->trees, sizeof(Tree) * new_capacity);
    if (!new_trees) {
        TraceLog(LOG_ERROR, "Failed to grow trees array to %d", new_capacity);
        return false;
    }

    state->trees = new_trees;
    state->tree_capacity = new_capacity;
    TraceLog(LOG_INFO, "Grew trees array to capacity %d", new_capacity);
    return true;
}

void game_init(GameState *state)
{
    game_init_full(state, -1, 0);
}

void game_init_with_seed(GameState *state, uint32_t seed)
{
    game_init_full(state, -1, seed);
}

void game_init_with_trees(GameState *state, int num_trees)
{
    game_init_full(state, num_trees, 0);
}

void game_init_full(GameState *state, int num_trees, uint32_t seed)
{
    game_init_common(state, seed);
    if (!state->running) return;

    if (num_trees < 0) {
        // Default: grid-spaced trees
        int spacing = 10;
        for (int x = 5; x < GRID_WIDTH - 5; x += spacing) {
            for (int z = 5; z < GRID_HEIGHT - 5; z += spacing) {
                // Get terrain height at grid cell center (grid cells are 2x2 terrain cells)
                // Tree renders at x * CELL_SIZE + CELL_SIZE/2, which is terrain cell 2*x + 1
                int terrain_x = x * 2 + 1;
                int terrain_z = z * 2 + 1;
                if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
                if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
                int ground_height = state->terrain_height[terrain_x][terrain_z];

                // Skip if there's water at this location
                if (water_get_depth(&state->water, terrain_x, terrain_z) > WATER_MIN_DEPTH) continue;

                if (!game_grow_trees(state)) break;
                tree_init(&state->trees[state->tree_count], x, ground_height, z, TREE_SPACE_COLONIZATION);
                state->tree_count++;
            }
        }
    } else {
        // Place specific number of trees
        if (num_trees > MAX_TREES) num_trees = MAX_TREES;

        int placed = 0;
        int attempts = 0;
        int max_attempts = num_trees * 10;

        while (placed < num_trees && attempts < max_attempts) {
            int x = 2 + (attempts * 7) % (GRID_WIDTH - 4);
            int z = 2 + (attempts * 11) % (GRID_HEIGHT - 4);

            // Get terrain height at grid cell center (grid cells are 2x2 terrain cells)
            int terrain_x = x * 2 + 1;
            int terrain_z = z * 2 + 1;
            if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
            if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
            int ground_height = state->terrain_height[terrain_x][terrain_z];

            // Only place tree if there's no water at this location
            if (water_get_depth(&state->water, terrain_x, terrain_z) <= WATER_MIN_DEPTH) {
                if (!game_grow_trees(state)) break;
                tree_init(&state->trees[state->tree_count], x, ground_height, z, TREE_SPACE_COLONIZATION);
                state->tree_count++;
                placed++;
            }
            attempts++;
        }

        TraceLog(LOG_INFO, "Initialized %d trees (requested %d)", placed, num_trees);
    }
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
        state->current_tool = TOOL_HEAT;
    }
    if (IsKeyPressed(KEY_TWO)) {
        state->current_tool = TOOL_TREE;
    }
    if (IsKeyPressed(KEY_THREE)) {
        state->current_tool = TOOL_WATER;
    }

    // ========== TARGET INDICATOR (raycast against actual terrain) ==========
    state->target_valid = false;
    {
        Ray ray = GetMouseRay(GetMousePosition(), state->camera);

        // Only proceed if ray is pointing downward
        if (ray.direction.y < -0.01f) {
            // Ray march to find terrain intersection
            float step_size = 2.0f;  // Step size in world units
            float max_dist = 500.0f;  // Maximum ray distance

            for (float t = 0; t < max_dist; t += step_size) {
                float px = ray.position.x + ray.direction.x * t;
                float py = ray.position.y + ray.direction.y * t;
                float pz = ray.position.z + ray.direction.z * t;

                // Convert to terrain coordinates
                int terrain_x = (int)(px / TERRAIN_SCALE);
                int terrain_z = (int)(pz / TERRAIN_SCALE);

                // Check bounds
                if (terrain_x < 0 || terrain_x >= TERRAIN_RESOLUTION ||
                    terrain_z < 0 || terrain_z >= TERRAIN_RESOLUTION) {
                    continue;
                }

                // Get terrain height at this position
                float terrain_y = state->terrain_height[terrain_x][terrain_z] * TERRAIN_SCALE;

                // Check if ray has gone below terrain
                if (py <= terrain_y + TERRAIN_SCALE) {
                    // Found intersection - convert to grid cell
                    int grid_x = (int)(px / CELL_SIZE);
                    int grid_z = (int)(pz / CELL_SIZE);

                    if (grid_x >= 0 && grid_x < GRID_WIDTH &&
                        grid_z >= 0 && grid_z < GRID_HEIGHT) {

                        // Get terrain height at grid cell center
                        int cell_terrain_x = grid_x * 2 + 1;
                        int cell_terrain_z = grid_z * 2 + 1;
                        if (cell_terrain_x >= TERRAIN_RESOLUTION) cell_terrain_x = TERRAIN_RESOLUTION - 1;
                        if (cell_terrain_z >= TERRAIN_RESOLUTION) cell_terrain_z = TERRAIN_RESOLUTION - 1;
                        int ground_height = state->terrain_height[cell_terrain_x][cell_terrain_z];

                        state->target_valid = true;
                        state->target_grid_x = grid_x;
                        state->target_grid_z = grid_z;
                        // World position at cell center
                        state->target_world_x = grid_x * CELL_SIZE + CELL_SIZE / 2.0f;
                        state->target_world_y = ground_height * TERRAIN_SCALE + TERRAIN_SCALE / 2.0f;
                        state->target_world_z = grid_z * CELL_SIZE + CELL_SIZE / 2.0f;
                    }
                    break;
                }
            }
        }
    }

    // ========== CLICK HANDLING ==========
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && state->target_valid) {
        int grid_x = state->target_grid_x;
        int grid_z = state->target_grid_z;

        if (state->current_tool == TOOL_TREE) {
            // Get terrain height at grid cell center
            int tree_terrain_x = grid_x * 2 + 1;
            int tree_terrain_z = grid_z * 2 + 1;
            if (tree_terrain_x >= TERRAIN_RESOLUTION) tree_terrain_x = TERRAIN_RESOLUTION - 1;
            if (tree_terrain_z >= TERRAIN_RESOLUTION) tree_terrain_z = TERRAIN_RESOLUTION - 1;
            int ground_height = state->terrain_height[tree_terrain_x][tree_terrain_z];

            // Only place tree if there's no water at this location
            if (water_get_depth(&state->water, tree_terrain_x, tree_terrain_z) <= WATER_MIN_DEPTH &&
                game_grow_trees(state)) {
                tree_init(&state->trees[state->tree_count], grid_x, ground_height, grid_z, TREE_SPACE_COLONIZATION);
                state->tree_count++;
            }
        } else if (state->current_tool == TOOL_HEAT) {
            // Add heat to matter cell - 2-3 clicks should ignite vegetation
            matter_add_heat_at(&state->matter,
                              state->target_world_x,
                              state->target_world_z,
                              FLOAT_TO_FIXED(300.0f));
        } else if (state->current_tool == TOOL_WATER) {
            // For water tool, add water to both systems
            water_add_at_world(&state->water,
                              state->target_world_x,
                              state->target_world_z,
                              3.0f);  // Add 3 units of water depth

            // Also add to matter system
            matter_add_water_at(&state->matter,
                               state->target_world_x,
                               state->target_world_z,
                               FLOAT_TO_FIXED(1.0f));
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

    // ========== WATER SIMULATION ==========
    if (!state->paused) {
        water_update(&state->water, delta);

        // NOTE: Water sync from water simulation to matter system is DISABLED
        // The water simulation (water.c) handles fluid dynamics
        // The matter system (matter.c) handles ground, atmosphere, and vegetation
        // They are separate systems - water affects rendering but not matter thermodynamics
        // This prevents energy drift bugs from the coupling
    }

    // ========== TREE SHADOW EFFECTS ==========
    // Update light levels based on tree canopy coverage
    if (!state->paused) {
        static int shadow_update_counter = 0;
        if (++shadow_update_counter >= 30) {  // Every ~0.5 seconds
            shadow_update_counter = 0;

            // Reset all cells to full light
            for (int x = 0; x < MATTER_RES; x++) {
                for (int z = 0; z < MATTER_RES; z++) {
                    state->matter.cells[x][z].light_level = FIXED_ONE;
                }
            }

            // Cast shadows from trees
            for (int t = 0; t < state->tree_count; t++) {
                const Tree *tree = &state->trees[t];
                if (!tree->active) continue;

                // Get tree base position in matter cell coordinates
                float base_world_x = tree->base_x * CELL_SIZE + CELL_SIZE / 2.0f;
                float base_world_z = tree->base_z * CELL_SIZE + CELL_SIZE / 2.0f;
                int base_cx, base_cz;
                matter_world_to_cell(base_world_x, base_world_z, &base_cx, &base_cz);

                // Calculate shadow radius based on leaf count
                int shadow_radius = 2 + tree->leaf_count / 500;
                if (shadow_radius > 10) shadow_radius = 10;

                // Apply shadow to nearby cells
                for (int dx = -shadow_radius; dx <= shadow_radius; dx++) {
                    for (int dz = -shadow_radius; dz <= shadow_radius; dz++) {
                        int cx = base_cx + dx;
                        int cz = base_cz + dz;
                        if (!matter_cell_valid(cx, cz)) continue;

                        // Distance-based shadow intensity
                        float dist = sqrtf((float)(dx * dx + dz * dz));
                        float shadow_factor = 1.0f - (dist / (shadow_radius + 1));
                        if (shadow_factor < 0) shadow_factor = 0;

                        // Reduce light level (more leaves = more shade)
                        fixed16_t shade = FLOAT_TO_FIXED(shadow_factor * 0.7f);
                        fixed16_t current = state->matter.cells[cx][cz].light_level;
                        state->matter.cells[cx][cz].light_level = current - shade;
                        if (state->matter.cells[cx][cz].light_level < FLOAT_TO_FIXED(0.1f)) {
                            state->matter.cells[cx][cz].light_level = FLOAT_TO_FIXED(0.1f);
                        }
                    }
                }
            }
        }
    }

    // ========== TREE IGNITION FROM MATTER HEAT ==========
    // Check if hot matter cells ignite nearby tree voxels
    if (!state->paused) {
        static int tree_heat_counter = 0;
        if (++tree_heat_counter >= 15) {  // Every ~0.25 seconds
            tree_heat_counter = 0;

            for (int t = 0; t < state->tree_count; t++) {
                Tree *tree = &state->trees[t];
                if (!tree->active) continue;

                // Get tree base in matter cell coordinates
                float base_world_x = tree->base_x * CELL_SIZE + CELL_SIZE / 2.0f;
                float base_world_z = tree->base_z * CELL_SIZE + CELL_SIZE / 2.0f;
                int base_cx, base_cz;
                matter_world_to_cell(base_world_x, base_world_z, &base_cx, &base_cz);

                // Check if ground under tree is hot
                const MatterCell *cell = matter_get_cell_const(&state->matter, base_cx, base_cz);
                if (!cell) continue;

                // Tree wood ignition temp is ~573K (300°C)
                fixed16_t ignition_temp = FLOAT_TO_FIXED(500.0f);  // Slightly lower for trees near fire

                if (cell->temperature > ignition_temp) {
                    // Ignite bottom voxels of the tree
                    for (int v = 0; v < tree->voxel_count; v++) {
                        TreeVoxel *voxel = &tree->voxels[v];
                        if (!voxel->active || voxel->burn_state != VOXEL_NORMAL) continue;

                        // Ignite low voxels (near ground)
                        if (voxel->y < 8) {
                            voxel->burn_state = VOXEL_BURNING;
                            voxel->burn_timer = 0.5f;
                        }
                    }
                }
            }
        }
    }

    // ========== TREE VOXEL FIRE SPREAD ==========
    // Spread fire between tree voxels
    if (!state->paused) {
        static int voxel_fire_counter = 0;
        if (++voxel_fire_counter >= 5) {  // Every ~0.08 seconds
            voxel_fire_counter = 0;

            for (int t = 0; t < state->tree_count; t++) {
                Tree *tree = &state->trees[t];
                if (!tree->active) continue;

                for (int v = 0; v < tree->voxel_count; v++) {
                    TreeVoxel *voxel = &tree->voxels[v];
                    if (!voxel->active || voxel->burn_state != VOXEL_BURNING) continue;

                    voxel->burn_timer -= delta;

                    // Spread to neighbors
                    for (int dy = 0; dy <= 2; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            for (int dz = -1; dz <= 1; dz++) {
                                if (dx == 0 && dy == 0 && dz == 0) continue;

                                TreeVoxel *neighbor = tree_get_voxel_at(tree,
                                    voxel->x + dx, voxel->y + dy, voxel->z + dz);

                                if (neighbor && neighbor->active &&
                                    neighbor->burn_state == VOXEL_NORMAL) {
                                    // 30% chance to spread
                                    if ((float)GetRandomValue(0, 100) / 100.0f < 0.3f) {
                                        neighbor->burn_state = VOXEL_BURNING;
                                        neighbor->burn_timer = 0.5f;
                                    }
                                }
                            }
                        }
                    }

                    // Transition when burned out
                    if (voxel->burn_timer <= 0) {
                        if (voxel->type == VOXEL_LEAF) {
                            voxel->active = false;
                            tree->leaf_count--;
                        } else {
                            voxel->burn_state = VOXEL_BURNED;
                        }
                    }
                }
            }
        }
    }

    // ========== MATTER SIMULATION (thermodynamic vegetation, fire, nutrients) ==========
    if (!state->paused) {
        matter_update(&state->matter, delta);
    }
}

// ============ CLEANUP ============

void game_cleanup(GameState *state)
{
    // Clean up each tree's resources (attractor octrees, etc.)
    if (state->trees != NULL) {
        for (int i = 0; i < state->tree_count; i++) {
            tree_cleanup(&state->trees[i]);
        }
        free(state->trees);
        state->trees = NULL;
    }
    state->tree_count = 0;
    state->tree_capacity = 0;
}
