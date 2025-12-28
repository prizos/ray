#ifndef TERRAIN_H
#define TERRAIN_H

#include "raylib.h"
#include "tree.h"
#include <stdbool.h>

// ============ TERRAIN CONSTANTS ============

// Terrain dimensions (doubled for 4x area)
#define TERRAIN_RESOLUTION 160
#define TERRAIN_SCALE 2.5f
#define WATER_LEVEL 3

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

typedef enum {
    TERRAIN_NORMAL,
    TERRAIN_BURNING,
    TERRAIN_BURNED
} TerrainBurnState;

// ============ TERRAIN FUNCTIONS ============

// Generate terrain height map using layered sine waves
void terrain_generate(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]);

// Initialize terrain burn state
void terrain_burn_init(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                       float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]);

// Update terrain burning and spread fire
void terrain_burn_update(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         const int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         Tree *trees, int tree_count);

// Regenerate burned terrain near healthy trees
void terrain_regenerate(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                        const Tree *trees, int tree_count);

// Get terrain height at world position
int terrain_get_height(const int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                       float world_x, float world_z);

#endif // TERRAIN_H
