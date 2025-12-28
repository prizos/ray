#ifndef TERRAIN_H
#define TERRAIN_H

#include "raylib.h"
#include "tree.h"
#include "water.h"
#include "noise.h"
#include <stdbool.h>
#include <stdint.h>

// ============ TERRAIN CONSTANTS ============

// Terrain dimensions (doubled for 4x area)
#define TERRAIN_RESOLUTION 160
#define TERRAIN_SCALE 2.5f
#define WATER_LEVEL 3

// Terrain generation defaults
#define TERRAIN_HEIGHT_MIN 0
#define TERRAIN_HEIGHT_MAX 12
#define TERRAIN_DEFAULT_OCTAVES 6
#define TERRAIN_DEFAULT_LACUNARITY 2.0f
#define TERRAIN_DEFAULT_PERSISTENCE 0.25f
#define TERRAIN_DEFAULT_SCALE 0.025f

// Burn system
#define BURN_SPREAD_INTERVAL 0.08f
#define BURN_DURATION 0.5f
#define BURN_SPREAD_CHANCE 0.3f
#define BURN_TREE_IGNITE_DISTANCE 2.0f
#define BURN_TREE_LOW_HEIGHT 15
#define BURN_TREE_RANDOM_CHANCE 0.1f
#define BURN_VOXEL_SPREAD_CHANCE 0.4f

// Regeneration system
#define REGEN_INTERVAL 0.15f
#define TREE_REGEN_RADIUS 8
#define REGEN_CHANCE_MAX 0.3f

// ============ TERRAIN TYPES ============

// Terrain generation configuration
typedef struct {
    uint32_t seed;              // Seed for reproducibility
    int octaves;                // FBM octaves (1-8)
    float lacunarity;           // Frequency multiplier per octave
    float persistence;          // Amplitude multiplier per octave
    float scale;                // Base noise scale
    int height_min;             // Minimum terrain height
    int height_max;             // Maximum terrain height
} TerrainConfig;

typedef enum {
    TERRAIN_NORMAL,
    TERRAIN_BURNING,
    TERRAIN_BURNED
} TerrainBurnState;

// ============ TERRAIN FUNCTIONS ============

// Get default terrain configuration
TerrainConfig terrain_config_default(uint32_t seed);

// Generate terrain height map using Simplex noise with FBM
void terrain_generate(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]);

// Generate terrain with specific configuration (seed-based)
void terrain_generate_seeded(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                             const TerrainConfig *config);

// Initialize terrain burn state
void terrain_burn_init(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                       float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]);

// Update terrain burning and spread fire
// Fire won't spread to cells with water on them
void terrain_burn_update(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         const int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         const WaterState *water,
                         Tree *trees, int tree_count);

// Regenerate burned terrain near healthy trees
void terrain_regenerate(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                        const Tree *trees, int tree_count);

// Get terrain height at world position
int terrain_get_height(const int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                       float world_x, float world_z);

#endif // TERRAIN_H
