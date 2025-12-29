#include "render.h"
#include "matter.h"
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

    // ========== COLLECT WATER INSTANCES ==========
    // Water is now in the unified matter system
    if (state->matter.initialized) {
        const float DEPTH_THRESHOLD = 0.05f;  // Minimum depth to render
        const float DEEP_THRESHOLD = 2.0f;    // Depth at which water is "deep"

        for (int x = 0; x < MATTER_RES; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                const MatterCell *cell = &state->matter.cells[x][z];
                float depth = FIXED_TO_FLOAT(CELL_H2O_LIQUID(cell));
                if (depth < DEPTH_THRESHOLD) continue;

                // World position - must match terrain positioning exactly
                // Terrain uses: world_x = x * TERRAIN_SCALE (cube centered there)
                float world_x = x * TERRAIN_SCALE;
                float world_z = z * TERRAIN_SCALE;

                // Terrain height in voxel units
                int terrain_h = cell->terrain_height;

                // Terrain cube is centered at (terrain_h * TERRAIN_SCALE)
                // Its TOP is at (terrain_h * TERRAIN_SCALE) + TERRAIN_SCALE/2
                float terrain_top = (terrain_h * TERRAIN_SCALE) + (TERRAIN_SCALE / 2.0f);

                // Water height in world units
                float water_height = depth * TERRAIN_SCALE;
                if (water_height < 0.5f) water_height = 0.5f;  // Minimum visible height
                if (water_height > 50.0f) water_height = 50.0f;

                // Water cube center: sits on top of terrain
                float water_y = terrain_top + (water_height / 2.0f);

                // Choose shallow or deep based on depth
                ColorGroup group = (depth > DEEP_THRESHOLD) ? GROUP_WATER_DEEP : GROUP_WATER_SHALLOW;

                // Add water cube - use TERRAIN_SCALE to match terrain cube size exactly
                add_instance_scaled(group, world_x, water_y, world_z,
                                   TERRAIN_SCALE, water_height, TERRAIN_SCALE);
            }
        }
    }

    // ========== COLLECT MATTER VEGETATION INSTANCES ==========
    // Now uses physics-based substances (SUBST_CELLULOSE, SUBST_ASH, etc.)
    if (state->matter.initialized) {
        const float MIN_DENSITY = 0.05f;  // Minimum density to render
        const float VEG_HEIGHT_BASE = 0.3f;  // Base vegetation height
        const float VEG_HEIGHT_SCALE = 2.0f; // Height multiplier based on density

        for (int x = 0; x < MATTER_RES; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                const MatterCell *cell = &state->matter.cells[x][z];

                // World position
                float world_x = x * MATTER_CELL_SIZE;
                float world_z = z * MATTER_CELL_SIZE;
                float terrain_top = (cell->terrain_height * TERRAIN_SCALE) + (TERRAIN_SCALE / 2.0f);

                // Check if cell is burning (cellulose + O2 + high temp)
                bool is_burning = cell_can_combust(cell, SUBST_CELLULOSE);

                // Render cellulose (vegetation/biomass)
                float cellulose = FIXED_TO_FLOAT(cell->cellulose_solid);
                if (cellulose > MIN_DENSITY) {
                    float height = VEG_HEIGHT_BASE + cellulose * VEG_HEIGHT_SCALE;
                    float y = terrain_top + height / 2.0f;
                    ColorGroup grp = is_burning ? GROUP_BURNING_MATTER : GROUP_CELLULOSE;
                    add_instance_scaled(grp, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.8f, height, MATTER_CELL_SIZE * 0.8f);
                }

                // Render ash
                float ash = FIXED_TO_FLOAT(cell->ash_solid);
                if (ash > MIN_DENSITY) {
                    float height = 0.05f + ash * 0.15f;
                    float y = terrain_top + height / 2.0f;
                    add_instance_scaled(GROUP_ASH, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.5f, height, MATTER_CELL_SIZE * 0.5f);
                }

                // ========== RENDER ICE (frozen water) ==========
                float ice = FIXED_TO_FLOAT(CELL_H2O_ICE(cell));
                if (ice > MIN_DENSITY) {
                    float height = 0.1f + ice * 0.5f;
                    float y = terrain_top + height / 2.0f;
                    add_instance_scaled(GROUP_ICE, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.9f, height, MATTER_CELL_SIZE * 0.9f);
                }

                // ========== RENDER LAVA (liquid silicate) ==========
                float lava = FIXED_TO_FLOAT(CELL_SILICATE_LIQUID(cell));
                if (lava > MIN_DENSITY) {
                    float height = 0.2f + lava * 0.8f;
                    float y = terrain_top + height / 2.0f;

                    // Temperature-based coloring
                    float temp = FIXED_TO_FLOAT(cell->temperature);
                    ColorGroup lava_grp;
                    if (temp > 2500.0f) {
                        lava_grp = GROUP_LAVA_HOT;       // Yellow-white hot
                    } else if (temp > 2350.0f) {
                        lava_grp = GROUP_LAVA_COOLING;   // Orange
                    } else {
                        lava_grp = GROUP_LAVA_COLD;      // Dark red
                    }

                    add_instance_scaled(lava_grp, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.95f, height, MATTER_CELL_SIZE * 0.95f);
                }

                // ========== RENDER STEAM (water vapor) ==========
                float steam = FIXED_TO_FLOAT(CELL_H2O_STEAM(cell));
                if (steam > MIN_DENSITY * 2.0f) {  // Higher threshold for steam
                    // Steam floats above the terrain
                    float height = steam * 1.0f;
                    float y = terrain_top + 2.0f + height / 2.0f;
                    add_instance_scaled(GROUP_STEAM, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 1.2f, height, MATTER_CELL_SIZE * 1.2f);
                }

                // ========== RENDER CRYOGENIC LIQUIDS ==========
                // Liquid Nitrogen (very cold environments only)
                float liquid_n2 = FIXED_TO_FLOAT(CELL_N2_LIQUID(cell));
                if (liquid_n2 > MIN_DENSITY) {
                    float height = 0.1f + liquid_n2 * 0.4f;
                    float y = terrain_top + height / 2.0f;
                    add_instance_scaled(GROUP_LIQUID_N2, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.85f, height, MATTER_CELL_SIZE * 0.85f);
                }

                // Liquid Oxygen (very cold environments only)
                float liquid_o2 = FIXED_TO_FLOAT(CELL_O2_LIQUID(cell));
                if (liquid_o2 > MIN_DENSITY) {
                    float height = 0.1f + liquid_o2 * 0.4f;
                    float y = terrain_top + height / 2.0f;
                    add_instance_scaled(GROUP_LIQUID_O2, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.85f, height, MATTER_CELL_SIZE * 0.85f);
                }

                // ========== RENDER FROZEN GASES ==========
                // Solid Nitrogen (extremely cold)
                float solid_n2 = FIXED_TO_FLOAT(CELL_N2_SOLID(cell));
                if (solid_n2 > MIN_DENSITY) {
                    float height = 0.05f + solid_n2 * 0.3f;
                    float y = terrain_top + height / 2.0f;
                    add_instance_scaled(GROUP_SOLID_N2, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.7f, height, MATTER_CELL_SIZE * 0.7f);
                }

                // Solid Oxygen (extremely cold)
                float solid_o2 = FIXED_TO_FLOAT(CELL_O2_SOLID(cell));
                if (solid_o2 > MIN_DENSITY) {
                    float height = 0.05f + solid_o2 * 0.3f;
                    float y = terrain_top + height / 2.0f;
                    add_instance_scaled(GROUP_SOLID_O2, world_x, y, world_z,
                                       MATTER_CELL_SIZE * 0.7f, height, MATTER_CELL_SIZE * 0.7f);
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
    DrawRectangle(10, 10, 200, 85, Fade(BLACK, 0.7f));

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

    // Key bindings (compact)
    DrawText("1-Heat 2-Cool 3-Tree 4-Water", 20, 45, 10, LIGHTGRAY);
    DrawText("LMB-Use  RMB-Look  WASD-Move", 20, 58, 10, LIGHTGRAY);
    DrawText("Q/E-Up/Down  Space-Pause  R-Reset", 20, 71, 10, LIGHTGRAY);

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

    // Water info - calculate total from matter system
    int water_cells = instanceCounts[GROUP_WATER_SHALLOW] + instanceCounts[GROUP_WATER_DEEP];
    float total_water = FIXED_TO_FLOAT(matter_total_mass(&state->matter, SUBST_H2O));
    DrawText(TextFormat("H2O: %.0f units (%d cells)  Beavers: %d",
             total_water, water_cells, state->beaver_count),
             20, SCREEN_HEIGHT - 46, 11, (Color){ 80, 170, 220, 255 });

    // Rendering info
    int total_instances = 0;
    for (int i = 0; i < GROUP_COUNT; i++) total_instances += instanceCounts[i];
    DrawText(TextFormat("Instances: %d  FPS: %d", total_instances, GetFPS()),
             20, SCREEN_HEIGHT - 29, 11, GREEN);

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
