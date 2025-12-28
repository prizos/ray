#include "render.h"

// Terrain colors - muted earth tones
static const Color GRASS_LOW_COLOR = { 141, 168, 104, 255 }; // Sage green (valleys)
static const Color GRASS_HIGH_COLOR = { 169, 186, 131, 255 };// Lighter green (hills)
static const Color ROCK_COLOR = { 158, 154, 146, 255 };      // Grey stone for peaks
static const Color WATER_COLOR = { 72, 145, 188, 220 };      // Bright blue

// Tree part colors - warm browns and vibrant green
static const Color TRUNK_COLOR = { 72, 50, 35, 255 };        // Dark brown
static const Color BRANCH_COLOR = { 92, 64, 45, 255 };       // Medium brown
static const Color LEAF_COLOR = { 50, 180, 70, 255 };        // Vibrant green

void render_init(void)
{
    // Nothing to initialize
}

void render_frame(const GameState *state)
{
    BeginDrawing();
    ClearBackground((Color){ 135, 206, 235, 255 });  // Sky blue

    BeginMode3D(state->camera);

    // Draw voxel terrain with hills and valleys
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            int height = state->terrain_height[x][z];
            float world_x = x * TERRAIN_SCALE;
            float world_z = z * TERRAIN_SCALE;

            // Height-based color for top surface
            Color surface_col;
            if (height >= 8) {
                surface_col = ROCK_COLOR;           // Rocky peaks
            } else if (height >= 5) {
                surface_col = GRASS_HIGH_COLOR;     // Highland grass
            } else {
                surface_col = GRASS_LOW_COLOR;      // Lowland grass
            }

            // Draw only the top terrain voxel for performance
            Vector3 pos = { world_x, height * TERRAIN_SCALE, world_z };
            DrawCube(pos, TERRAIN_SCALE, TERRAIN_SCALE, TERRAIN_SCALE, surface_col);

            // Draw water if below water level
            if (height < WATER_LEVEL) {
                Vector3 water_pos = { world_x, WATER_LEVEL * TERRAIN_SCALE, world_z };
                DrawCube(water_pos, TERRAIN_SCALE, TERRAIN_SCALE * 0.8f, TERRAIN_SCALE, WATER_COLOR);
            }
        }
    }

    // Draw all trees
    for (int t = 0; t < state->tree_count; t++) {
        const Tree *tree = &state->trees[t];
        if (!tree->active) continue;

        float base_x = tree->base_x * CELL_SIZE + CELL_SIZE / 2.0f;
        float base_y = tree->base_y * TERRAIN_SCALE;  // Terrain height offset
        float base_z = tree->base_z * CELL_SIZE + CELL_SIZE / 2.0f;

        // Draw each voxel
        for (int v = 0; v < tree->voxel_count; v++) {
            const TreeVoxel *voxel = &tree->voxels[v];

            Vector3 pos = {
                base_x + voxel->x * BOX_SIZE,
                base_y + voxel->y * BOX_SIZE + BOX_SIZE / 2.0f,
                base_z + voxel->z * BOX_SIZE
            };

            // Color based on voxel type
            Color col;
            switch (voxel->type) {
                case VOXEL_TRUNK:  col = TRUNK_COLOR; break;
                case VOXEL_BRANCH: col = BRANCH_COLOR; break;
                case VOXEL_LEAF:
                default:           col = LEAF_COLOR; break;
            }

            DrawCube(pos, BOX_SIZE, BOX_SIZE, BOX_SIZE, col);
        }
    }

    EndMode3D();

    // UI
    DrawRectangle(10, 10, 300, 160, Fade(BLACK, 0.7f));
    DrawText("Tree Growth Simulator", 20, 15, 20, WHITE);
    DrawText("Right-click + drag - Look around", 20, 42, 12, LIGHTGRAY);
    DrawText("WASD - Move, Q/E - Down/Up, Shift - Sprint", 20, 57, 12, LIGHTGRAY);
    DrawText("Scroll - Zoom, SPACE - Pause, R - Reset", 20, 72, 12, LIGHTGRAY);

    DrawText(TextFormat("Trees: %d", state->tree_count), 20, 95, 14, WHITE);
    DrawText(state->paused ? "PAUSED" : "GROWING...", 120, 95, 14,
             state->paused ? YELLOW : GREEN);

    // Use cached counts
    int total_voxels = 0;
    int trunk_count = 0, branch_count = 0, leaf_count = 0;
    for (int t = 0; t < state->tree_count; t++) {
        total_voxels += state->trees[t].voxel_count;
        trunk_count += state->trees[t].trunk_count;
        branch_count += state->trees[t].branch_count_cached;
        leaf_count += state->trees[t].leaf_count;
    }
    DrawText(TextFormat("Voxels: %d (trees:%d)", total_voxels, state->tree_count), 20, 115, 14,
             total_voxels > 0 ? WHITE : RED);
    DrawText(TextFormat("Trunk: %d  Branch: %d  Leaf: %d", trunk_count, branch_count, leaf_count),
             20, 135, 11, LIGHTGRAY);

    // Legend
    DrawRectangle(SCREEN_WIDTH - 200, 10, 190, 80, Fade(BLACK, 0.7f));
    DrawText("Space Colonization", SCREEN_WIDTH - 190, 15, 14, WHITE);

    DrawText("Parts:", SCREEN_WIDTH - 190, 38, 12, WHITE);
    DrawRectangle(SCREEN_WIDTH - 190, 55, 10, 10, TRUNK_COLOR);
    DrawText("Trunk", SCREEN_WIDTH - 175, 53, 10, LIGHTGRAY);
    DrawRectangle(SCREEN_WIDTH - 130, 55, 10, 10, BRANCH_COLOR);
    DrawText("Branch", SCREEN_WIDTH - 115, 53, 10, LIGHTGRAY);
    DrawRectangle(SCREEN_WIDTH - 65, 55, 10, 10, LEAF_COLOR);
    DrawText("Leaf", SCREEN_WIDTH - 50, 53, 10, LIGHTGRAY);

    DrawText(TextFormat("Max: %dk voxels/tree", MAX_VOXELS_PER_TREE / 1000), SCREEN_WIDTH - 190, 70, 10, GRAY);

    DrawFPS(SCREEN_WIDTH - 100, SCREEN_HEIGHT - 25);

    EndDrawing();
}

void render_cleanup(void)
{
    // Nothing to cleanup
}
