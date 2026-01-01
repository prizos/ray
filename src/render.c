#include "render.h"
#include "chunk.h"
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

// Tree colors
static const Color TRUNK_COLOR = { 72, 50, 35, 255 };
static const Color BRANCH_COLOR = { 92, 64, 45, 255 };
static const Color LEAF_COLOR = { 50, 180, 70, 255 };
static const Color BURNING_LEAF_COLOR = { 255, 60, 20, 255 };
static const Color CHARRED_COLOR = { 25, 20, 15, 255 };

// Creature colors - Beaver
static const Color BEAVER_BODY_COLOR = { 130, 82, 45, 255 };   // Warm brown fur
static const Color BEAVER_BELLY_COLOR = { 160, 120, 80, 255 }; // Lighter tan belly
static const Color BEAVER_TAIL_COLOR = { 55, 50, 48, 255 };    // Dark gray scaly tail
static const Color BEAVER_NOSE_COLOR = { 25, 20, 18, 255 };    // Black nose
static const Color BEAVER_TEETH_COLOR = { 255, 245, 220, 255 };// Cream white teeth
static const Color BEAVER_EYE_COLOR = { 15, 12, 10, 255 };     // Dark eyes

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
    GROUP_BEAVER_BODY,
    GROUP_BEAVER_BELLY,
    GROUP_BEAVER_TAIL,
    GROUP_BEAVER_NOSE,
    GROUP_BEAVER_TEETH,
    GROUP_BEAVER_EYE,
    GROUP_WATER_SHALLOW,
    GROUP_WATER_DEEP,
    // Matter-based (physics-based substances)
    GROUP_CELLULOSE,        // Green vegetation (all biomass)
    GROUP_BURNING_MATTER,   // Orange/red fire
    GROUP_ASH,              // Gray residue

    // Phase-based rendering (unified phase system)
    GROUP_ICE,              // Frozen water (white/translucent)
    GROUP_STEAM,            // Water vapor (white mist)
    GROUP_LAVA_HOT,         // Liquid silicate >2500K (yellow-white)
    GROUP_LAVA_COOLING,     // Liquid silicate 2259-2500K (orange)
    GROUP_LAVA_COLD,        // Solidifying lava (dark red)
    GROUP_LIQUID_N2,        // Cryogenic nitrogen (pale blue)
    GROUP_LIQUID_O2,        // Cryogenic oxygen (pale blue)
    GROUP_SOLID_N2,         // Frozen nitrogen (white frost)
    GROUP_SOLID_O2,         // Frozen oxygen (blue frost)

    // Geology-based terrain
    GROUP_TERRAIN_TOPSOIL,  // Brown organic soil
    GROUP_TERRAIN_BEDROCK,  // Dark dense rock

    GROUP_COUNT
} ColorGroup;

// Max instances per group
// Conservative estimate: 100 trees × ~8000 avg voxels + terrain + beavers
// This caps memory usage while supporting most gameplay scenarios
#define MAX_INSTANCES 1000000

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
        { 25, 20, 15, 255 },  // Burned terrain
        BEAVER_BODY_COLOR,
        BEAVER_BELLY_COLOR,
        BEAVER_TAIL_COLOR,
        BEAVER_NOSE_COLOR,
        BEAVER_TEETH_COLOR,
        BEAVER_EYE_COLOR,
        { 80, 170, 220, 160 },  // Water shallow (lighter blue, semi-transparent)
        { 30, 100, 180, 200 },  // Water deep (darker blue, more opaque)
        // Matter-based (physics substances)
        { 86, 141, 58, 255 },   // Cellulose (green vegetation)
        { 255, 120, 30, 255 },  // Burning matter (orange-red)
        { 80, 80, 80, 255 },    // Ash (dark gray)

        // Phase-based rendering
        { 200, 240, 255, 200 }, // Ice (white/translucent blue)
        { 255, 255, 255, 80 },  // Steam (white mist, very transparent)
        { 255, 255, 200, 255 }, // Lava hot (yellow-white, >2500K)
        { 255, 120, 20, 255 },  // Lava cooling (orange, 2259-2500K)
        { 200, 50, 10, 255 },   // Lava cold (dark red, near solidification)
        { 180, 220, 255, 150 }, // Liquid N2 (pale blue, translucent)
        { 180, 200, 255, 150 }, // Liquid O2 (pale blue, translucent)
        { 240, 250, 255, 255 }, // Solid N2 (white frost)
        { 200, 220, 255, 255 }, // Solid O2 (blue frost)

        // Geology-based terrain
        { 101, 67, 33, 255 },   // Topsoil (brown)
        { 64, 64, 64, 255 }     // Bedrock (dark gray)
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
        if (!instanceTransforms[i]) {
            TraceLog(LOG_ERROR, "RENDER: Failed to allocate instance transforms for group %d", i);
            // Clean up already allocated
            for (int j = 0; j < i; j++) {
                free(instanceTransforms[j]);
                instanceTransforms[j] = NULL;
            }
            return;
        }
        instanceCounts[i] = 0;
    }

    initialized = true;
    TraceLog(LOG_INFO, "RENDER: Initialized with %s rendering", useInstancing ? "INSTANCED" : "FALLBACK");
    TraceLog(LOG_INFO, "RENDER: Allocated %zu MB for instance transforms",
             (sizeof(Matrix) * MAX_INSTANCES * GROUP_COUNT) / (1024 * 1024));
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

// Add instance with non-uniform scale (no rotation)
static inline void add_instance_scaled(ColorGroup group, float x, float y, float z,
                                        float sx, float sy, float sz) {
    if (instanceCounts[group] >= MAX_INSTANCES) return;

    Matrix *m = &instanceTransforms[group][instanceCounts[group]++];

    // Build scale + translate matrix
    m->m0 = sx;  m->m4 = 0;   m->m8 = 0;    m->m12 = x;
    m->m1 = 0;   m->m5 = sy;  m->m9 = 0;    m->m13 = y;
    m->m2 = 0;   m->m6 = 0;   m->m10 = sz;  m->m14 = z;
    m->m3 = 0;   m->m7 = 0;   m->m11 = 0;   m->m15 = 1;
}

// Add instance with non-uniform scale and Y-axis rotation
static inline void add_instance_rotated(ColorGroup group, float x, float y, float z,
                                         float sx, float sy, float sz, float angle) {
    if (instanceCounts[group] >= MAX_INSTANCES) return;

    Matrix *m = &instanceTransforms[group][instanceCounts[group]++];

    // Build scale * rotateY * translate matrix
    float c = cosf(angle);
    float s = sinf(angle);

    // Scale then rotate around Y, then translate
    m->m0 = sx * c;   m->m4 = 0;    m->m8 = sx * s;    m->m12 = x;
    m->m1 = 0;        m->m5 = sy;   m->m9 = 0;         m->m13 = y;
    m->m2 = -sz * s;  m->m6 = 0;    m->m10 = sz * c;   m->m14 = z;
    m->m3 = 0;        m->m7 = 0;    m->m11 = 0;        m->m15 = 1;
}

static ColorGroup get_terrain_group(const GameState *state, int x, int z) {
    int height = state->terrain_height[x][z];

    // Terrain base color by height only (fire now comes from matter system)
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
    // Use cell centers (+0.5) to align with physics cell coordinate system.
    // Cell (cx, cy, cz) renders at center: ((cx-offset+0.5)*SIZE, ...)
    // Terrain grid (x, z) maps to cell (x+128, height+128, z+128), so:
    //   world_x = (x + 0.5) * TERRAIN_SCALE gives the cell center
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            int height = state->terrain_height[x][z];
            float world_x = (x + 0.5f) * TERRAIN_SCALE;
            float world_y = (height + 0.5f) * TERRAIN_SCALE;
            float world_z = (z + 0.5f) * TERRAIN_SCALE;

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

    // ========== COLLECT BEAVER INSTANCES ==========
    for (int b = 0; b < MAX_BEAVERS; b++) {
        const Beaver *beaver = &state->beavers[b];
        if (!beaver->active) continue;

        // Calculate facing direction (toward target)
        float dx = beaver->target_x - beaver->x;
        float dz = beaver->target_z - beaver->z;
        float angle = atan2f(dx, dz);  // Angle in XZ plane

        float bx = beaver->x;
        float by = beaver->y;
        float bz = beaver->z;

        // Forward and right vectors
        float fwd_x = sinf(angle);
        float fwd_z = cosf(angle);
        float right_x = cosf(angle);
        float right_z = -sinf(angle);

        // === BODY ===
        // Main body (chunky, rounded look)
        add_instance_rotated(GROUP_BEAVER_BODY, bx, by, bz, 1.8f, 1.1f, 1.3f, angle);
        // Belly (lighter underside, slightly lower)
        add_instance_rotated(GROUP_BEAVER_BELLY, bx, by - 0.35f, bz, 1.4f, 0.5f, 1.0f, angle);
        // Rump (back is fatter)
        add_instance_rotated(GROUP_BEAVER_BODY,
            bx - fwd_x * 0.6f, by + 0.1f, bz - fwd_z * 0.6f,
            1.0f, 1.0f, 1.2f, angle);

        // === HEAD ===
        float head_x = bx + fwd_x * 1.1f;
        float head_y = by + 0.15f;
        float head_z = bz + fwd_z * 1.1f;
        add_instance_rotated(GROUP_BEAVER_BODY, head_x, head_y, head_z, 0.9f, 0.85f, 0.9f, angle);

        // Snout/muzzle (protruding)
        float snout_x = head_x + fwd_x * 0.5f;
        float snout_y = head_y - 0.15f;
        float snout_z = head_z + fwd_z * 0.5f;
        add_instance_rotated(GROUP_BEAVER_BODY, snout_x, snout_y, snout_z, 0.5f, 0.4f, 0.5f, angle);

        // Nose (black, on snout)
        add_instance_rotated(GROUP_BEAVER_NOSE,
            snout_x + fwd_x * 0.3f, snout_y + 0.05f, snout_z + fwd_z * 0.3f,
            0.2f, 0.15f, 0.25f, angle);

        // === BUCK TEETH (iconic!) ===
        float teeth_y = snout_y - 0.2f;
        // Left tooth
        add_instance_rotated(GROUP_BEAVER_TEETH,
            snout_x + fwd_x * 0.2f - right_x * 0.08f, teeth_y,
            snout_z + fwd_z * 0.2f - right_z * 0.08f,
            0.12f, 0.25f, 0.08f, angle);
        // Right tooth
        add_instance_rotated(GROUP_BEAVER_TEETH,
            snout_x + fwd_x * 0.2f + right_x * 0.08f, teeth_y,
            snout_z + fwd_z * 0.2f + right_z * 0.08f,
            0.12f, 0.25f, 0.08f, angle);

        // === EYES ===
        float eye_y = head_y + 0.2f;
        float eye_fwd = 0.35f;
        float eye_side = 0.3f;
        // Left eye
        add_instance_rotated(GROUP_BEAVER_EYE,
            head_x + fwd_x * eye_fwd - right_x * eye_side, eye_y,
            head_z + fwd_z * eye_fwd - right_z * eye_side,
            0.12f, 0.12f, 0.12f, angle);
        // Right eye
        add_instance_rotated(GROUP_BEAVER_EYE,
            head_x + fwd_x * eye_fwd + right_x * eye_side, eye_y,
            head_z + fwd_z * eye_fwd + right_z * eye_side,
            0.12f, 0.12f, 0.12f, angle);

        // === EARS (small rounded) ===
        float ear_y = head_y + 0.4f;
        float ear_side = 0.35f;
        // Left ear
        add_instance_rotated(GROUP_BEAVER_BODY,
            head_x - right_x * ear_side, ear_y, head_z - right_z * ear_side,
            0.2f, 0.25f, 0.15f, angle);
        // Right ear
        add_instance_rotated(GROUP_BEAVER_BODY,
            head_x + right_x * ear_side, ear_y, head_z + right_z * ear_side,
            0.2f, 0.25f, 0.15f, angle);

        // === TAIL (flat paddle, iconic beaver tail) ===
        float tail_x = bx - fwd_x * 1.8f;
        float tail_y = by - 0.2f;
        float tail_z = bz - fwd_z * 1.8f;
        add_instance_rotated(GROUP_BEAVER_TAIL, tail_x, tail_y, tail_z, 1.6f, 0.18f, 0.9f, angle);

        // === LEGS (stubby) ===
        float leg_h = 0.5f;
        float leg_w = 0.35f;
        float leg_y = by - 0.7f;
        float leg_fwd = 0.5f;
        float leg_side = 0.4f;

        // Front-left
        add_instance_rotated(GROUP_BEAVER_BODY,
            bx + fwd_x * leg_fwd - right_x * leg_side, leg_y,
            bz + fwd_z * leg_fwd - right_z * leg_side,
            leg_w, leg_h, leg_w, angle);
        // Front-right
        add_instance_rotated(GROUP_BEAVER_BODY,
            bx + fwd_x * leg_fwd + right_x * leg_side, leg_y,
            bz + fwd_z * leg_fwd + right_z * leg_side,
            leg_w, leg_h, leg_w, angle);
        // Back-left (bigger, beaver has big back feet)
        add_instance_rotated(GROUP_BEAVER_BODY,
            bx - fwd_x * leg_fwd - right_x * leg_side, leg_y,
            bz - fwd_z * leg_fwd - right_z * leg_side,
            leg_w * 1.2f, leg_h, leg_w * 1.3f, angle);
        // Back-right
        add_instance_rotated(GROUP_BEAVER_BODY,
            bx - fwd_x * leg_fwd + right_x * leg_side, leg_y,
            bz - fwd_z * leg_fwd + right_z * leg_side,
            leg_w * 1.2f, leg_h, leg_w * 1.3f, angle);
    }

    // ========== COLLECT WATER/MATTER INSTANCES FROM SVO ==========
    // Sample the SVO around the terrain area and render visible materials
    {
        // Sample area: must match where terrain was placed in world_init_terrain()
        // Terrain grid (tx, tz) with tx from 0 to TERRAIN_RESOLUTION-1 is placed at:
        //   world_x = tx * TERRAIN_SCALE
        //   cx = floor(world_x / VOXEL_CELL_SIZE) + WORLD_SIZE_CELLS/2 = tx + 128
        // So terrain cells span from cx=128 to cx=128+TERRAIN_RESOLUTION-1
        int sample_start_x = SVO_SIZE / 2;  // = 128, where terrain starts
        int sample_start_z = SVO_SIZE / 2;
        int sample_end_x = sample_start_x + TERRAIN_RESOLUTION;
        int sample_end_z = sample_start_z + TERRAIN_RESOLUTION;

        // Y range: from below ground to above (ground level +/- some range)
        int y_min = SVO_GROUND_Y - 5;
        int y_max = SVO_GROUND_Y + 20;

        for (int cx = sample_start_x; cx < sample_end_x; cx++) {
            for (int cz = sample_start_z; cz < sample_end_z; cz++) {
                for (int cy = y_min; cy < y_max; cy++) {
                    // Use non-const access for temperature caching during render
                    Cell3D *cell = svo_get_cell_for_write((MatterSVO*)&state->matter_svo, cx, cy, cz);
                    if (!cell || cell->present == 0) continue;

                    // Check for water
                    MaterialEntry *water = cell3d_find_material(cell, MAT_WATER);
                    if (water && water->state.moles > 0.1) {
                        // Convert cell coords to world coords
                        float wx, wy, wz;
                        svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);

                        // In single-phase model, MAT_WATER is always liquid
                        double water_moles = water->state.moles;
                        ColorGroup water_group = (water_moles > 2.0) ? GROUP_WATER_DEEP : GROUP_WATER_SHALLOW;
                        add_instance(water_group, wx, wy, wz, SVO_CELL_SIZE);
                    }

                    // Check for ice (now a separate material)
                    MaterialEntry *ice = cell3d_find_material(cell, MAT_ICE);
                    if (ice && ice->state.moles > 0.1) {
                        float wx, wy, wz;
                        svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);
                        add_instance(GROUP_ICE, wx, wy, wz, SVO_CELL_SIZE);
                    }

                    // Check for steam (now a separate material)
                    MaterialEntry *steam = cell3d_find_material(cell, MAT_STEAM);
                    if (steam && steam->state.moles > 0.1) {
                        float wx, wy, wz;
                        svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);
                        add_instance(GROUP_STEAM, wx, wy, wz, SVO_CELL_SIZE);
                    }

                    // Check for magma (now a separate material, was MAT_ROCK in liquid phase)
                    MaterialEntry *magma = cell3d_find_material(cell, MAT_MAGMA);
                    if (magma && magma->state.moles > 0.1) {
                        float wx, wy, wz;
                        svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);
                        double temp = material_get_temperature(&magma->state, MAT_MAGMA);
                        ColorGroup lava_group = (temp > 2500) ? GROUP_LAVA_HOT :
                                                (temp > 2000) ? GROUP_LAVA_COOLING : GROUP_LAVA_COLD;
                        add_instance(lava_group, wx, wy, wz, SVO_CELL_SIZE);
                    }
                }
            }
        }
    }

    // ========== DRAW ==========
    BeginDrawing();
    ClearBackground(SKY_COLOR);

    BeginMode3D(state->camera);

    // Helper to check if a group is transparent
    #define IS_TRANSPARENT_GROUP(g) ( \
        (g) == GROUP_WATER_SHALLOW || (g) == GROUP_WATER_DEEP || \
        (g) == GROUP_STEAM || (g) == GROUP_ICE || \
        (g) == GROUP_LIQUID_N2 || (g) == GROUP_LIQUID_O2 \
    )

    // Draw opaque groups first (skip transparent for later)
    if (useInstancing) {
        // GPU instanced rendering - one draw call per color group
        for (int i = 0; i < GROUP_COUNT; i++) {
            // Skip transparent groups for now (draw them last)
            if (IS_TRANSPARENT_GROUP(i)) continue;
            if (instanceCounts[i] > 0) {
                DrawMeshInstanced(cubeMesh, cubeMaterials[i], instanceTransforms[i], instanceCounts[i]);
            }
        }
    } else {
        // Fallback: individual DrawMesh calls
        for (int i = 0; i < GROUP_COUNT; i++) {
            if (IS_TRANSPARENT_GROUP(i)) continue;
            for (int j = 0; j < instanceCounts[i]; j++) {
                DrawMesh(cubeMesh, cubeMaterials[i], instanceTransforms[i][j]);
            }
        }
    }

    // Draw transparent groups last (water, steam, ice, cryogenic liquids)
    // Transparent objects should be rendered after opaque geometry
    ColorGroup transparent_groups[] = {
        GROUP_ICE, GROUP_LIQUID_N2, GROUP_LIQUID_O2,  // Less transparent first
        GROUP_WATER_SHALLOW, GROUP_WATER_DEEP,        // Water
        GROUP_STEAM                                    // Most transparent last
    };
    int num_transparent = sizeof(transparent_groups) / sizeof(transparent_groups[0]);

    if (useInstancing) {
        for (int t = 0; t < num_transparent; t++) {
            ColorGroup grp = transparent_groups[t];
            if (instanceCounts[grp] > 0) {
                DrawMeshInstanced(cubeMesh, cubeMaterials[grp],
                                 instanceTransforms[grp], instanceCounts[grp]);
            }
        }
    } else {
        for (int t = 0; t < num_transparent; t++) {
            ColorGroup grp = transparent_groups[t];
            for (int j = 0; j < instanceCounts[grp]; j++) {
                DrawMesh(cubeMesh, cubeMaterials[grp], instanceTransforms[grp][j]);
            }
        }
    }

    #undef IS_TRANSPARENT_GROUP

    EndMode3D();

    // Draw target indicator in a separate 3D pass (avoids shader conflicts)
    if (state->target_valid) {
        BeginMode3D(state->camera);
        Vector3 target_pos = {
            state->target_world_x,
            state->target_world_y,
            state->target_world_z
        };

        // Draw vertical reference line from terrain to cursor (when offset from terrain)
        float terrain_y = state->target_terrain_y;
        if (fabsf(target_pos.y - terrain_y) > 0.5f) {
            DrawLine3D(
                (Vector3){target_pos.x, terrain_y, target_pos.z},
                (Vector3){target_pos.x, target_pos.y, target_pos.z},
                YELLOW
            );
            // Draw small marker at terrain level
            DrawCubeWires((Vector3){target_pos.x, terrain_y, target_pos.z},
                         CELL_SIZE * 0.5f, 0.5f, CELL_SIZE * 0.5f, YELLOW);
        }

        // Draw wireframe cube at target (size matches one grid cell)
        DrawCubeWires(target_pos, CELL_SIZE, CELL_SIZE, CELL_SIZE, RED);
        // Draw crosshair lines extending up
        DrawLine3D(
            (Vector3){target_pos.x, target_pos.y, target_pos.z},
            (Vector3){target_pos.x, target_pos.y + 20.0f, target_pos.z},
            RED
        );
        EndMode3D();
    }

    // ========== UI: TOP-LEFT PANEL ==========
    DrawRectangle(10, 10, 230, 98, Fade(BLACK, 0.7f));

    // Tool name and color
    const char *tool_name = "TREE";
    Color tool_color = GREEN;
    if (state->current_tool == TOOL_HEAT) {
        tool_name = "HEAT";
        tool_color = ORANGE;
    } else if (state->current_tool == TOOL_COOL) {
        tool_name = "COOL";
        tool_color = (Color){ 100, 180, 255, 255 };
    } else if (state->current_tool == TOOL_WATER) {
        tool_name = "WATER";
        tool_color = (Color){ 80, 170, 220, 255 };
    }

    // Tool display with temperature for heat/cool
    if ((state->current_tool == TOOL_HEAT || state->current_tool == TOOL_COOL) && state->target_valid) {
        float temp = state->target_temperature;
        Color temp_color = temp > 100 ? RED : (temp < 0 ? SKYBLUE : tool_color);
        DrawText(TextFormat("%s: %.0fC", tool_name, temp), 20, 18, 20, temp_color);
    } else {
        DrawText(tool_name, 20, 18, 20, tool_color);
    }

    // Pause indicator
    if (state->paused) {
        DrawText("PAUSED", 130, 22, 14, YELLOW);
    }

    // Height mode indicator
    if (state->target_absolute_mode) {
        DrawText(TextFormat("Height: Y=%.1f", state->target_absolute_y), 20, 45, 10, YELLOW);
    } else {
        DrawText("Height: GROUND", 20, 45, 10, GRAY);
    }

    // Key bindings (compact)
    DrawText("1-Heat 2-Cool 3-Tree 4-Water", 20, 58, 10, LIGHTGRAY);
    DrawText("LMB-Use  RMB-Look  WASD-Move", 20, 71, 10, LIGHTGRAY);
    DrawText("Q/E-Up/Down  [/]-Height  \\-Reset", 20, 84, 10, LIGHTGRAY);

    // ========== UI: BOTTOM-LEFT STATS ==========
    DrawRectangle(10, SCREEN_HEIGHT - 85, 260, 75, Fade(BLACK, 0.7f));

    // Voxel counts
    int total_voxels = 0, trunk_count = 0, branch_count = 0, leaf_count = 0;
    for (int t = 0; t < state->tree_count; t++) {
        total_voxels += state->trees[t].voxel_count;
        trunk_count += state->trees[t].trunk_count;
        branch_count += state->trees[t].branch_count;
        leaf_count += state->trees[t].leaf_count;
    }

    DrawText(TextFormat("Trees: %d  Voxels: %d", state->tree_count, total_voxels),
             20, SCREEN_HEIGHT - 78, 12, WHITE);
    DrawText(TextFormat("  Trunk: %d  Branch: %d  Leaf: %d", trunk_count, branch_count, leaf_count),
             20, SCREEN_HEIGHT - 63, 11, LIGHTGRAY);

    // Water info (SVO-based)
    int water_cells = instanceCounts[GROUP_WATER_SHALLOW] + instanceCounts[GROUP_WATER_DEEP];
    DrawText(TextFormat("Water cells: %d  Beavers: %d",
             water_cells, state->beaver_count),
             20, SCREEN_HEIGHT - 46, 11, (Color){ 80, 170, 220, 255 });

    // Rendering info
    int total_instances = 0;
    for (int i = 0; i < GROUP_COUNT; i++) total_instances += instanceCounts[i];
    DrawText(TextFormat("Instances: %d  FPS: %d", total_instances, GetFPS()),
             20, SCREEN_HEIGHT - 29, 11, GREEN);

    // ========== UI: RIGHT-SIDE MATTER INFO PANEL (SVO) ==========
    if (state->target_valid) {
        int panel_x = SCREEN_WIDTH - 200;
        int panel_y = 10;
        int panel_w = 190;
        int panel_h = 180;

        DrawRectangle(panel_x, panel_y, panel_w, panel_h, Fade(BLACK, 0.8f));

        // Get SVO cell info at cursor
        CellInfo info = svo_get_cell_info((MatterSVO*)&state->matter_svo,
                                          state->target_world_x,
                                          state->target_world_y,
                                          state->target_world_z);

        int y = panel_y + 8;
        int x = panel_x + 10;

        // Header with cell coordinates
        DrawText(TextFormat("CELL [%d,%d,%d]", info.cell_x, info.cell_y, info.cell_z), x, y, 12, WHITE);
        y += 18;

        if (info.valid) {
            // Temperature (convert from Kelvin to Celsius)
            float temp_c = (float)(info.temperature - 273.15);
            Color temp_color = temp_c > 100 ? RED : (temp_c < 0 ? SKYBLUE : WHITE);
            DrawText(TextFormat("Temp: %.1f C", temp_c), x, y, 11, temp_color);
            y += 14;

            // Cursor height info
            if (state->target_absolute_mode) {
                DrawText(TextFormat("Cursor Y: %.1f (abs)", state->target_absolute_y), x, y, 10, YELLOW);
            } else {
                DrawText(TextFormat("Cursor Y: %.1f (ground)", state->target_terrain_y), x, y, 10, GRAY);
            }
            y += 14;

            // Divider
            DrawLine(x, y, x + panel_w - 20, y, DARKGRAY);
            y += 6;

            // Material count
            DrawText(TextFormat("Materials: %d", info.material_count), x, y, 10, WHITE);
            y += 14;

            // Primary material and phase
            if (info.material_count > 0) {
                const char *mat_names[] = {"None", "Air", "Water", "Rock", "Dirt",
                                           "N2", "O2", "CO2", "Steam"};
                const char *phase_names[] = {"Solid", "Liquid", "Gas"};

                const char *mat_name = (info.primary_material < MAT_COUNT) ?
                                       mat_names[info.primary_material] : "Unknown";
                const char *phase_name = phase_names[info.primary_phase];

                // Color based on material
                Color mat_color = WHITE;
                if (info.primary_material == MAT_WATER) mat_color = (Color){ 80, 170, 220, 255 };
                else if (info.primary_material == MAT_ROCK) mat_color = GRAY;
                else if (info.primary_material == MAT_DIRT) mat_color = (Color){ 139, 90, 43, 255 };
                else if (info.primary_material == MAT_AIR) mat_color = LIGHTGRAY;

                DrawText(TextFormat("Primary: %s (%s)", mat_name, phase_name), x, y, 10, mat_color);
                y += 14;
            }

            // SVO depth info (for debugging)
            DrawText(TextFormat("Y level: %d (ground=%d)", info.cell_y, SVO_GROUND_Y), x, y, 10, DARKGRAY);
            y += 14;

        } else {
            DrawText("(outside SVO bounds)", x, y, 10, DARKGRAY);
        }
    }

    EndDrawing();
}

void render_cleanup(void)
{
    if (!initialized) return;

    // Free instance transform arrays first
    for (int i = 0; i < GROUP_COUNT; i++) {
        if (instanceTransforms[i]) {
            free(instanceTransforms[i]);
            instanceTransforms[i] = NULL;
        }
    }

    // Reset material shaders to default before unloading to avoid double-free
    // (all materials share the same instancingShader)
    Shader defaultShader = { 0 };
    for (int i = 0; i < GROUP_COUNT; i++) {
        cubeMaterials[i].shader = defaultShader;
    }

    // Now unload materials (won't try to free the shader since it's reset)
    for (int i = 0; i < GROUP_COUNT; i++) {
        UnloadMaterial(cubeMaterials[i]);
    }

    // Unload the shared shader once
    if (useInstancing) {
        UnloadShader(instancingShader);
    }

    UnloadMesh(cubeMesh);

    initialized = false;
}
