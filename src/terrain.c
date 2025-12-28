#include "terrain.h"
#include "water.h"
#include "noise.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Grid settings (needed for tree position conversion)
#define CELL_SIZE 5.0f

static float randf(void) {
    return (float)GetRandomValue(0, 10000) / 10000.0f;
}

TerrainConfig terrain_config_default(uint32_t seed) {
    TerrainConfig config = {
        .seed = seed,
        .octaves = TERRAIN_DEFAULT_OCTAVES,
        .lacunarity = TERRAIN_DEFAULT_LACUNARITY,
        .persistence = TERRAIN_DEFAULT_PERSISTENCE,
        .scale = TERRAIN_DEFAULT_SCALE,
        .height_min = TERRAIN_HEIGHT_MIN,
        .height_max = TERRAIN_HEIGHT_MAX
    };
    return config;
}

void terrain_generate_seeded(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                             const TerrainConfig *config) {
    // Initialize noise with the provided seed
    noise_init(config->seed);

    // Build noise configuration for FBM
    NoiseConfig noise_cfg = {
        .seed = config->seed,
        .octaves = config->octaves,
        .lacunarity = config->lacunarity,
        .persistence = config->persistence,
        .scale = config->scale
    };

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            // Get FBM noise value at this position
            float noise_val = noise_fbm2d((float)x, (float)z, &noise_cfg);

            // Remap from [-1, 1] to [height_min, height_max]
            float normalized = noise_normalize(noise_val);
            int h = config->height_min +
                    (int)(normalized * (float)(config->height_max - config->height_min));

            // Clamp just in case
            if (h < config->height_min) h = config->height_min;
            if (h > config->height_max) h = config->height_max;

            height[x][z] = h;
        }
    }

    TraceLog(LOG_INFO, "Terrain generated with seed %u (octaves=%d, scale=%.3f)",
             config->seed, config->octaves, config->scale);
}

void terrain_generate(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]) {
    // Use time-based seed for variety when no seed is specified
    uint32_t seed = (uint32_t)time(NULL);
    TerrainConfig config = terrain_config_default(seed);
    terrain_generate_seeded(height, &config);
}

void terrain_burn_init(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                       float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]) {
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            burn[x][z] = TERRAIN_NORMAL;
            timers[x][z] = 0;
        }
    }
}

void terrain_burn_update(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         const int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                         const WaterState *water,
                         Tree *trees, int tree_count) {

    (void)height;  // No longer used for water check

    // ========== TERRAIN FIRE SPREAD ==========
    // Only update burning cells, O(burning_cells)
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (burn[x][z] != TERRAIN_BURNING) continue;

            timers[x][z] -= BURN_SPREAD_INTERVAL;

            // Spread to 8 neighbors - O(8)
            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dz == 0) continue;
                    int nx = x + dx;
                    int nz = z + dz;
                    if (nx >= 0 && nx < TERRAIN_RESOLUTION &&
                        nz >= 0 && nz < TERRAIN_RESOLUTION) {
                        // Fire doesn't spread to cells with water
                        if (burn[nx][nz] == TERRAIN_NORMAL &&
                            water_get_depth(water, nx, nz) <= WATER_MIN_DEPTH) {
                            if (randf() < BURN_SPREAD_CHANCE) {
                                burn[nx][nz] = TERRAIN_BURNING;
                                timers[nx][nz] = BURN_DURATION;
                            }
                        }
                    }
                }
            }

            // Transition to burned
            if (timers[x][z] <= 0) {
                burn[x][z] = TERRAIN_BURNED;
            }
        }
    }

    // ========== TREE IGNITION FROM TERRAIN ==========
    // Iterate trees first, check their local terrain - O(trees) instead of O(terrain * trees)
    for (int t = 0; t < tree_count; t++) {
        Tree *tree = &trees[t];
        if (!tree->active) continue;

        // Convert tree position to terrain coordinates
        int tree_terrain_x = (int)(tree->base_x * CELL_SIZE / TERRAIN_SCALE);
        int tree_terrain_z = (int)(tree->base_z * CELL_SIZE / TERRAIN_SCALE);

        // Check nearby terrain cells for fire (small radius)
        bool near_fire = false;
        int check_radius = (int)(BURN_TREE_IGNITE_DISTANCE + 1);
        for (int dx = -check_radius; dx <= check_radius && !near_fire; dx++) {
            for (int dz = -check_radius; dz <= check_radius && !near_fire; dz++) {
                int tx = tree_terrain_x + dx;
                int tz = tree_terrain_z + dz;
                if (tx >= 0 && tx < TERRAIN_RESOLUTION &&
                    tz >= 0 && tz < TERRAIN_RESOLUTION) {
                    if (burn[tx][tz] == TERRAIN_BURNING) {
                        near_fire = true;
                    }
                }
            }
        }

        // If near fire, ignite low voxels
        if (near_fire) {
            for (int v = 0; v < tree->voxel_count; v++) {
                TreeVoxel *voxel = &tree->voxels[v];
                if (!voxel->active || voxel->burn_state != VOXEL_NORMAL) continue;

                if (voxel->y < BURN_TREE_LOW_HEIGHT || randf() < BURN_TREE_RANDOM_CHANCE) {
                    voxel->burn_state = VOXEL_BURNING;
                    voxel->burn_timer = BURN_DURATION;
                }
            }
        }
    }

    // ========== VOXEL FIRE SPREAD (Using spatial hash) ==========
    // O(trees * burning_voxels * 27) instead of O(trees * voxels^2)
    for (int t = 0; t < tree_count; t++) {
        Tree *tree = &trees[t];
        if (!tree->active) continue;

        for (int v = 0; v < tree->voxel_count; v++) {
            TreeVoxel *voxel = &tree->voxels[v];
            if (!voxel->active || voxel->burn_state != VOXEL_BURNING) continue;

            voxel->burn_timer -= BURN_SPREAD_INTERVAL;

            // Spread fire to neighbors using spatial hash - O(27) lookups
            for (int dy = 0; dy <= 2; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        if (dx == 0 && dy == 0 && dz == 0) continue;

                        TreeVoxel *neighbor = tree_get_voxel_at(tree,
                            voxel->x + dx, voxel->y + dy, voxel->z + dz);

                        if (neighbor && neighbor->active &&
                            neighbor->burn_state == VOXEL_NORMAL) {
                            if (randf() < BURN_VOXEL_SPREAD_CHANCE) {
                                neighbor->burn_state = VOXEL_BURNING;
                                neighbor->burn_timer = BURN_DURATION;
                            }
                        }
                    }
                }
            }

            // Transition based on voxel type
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

void terrain_regenerate(TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                        const Tree *trees, int tree_count) {

    for (int t = 0; t < tree_count; t++) {
        const Tree *tree = &trees[t];
        if (!tree->active) continue;

        // Check for healthy leaves
        bool has_healthy_leaves = false;
        for (int v = 0; v < tree->voxel_count; v++) {
            if (tree->voxels[v].active &&
                tree->voxels[v].type == VOXEL_LEAF &&
                tree->voxels[v].burn_state == VOXEL_NORMAL) {
                has_healthy_leaves = true;
                break;
            }
        }

        if (!has_healthy_leaves) continue;

        int tree_terrain_x = (int)(tree->base_x * CELL_SIZE / TERRAIN_SCALE);
        int tree_terrain_z = (int)(tree->base_z * CELL_SIZE / TERRAIN_SCALE);

        for (int dx = -TREE_REGEN_RADIUS; dx <= TREE_REGEN_RADIUS; dx++) {
            for (int dz = -TREE_REGEN_RADIUS; dz <= TREE_REGEN_RADIUS; dz++) {
                int tx = tree_terrain_x + dx;
                int tz = tree_terrain_z + dz;

                if (tx < 0 || tx >= TERRAIN_RESOLUTION ||
                    tz < 0 || tz >= TERRAIN_RESOLUTION) continue;

                if (burn[tx][tz] != TERRAIN_BURNED) continue;

                float dist = sqrtf((float)(dx*dx + dz*dz));
                float regen_chance = REGEN_CHANCE_MAX * (1.0f - dist / (TREE_REGEN_RADIUS + 1));

                if (randf() < regen_chance) {
                    burn[tx][tz] = TERRAIN_NORMAL;
                }
            }
        }
    }
}

int terrain_get_height(const int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                       float world_x, float world_z) {
    int tx = (int)(world_x / TERRAIN_SCALE);
    int tz = (int)(world_z / TERRAIN_SCALE);

    if (tx < 0) tx = 0;
    if (tx >= TERRAIN_RESOLUTION) tx = TERRAIN_RESOLUTION - 1;
    if (tz < 0) tz = 0;
    if (tz >= TERRAIN_RESOLUTION) tz = TERRAIN_RESOLUTION - 1;

    return height[tx][tz];
}
