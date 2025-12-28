#include "render.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============ COLOR DEFINITIONS ============

// Terrain colors
static const Color GRASS_LOW_COLOR = { 141, 168, 104, 255 };
static const Color GRASS_HIGH_COLOR = { 169, 186, 131, 255 };
static const Color ROCK_COLOR = { 158, 154, 146, 255 };
static const Color UNDERWATER_COLOR = { 120, 110, 90, 255 };
static const Color FIRE_COLOR = { 255, 100, 20, 255 };
static const Color SKY_COLOR = { 135, 206, 235, 255 };
static const Color WATER_COLOR = { 40, 120, 180, 140 };

// Tree colors
static const Color TRUNK_COLOR = { 72, 50, 35, 255 };
static const Color BRANCH_COLOR = { 92, 64, 45, 255 };
static const Color LEAF_COLOR = { 50, 180, 70, 255 };
static const Color BURNING_LEAF_COLOR = { 255, 60, 20, 255 };
static const Color CHARRED_COLOR = { 25, 20, 15, 255 };

// ============ INSTANCING DATA ============

// Color groups for batching
typedef enum {
    GROUP_TRUNK,
    GROUP_BRANCH,
    GROUP_LEAF,
    GROUP_BURNING_WOOD,
    GROUP_BURNING_LEAF,
    GROUP_CHARRED,
    GROUP_TERRAIN_LOW,
    GROUP_TERRAIN_HIGH,
    GROUP_TERRAIN_ROCK,
    GROUP_TERRAIN_UNDERWATER,
    GROUP_TERRAIN_FIRE,
    GROUP_TERRAIN_BURNED,
    GROUP_COUNT
} ColorGroup;

// Max instances per group
#define MAX_INSTANCES (MAX_TREES * MAX_VOXELS_PER_TREE + TERRAIN_RESOLUTION * TERRAIN_RESOLUTION)

// Instance data
static Mesh cubeMesh;
static Shader instancingShader;
static Material cubeMaterials[GROUP_COUNT];
static Matrix *instanceTransforms[GROUP_COUNT];
static int instanceCounts[GROUP_COUNT];
static bool initialized = false;
static bool useInstancing = false;

// ============ INITIALIZATION ============

void render_init(void)
{
    if (initialized) return;

    // Create cube mesh
    cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);

    // Try to load instancing shader
    // GLSL 330 for OpenGL 3.3+ (macOS uses OpenGL 4.1 which supports GLSL 330)
    instancingShader = LoadShader("resources/shaders/instancing.vs",
                                   "resources/shaders/instancing.fs");

    // Check if shader loaded successfully
    if (instancingShader.id > 0) {
        // Set shader locations
        // MVP matrix location (uniform)
        instancingShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(instancingShader, "mvp");

        // Instance transform location (vertex attribute, not uniform!)
        // This is critical: use GetShaderLocationAttrib for vertex attributes
        instancingShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(instancingShader, "instanceTransform");

        if (instancingShader.locs[SHADER_LOC_MATRIX_MVP] != -1 &&
            instancingShader.locs[SHADER_LOC_MATRIX_MODEL] != -1) {
            useInstancing = true;
            TraceLog(LOG_INFO, "RENDER: Instancing shader loaded successfully");
            TraceLog(LOG_INFO, "RENDER: MVP location: %d, instanceTransform location: %d",
                     instancingShader.locs[SHADER_LOC_MATRIX_MVP],
                     instancingShader.locs[SHADER_LOC_MATRIX_MODEL]);
        } else {
            TraceLog(LOG_WARNING, "RENDER: Shader locations not found, falling back to non-instanced rendering");
            UnloadShader(instancingShader);
            useInstancing = false;
        }
    } else {
        TraceLog(LOG_WARNING, "RENDER: Failed to load instancing shader, using fallback");
        useInstancing = false;
    }

    // Create materials for each color group
    Color groupColors[GROUP_COUNT] = {
        TRUNK_COLOR,
        BRANCH_COLOR,
        LEAF_COLOR,
        FIRE_COLOR,
        BURNING_LEAF_COLOR,
        CHARRED_COLOR,
        GRASS_LOW_COLOR,
        GRASS_HIGH_COLOR,
        ROCK_COLOR,
        UNDERWATER_COLOR,
        FIRE_COLOR,
        { 25, 20, 15, 255 }  // Burned terrain
    };

    for (int i = 0; i < GROUP_COUNT; i++) {
        cubeMaterials[i] = LoadMaterialDefault();
        cubeMaterials[i].maps[MATERIAL_MAP_DIFFUSE].color = groupColors[i];

        // Assign instancing shader to material if available
        if (useInstancing) {
            cubeMaterials[i].shader = instancingShader;
        }

        // Allocate transform arrays
        instanceTransforms[i] = (Matrix *)malloc(sizeof(Matrix) * MAX_INSTANCES);
        instanceCounts[i] = 0;
    }

    initialized = true;
    TraceLog(LOG_INFO, "RENDER: Initialized with %s rendering",
             useInstancing ? "INSTANCED" : "FALLBACK");
}

// ============ HELPER FUNCTIONS ============

static inline void add_instance(ColorGroup group, float x, float y, float z, float size) {
    if (instanceCounts[group] >= MAX_INSTANCES) return;

    Matrix *m = &instanceTransforms[group][instanceCounts[group]++];

    // Build scale + translate matrix
    // Raylib Matrix: m0,m5,m10 = diagonal (scale), m12,m13,m14 = translation
    m->m0 = size;  m->m4 = 0;     m->m8 = 0;      m->m12 = x;
    m->m1 = 0;     m->m5 = size;  m->m9 = 0;      m->m13 = y;
    m->m2 = 0;     m->m6 = 0;     m->m10 = size;  m->m14 = z;
    m->m3 = 0;     m->m7 = 0;     m->m11 = 0;     m->m15 = 1;
}

static ColorGroup get_terrain_group(const GameState *state, int x, int z) {
    TerrainBurnState burn = state->terrain_burn[x][z];
    int height = state->terrain_height[x][z];

    if (burn == TERRAIN_BURNING) return GROUP_TERRAIN_FIRE;
    if (burn == TERRAIN_BURNED) return GROUP_TERRAIN_BURNED;
    if (height >= 8) return GROUP_TERRAIN_ROCK;
    if (height >= 5) return GROUP_TERRAIN_HIGH;
    if (height < WATER_LEVEL) return GROUP_TERRAIN_UNDERWATER;
    return GROUP_TERRAIN_LOW;
}

static ColorGroup get_voxel_group(const TreeVoxel *voxel) {
    if (voxel->burn_state == VOXEL_BURNED) return GROUP_CHARRED;
    if (voxel->burn_state == VOXEL_BURNING) {
        return (voxel->type == VOXEL_LEAF) ? GROUP_BURNING_LEAF : GROUP_BURNING_WOOD;
    }
    switch (voxel->type) {
        case VOXEL_TRUNK:  return GROUP_TRUNK;
        case VOXEL_BRANCH: return GROUP_BRANCH;
        default:           return GROUP_LEAF;
    }
}

// ============ MAIN RENDER ============

void render_frame(const GameState *state)
{
    // Reset instance counts
    for (int i = 0; i < GROUP_COUNT; i++) {
        instanceCounts[i] = 0;
    }

    // ========== COLLECT TERRAIN INSTANCES ==========
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            int height = state->terrain_height[x][z];
            float world_x = x * TERRAIN_SCALE;
            float world_y = height * TERRAIN_SCALE;
            float world_z = z * TERRAIN_SCALE;

            ColorGroup group = get_terrain_group(state, x, z);
            add_instance(group, world_x, world_y, world_z, TERRAIN_SCALE);
        }
    }

    // ========== COLLECT TREE VOXEL INSTANCES ==========
    for (int t = 0; t < state->tree_count; t++) {
        const Tree *tree = &state->trees[t];
        if (!tree->active) continue;

        float base_x = tree->base_x * CELL_SIZE + CELL_SIZE / 2.0f;
        float base_y = tree->base_y * TERRAIN_SCALE;
        float base_z = tree->base_z * CELL_SIZE + CELL_SIZE / 2.0f;

        for (int v = 0; v < tree->voxel_count; v++) {
            const TreeVoxel *voxel = &tree->voxels[v];
            if (!voxel->active) continue;

            float px = base_x + voxel->x * BOX_SIZE;
            float py = base_y + voxel->y * BOX_SIZE + BOX_SIZE / 2.0f;
            float pz = base_z + voxel->z * BOX_SIZE;

            ColorGroup group = get_voxel_group(voxel);
            add_instance(group, px, py, pz, BOX_SIZE);
        }
    }

    // ========== DRAW ==========
    BeginDrawing();
    ClearBackground(SKY_COLOR);

    BeginMode3D(state->camera);

    // Draw all groups
    if (useInstancing) {
        // GPU instanced rendering - one draw call per color group
        for (int i = 0; i < GROUP_COUNT; i++) {
            if (instanceCounts[i] > 0) {
                DrawMeshInstanced(cubeMesh, cubeMaterials[i], instanceTransforms[i], instanceCounts[i]);
            }
        }
    } else {
        // Fallback: individual DrawMesh calls
        for (int i = 0; i < GROUP_COUNT; i++) {
            for (int j = 0; j < instanceCounts[i]; j++) {
                DrawMesh(cubeMesh, cubeMaterials[i], instanceTransforms[i][j]);
            }
        }
    }

    // Draw water plane
    float water_y = WATER_LEVEL * TERRAIN_SCALE + 0.3f;
    float terrain_size = TERRAIN_RESOLUTION * TERRAIN_SCALE;
    float water_center = terrain_size / 2.0f;
    DrawPlane(
        (Vector3){ water_center, water_y, water_center },
        (Vector2){ terrain_size, terrain_size },
        WATER_COLOR
    );

    EndMode3D();

    // ========== UI ==========
    DrawRectangle(10, 10, 310, 195, Fade(BLACK, 0.7f));
    DrawText("Tree Growth Simulator", 20, 15, 20, WHITE);

    const char *tool_name = (state->current_tool == TOOL_BURN) ? "BURN" : "TREE";
    Color tool_color = (state->current_tool == TOOL_BURN) ? ORANGE : GREEN;
    DrawText(TextFormat("Tool: %s", tool_name), 200, 17, 16, tool_color);

    DrawText("1 - Burn tool, 2 - Tree tool", 20, 42, 12, LIGHTGRAY);
    DrawText("Left-click - Use tool", 20, 57, 12, LIGHTGRAY);
    DrawText("Right-click + drag - Look around", 20, 72, 12, LIGHTGRAY);
    DrawText("WASD - Move, Q/E - Down/Up, Shift - Sprint", 20, 87, 12, LIGHTGRAY);
    DrawText("Scroll - Zoom, SPACE - Pause, R - Reset", 20, 102, 12, LIGHTGRAY);

    DrawText(TextFormat("Trees: %d", state->tree_count), 20, 125, 14, WHITE);
    DrawText(state->paused ? "PAUSED" : "GROWING...", 120, 125, 14,
             state->paused ? YELLOW : GREEN);

    // Voxel counts
    int total_voxels = 0;
    int trunk_count = 0, branch_count = 0, leaf_count = 0;
    for (int t = 0; t < state->tree_count; t++) {
        total_voxels += state->trees[t].voxel_count;
        trunk_count += state->trees[t].trunk_count;
        branch_count += state->trees[t].branch_count;
        leaf_count += state->trees[t].leaf_count;
    }
    DrawText(TextFormat("Voxels: %d", total_voxels), 20, 145, 14,
             total_voxels > 0 ? WHITE : RED);
    DrawText(TextFormat("Trunk: %d  Branch: %d  Leaf: %d", trunk_count, branch_count, leaf_count),
             20, 165, 11, LIGHTGRAY);

    // Rendering info
    int total_instances = 0;
    for (int i = 0; i < GROUP_COUNT; i++) total_instances += instanceCounts[i];
    DrawText(TextFormat("%s: %d calls (%d instances)",
             useInstancing ? "Instanced" : "Fallback",
             useInstancing ? GROUP_COUNT : total_instances,
             total_instances),
             20, 180, 10, useInstancing ? GREEN : ORANGE);

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
    if (!initialized) return;

    UnloadMesh(cubeMesh);

    if (useInstancing) {
        UnloadShader(instancingShader);
    }

    for (int i = 0; i < GROUP_COUNT; i++) {
        UnloadMaterial(cubeMaterials[i]);
        free(instanceTransforms[i]);
    }

    initialized = false;
}
