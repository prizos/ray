#ifndef TERRAIN_TUNE_H
#define TERRAIN_TUNE_H

#include "terrain.h"
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// ============ CONSTANTS ============

#define MAX_TUNE_VARIATIONS 64
#define TUNE_DEFAULT_CONFIG "terrain_params.cfg"
#define TUNE_DEFAULT_OUTPUT "terrain_output"

// ============ CONFIGURATION ============

typedef struct {
    // Seed (0 = random)
    uint32_t seed;

    // Noise parameters (center values)
    int octaves_center;
    float lacunarity_center;
    float persistence_center;
    float scale_center;

    // Height mapping
    int height_min;
    int height_max;

    // Which parameters to splay
    bool octaves_splay;
    bool lacunarity_splay;
    bool persistence_splay;
    bool scale_splay;

    // Splay configuration
    int variations_per_param;  // Usually 4 (±small, ±large)
    float splay_small;         // e.g., 0.20 for ±20%
    float splay_large;         // e.g., 0.50 for ±50%

    // Output configuration
    char output_dir[256];
    int image_width;
    int image_height;
    bool export_heightmap;     // Grayscale PNG
    bool export_colored;       // Colored visualization PNG
} TuneConfig;

// ============ VARIATION ============

typedef struct {
    TerrainConfig terrain;     // The terrain config to use
    char label[64];            // e.g., "CENTER", "oct+50", "scale-20"
} TerrainVariation;

// ============ FUNCTIONS ============

// Get default configuration
TuneConfig tune_config_default(void);

// Load configuration from file
// Returns true on success, false on error (uses defaults on error)
bool tune_config_load(const char *path, TuneConfig *config);

// Save template configuration file with comments
bool tune_config_save_template(const char *path);

// Generate terrain variations based on config
// Returns number of variations generated
// variations array must have space for MAX_TUNE_VARIATIONS
int tune_generate_variations(const TuneConfig *config, TerrainVariation *variations);

// Convert height array to grayscale image
// Caller must call UnloadImage() when done
Image tune_terrain_to_grayscale(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                                int img_width, int img_height);

// Convert height array to colored image using game's terrain colors
// Caller must call UnloadImage() when done
Image tune_terrain_to_colored(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                              int img_width, int img_height);

// Generate filename for a variation
// buffer must be at least 256 bytes
void tune_make_filename(char *buffer, size_t size,
                        const TerrainVariation *var,
                        const char *suffix);  // "gray" or "color"

// Write INDEX.txt summary file
bool tune_write_index(const char *output_dir,
                      const TuneConfig *config,
                      const TerrainVariation *variations,
                      int variation_count);

#endif // TERRAIN_TUNE_H
