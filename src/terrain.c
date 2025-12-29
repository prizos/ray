#include "terrain.h"
#include "noise.h"
#include <time.h>

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
