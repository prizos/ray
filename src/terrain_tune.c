#include "terrain_tune.h"
#include "noise.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

// ============ COLOR DEFINITIONS (from render.c) ============

static const Color GRASS_LOW_COLOR = { 141, 168, 104, 255 };
static const Color GRASS_HIGH_COLOR = { 169, 186, 131, 255 };
static const Color ROCK_COLOR = { 158, 154, 146, 255 };
static const Color UNDERWATER_COLOR = { 100, 140, 180, 255 };
static const Color WATER_COLOR = { 60, 120, 180, 255 };

// ============ DEFAULT CONFIGURATION ============

TuneConfig tune_config_default(void)
{
    TuneConfig config = {
        .seed = 0,
        .octaves_center = 6,
        .lacunarity_center = 2.0f,
        .persistence_center = 0.5f,
        .scale_center = 0.025f,
        .height_min = 0,
        .height_max = 12,
        .octaves_splay = true,
        .lacunarity_splay = true,
        .persistence_splay = true,
        .scale_splay = true,
        .variations_per_param = 4,
        .splay_small = 0.20f,
        .splay_large = 0.50f,
        .image_width = 512,
        .image_height = 512,
        .export_heightmap = true,
        .export_colored = true
    };
    strncpy(config.output_dir, TUNE_DEFAULT_OUTPUT, sizeof(config.output_dir) - 1);
    return config;
}

// ============ CONFIG FILE PARSING ============

static char *trim(char *str)
{
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;

    return str;
}

static bool parse_bool(const char *value)
{
    return (strcmp(value, "true") == 0 ||
            strcmp(value, "1") == 0 ||
            strcmp(value, "yes") == 0);
}

bool tune_config_load(const char *path, TuneConfig *config)
{
    *config = tune_config_default();

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Config file '%s' not found, using defaults\n", path);
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim(line);

        // Skip comments and empty lines
        if (trimmed[0] == '#' || trimmed[0] == '\0') continue;

        // Parse key = value
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = trim(trimmed);
        char *value = trim(eq + 1);

        // Match keys
        if (strcmp(key, "seed") == 0) {
            config->seed = (uint32_t)strtoul(value, NULL, 10);
        } else if (strcmp(key, "octaves_center") == 0) {
            config->octaves_center = atoi(value);
        } else if (strcmp(key, "lacunarity_center") == 0) {
            config->lacunarity_center = (float)atof(value);
        } else if (strcmp(key, "persistence_center") == 0) {
            config->persistence_center = (float)atof(value);
        } else if (strcmp(key, "scale_center") == 0) {
            config->scale_center = (float)atof(value);
        } else if (strcmp(key, "height_min") == 0) {
            config->height_min = atoi(value);
        } else if (strcmp(key, "height_max") == 0) {
            config->height_max = atoi(value);
        } else if (strcmp(key, "octaves_splay") == 0) {
            config->octaves_splay = parse_bool(value);
        } else if (strcmp(key, "lacunarity_splay") == 0) {
            config->lacunarity_splay = parse_bool(value);
        } else if (strcmp(key, "persistence_splay") == 0) {
            config->persistence_splay = parse_bool(value);
        } else if (strcmp(key, "scale_splay") == 0) {
            config->scale_splay = parse_bool(value);
        } else if (strcmp(key, "variations_per_param") == 0) {
            config->variations_per_param = atoi(value);
        } else if (strcmp(key, "splay_small") == 0) {
            config->splay_small = (float)atof(value);
        } else if (strcmp(key, "splay_large") == 0) {
            config->splay_large = (float)atof(value);
        } else if (strcmp(key, "output_dir") == 0) {
            strncpy(config->output_dir, value, sizeof(config->output_dir) - 1);
        } else if (strcmp(key, "image_width") == 0) {
            config->image_width = atoi(value);
        } else if (strcmp(key, "image_height") == 0) {
            config->image_height = atoi(value);
        } else if (strcmp(key, "export_heightmap") == 0) {
            config->export_heightmap = parse_bool(value);
        } else if (strcmp(key, "export_colored") == 0) {
            config->export_colored = parse_bool(value);
        }
    }

    fclose(f);
    return true;
}

bool tune_config_save_template(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("Error: Cannot create config file '%s'\n", path);
        return false;
    }

    fprintf(f,
"# Terrain Parameter Configuration\n"
"# ================================\n"
"# Edit this file to tune terrain generation parameters.\n"
"# Run `make tune-terrain` to generate preview images.\n"
"#\n"
"# WORKFLOW:\n"
"#   1. Edit center values below\n"
"#   2. Run: make tune-terrain\n"
"#   3. View images in terrain_output/\n"
"#   4. Pick the best looking terrain\n"
"#   5. Update center values based on your choice\n"
"#   6. Repeat until satisfied\n"
"\n"
"# ============ SEED ============\n"
"# Random seed for reproducibility.\n"
"# Use 0 for random seed, or a specific number to reproduce terrain.\n"
"seed = 0\n"
"\n"
"# ============ NOISE PARAMETERS ============\n"
"# Each parameter has a _center value and _splay toggle.\n"
"# When splay is true, variations will be generated around the center.\n"
"\n"
"# OCTAVES (range: 1-8, default: 6)\n"
"# Number of noise layers stacked together.\n"
"# Lower (1-3): Smooth, rolling hills\n"
"# Higher (6-8): More detailed, rough terrain\n"
"octaves_center = 6\n"
"octaves_splay = true\n"
"\n"
"# LACUNARITY (range: 1.5-3.0, default: 2.0)\n"
"# Frequency multiplier between octaves.\n"
"# Controls how quickly detail increases at smaller scales.\n"
"# Higher values: sharper distinction between feature sizes\n"
"lacunarity_center = 2.0\n"
"lacunarity_splay = true\n"
"\n"
"# PERSISTENCE (range: 0.2-0.8, default: 0.5)\n"
"# Amplitude multiplier between octaves.\n"
"# Controls how much fine detail shows through.\n"
"# Lower (0.2-0.4): Smoother, dominated by large features\n"
"# Higher (0.6-0.8): Rougher, more visible fine detail\n"
"persistence_center = 0.5\n"
"persistence_splay = true\n"
"\n"
"# SCALE (range: 0.01-0.1, default: 0.025)\n"
"# Base noise frequency - controls overall feature size.\n"
"# Lower (0.01-0.02): Large mountains/valleys\n"
"# Higher (0.05-0.1): Smaller, more frequent hills\n"
"scale_center = 0.025\n"
"scale_splay = true\n"
"\n"
"# ============ HEIGHT MAPPING ============\n"
"# These control the output height range in voxels.\n"
"height_min = 0\n"
"height_max = 12\n"
"\n"
"# ============ SPLAY CONFIGURATION ============\n"
"# Controls how variations are generated.\n"
"\n"
"# Number of variations per parameter (usually 4: -large, -small, +small, +large)\n"
"variations_per_param = 4\n"
"\n"
"# Splay amounts as fractions of center value\n"
"splay_small = 0.20\n"
"splay_large = 0.50\n"
"\n"
"# ============ OUTPUT ============\n"
"output_dir = terrain_output\n"
"image_width = 512\n"
"image_height = 512\n"
"\n"
"# Export grayscale heightmap (height -> brightness)\n"
"export_heightmap = true\n"
"\n"
"# Export colored visualization (uses game terrain colors)\n"
"export_colored = true\n"
    );

    fclose(f);
    printf("Created config template: %s\n", path);
    return true;
}

// ============ IMAGE GENERATION ============

Image tune_terrain_to_grayscale(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                                int img_width, int img_height)
{
    Image img = GenImageColor(img_width, img_height, BLACK);

    float scale_x = (float)TERRAIN_RESOLUTION / img_width;
    float scale_y = (float)TERRAIN_RESOLUTION / img_height;

    for (int py = 0; py < img_height; py++) {
        for (int px = 0; px < img_width; px++) {
            int tx = (int)(px * scale_x);
            int ty = (int)(py * scale_y);
            if (tx >= TERRAIN_RESOLUTION) tx = TERRAIN_RESOLUTION - 1;
            if (ty >= TERRAIN_RESOLUTION) ty = TERRAIN_RESOLUTION - 1;

            int h = height[tx][ty];
            // Map height to 0-255 (assuming height_max around 12)
            unsigned char gray = (unsigned char)(h * 255 / 12);
            if (gray > 255) gray = 255;

            Color pixel = { gray, gray, gray, 255 };
            ImageDrawPixel(&img, px, py, pixel);
        }
    }

    return img;
}

Image tune_terrain_to_colored(int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                              int img_width, int img_height)
{
    Image img = GenImageColor(img_width, img_height, BLACK);

    float scale_x = (float)TERRAIN_RESOLUTION / img_width;
    float scale_y = (float)TERRAIN_RESOLUTION / img_height;

    for (int py = 0; py < img_height; py++) {
        for (int px = 0; px < img_width; px++) {
            int tx = (int)(px * scale_x);
            int ty = (int)(py * scale_y);
            if (tx >= TERRAIN_RESOLUTION) tx = TERRAIN_RESOLUTION - 1;
            if (ty >= TERRAIN_RESOLUTION) ty = TERRAIN_RESOLUTION - 1;

            int h = height[tx][ty];

            Color pixel;
            if (h < WATER_LEVEL) {
                // Underwater - blend based on depth
                if (h <= 1) {
                    pixel = WATER_COLOR;
                } else {
                    pixel = UNDERWATER_COLOR;
                }
            } else if (h < 5) {
                pixel = GRASS_LOW_COLOR;
            } else if (h < 8) {
                pixel = GRASS_HIGH_COLOR;
            } else {
                pixel = ROCK_COLOR;
            }

            ImageDrawPixel(&img, px, py, pixel);
        }
    }

    return img;
}

// ============ VARIATION GENERATION ============

static float clamp_float(float val, float min, float max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

static int clamp_int(int val, int min, int max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

int tune_generate_variations(const TuneConfig *config, TerrainVariation *variations)
{
    int count = 0;

    // Use provided seed or generate random one
    uint32_t seed = config->seed;
    if (seed == 0) {
        seed = (uint32_t)time(NULL);
    }

    // Base terrain config from center values
    TerrainConfig base = {
        .seed = seed,
        .octaves = config->octaves_center,
        .lacunarity = config->lacunarity_center,
        .persistence = config->persistence_center,
        .scale = config->scale_center,
        .height_min = config->height_min,
        .height_max = config->height_max
    };

    // First: CENTER variation
    variations[count].terrain = base;
    snprintf(variations[count].label, sizeof(variations[count].label), "CENTER");
    count++;

    // Splay multipliers: -large, -small, +small, +large
    float multipliers[4] = {
        1.0f - config->splay_large,
        1.0f - config->splay_small,
        1.0f + config->splay_small,
        1.0f + config->splay_large
    };
    const char *splay_labels[4] = { "-50", "-20", "+20", "+50" };

    // Octaves splay
    if (config->octaves_splay && count < MAX_TUNE_VARIATIONS - 4) {
        for (int i = 0; i < 4; i++) {
            variations[count].terrain = base;
            int val = (int)(config->octaves_center * multipliers[i] + 0.5f);
            variations[count].terrain.octaves = clamp_int(val, 1, 8);
            snprintf(variations[count].label, sizeof(variations[count].label),
                     "oct%s", splay_labels[i]);
            count++;
        }
    }

    // Lacunarity splay
    if (config->lacunarity_splay && count < MAX_TUNE_VARIATIONS - 4) {
        for (int i = 0; i < 4; i++) {
            variations[count].terrain = base;
            float val = config->lacunarity_center * multipliers[i];
            variations[count].terrain.lacunarity = clamp_float(val, 1.5f, 3.0f);
            snprintf(variations[count].label, sizeof(variations[count].label),
                     "lac%s", splay_labels[i]);
            count++;
        }
    }

    // Persistence splay
    if (config->persistence_splay && count < MAX_TUNE_VARIATIONS - 4) {
        for (int i = 0; i < 4; i++) {
            variations[count].terrain = base;
            float val = config->persistence_center * multipliers[i];
            variations[count].terrain.persistence = clamp_float(val, 0.2f, 0.8f);
            snprintf(variations[count].label, sizeof(variations[count].label),
                     "per%s", splay_labels[i]);
            count++;
        }
    }

    // Scale splay
    if (config->scale_splay && count < MAX_TUNE_VARIATIONS - 4) {
        for (int i = 0; i < 4; i++) {
            variations[count].terrain = base;
            float val = config->scale_center * multipliers[i];
            variations[count].terrain.scale = clamp_float(val, 0.005f, 0.15f);
            snprintf(variations[count].label, sizeof(variations[count].label),
                     "scl%s", splay_labels[i]);
            count++;
        }
    }

    return count;
}

// ============ FILENAME GENERATION ============

void tune_make_filename(char *buffer, size_t size,
                        const TerrainVariation *var,
                        const char *suffix)
{
    snprintf(buffer, size,
             "terrain_s%u_o%d_l%.2f_p%.2f_sc%.3f_%s_%s.png",
             var->terrain.seed,
             var->terrain.octaves,
             var->terrain.lacunarity,
             var->terrain.persistence,
             var->terrain.scale,
             var->label,
             suffix);
}

// ============ INDEX FILE ============

bool tune_write_index(const char *output_dir,
                      const TuneConfig *config,
                      const TerrainVariation *variations,
                      int variation_count)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/INDEX.txt", output_dir);

    FILE *f = fopen(path, "w");
    if (!f) return false;

    time_t now = time(NULL);
    fprintf(f, "Terrain Tuning Output\n");
    fprintf(f, "=====================\n");
    fprintf(f, "Generated: %s\n", ctime(&now));

    fprintf(f, "Center Parameters:\n");
    fprintf(f, "  Seed: %u%s\n", variations[0].terrain.seed,
            config->seed == 0 ? " (random)" : "");
    fprintf(f, "  Octaves: %d\n", config->octaves_center);
    fprintf(f, "  Lacunarity: %.2f\n", config->lacunarity_center);
    fprintf(f, "  Persistence: %.2f\n", config->persistence_center);
    fprintf(f, "  Scale: %.3f\n", config->scale_center);
    fprintf(f, "  Height Range: %d-%d\n", config->height_min, config->height_max);
    fprintf(f, "\n");

    fprintf(f, "Splay Configuration:\n");
    fprintf(f, "  Small: +/-%.0f%%\n", config->splay_small * 100);
    fprintf(f, "  Large: +/-%.0f%%\n", config->splay_large * 100);
    fprintf(f, "\n");

    fprintf(f, "Generated Variations (%d total):\n", variation_count);
    fprintf(f, "--------------------------------\n");

    for (int i = 0; i < variation_count; i++) {
        const TerrainVariation *v = &variations[i];
        fprintf(f, "  [%s]\n", v->label);
        fprintf(f, "    octaves=%d, lacunarity=%.2f, persistence=%.2f, scale=%.3f\n",
                v->terrain.octaves, v->terrain.lacunarity,
                v->terrain.persistence, v->terrain.scale);

        char fname[256];
        if (config->export_heightmap) {
            tune_make_filename(fname, sizeof(fname), v, "gray");
            fprintf(f, "    -> %s\n", fname);
        }
        if (config->export_colored) {
            tune_make_filename(fname, sizeof(fname), v, "color");
            fprintf(f, "    -> %s\n", fname);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    return true;
}

// ============ MAIN ============

static void print_usage(const char *program)
{
    printf("Usage: %s [OPTIONS]\n\n", program);
    printf("Terrain Parameter Tuning Tool\n");
    printf("Generates terrain preview images for parameter evaluation.\n\n");
    printf("OPTIONS:\n");
    printf("  -c, --config FILE     Config file (default: %s)\n", TUNE_DEFAULT_CONFIG);
    printf("  -o, --output DIR      Override output directory\n");
    printf("  -s, --seed N          Override seed value\n");
    printf("  --create-template     Create template config file and exit\n");
    printf("  --single              Generate only center values (no splay)\n");
    printf("  --dry-run             Show what would be generated\n");
    printf("  -v, --verbose         Verbose output\n");
    printf("  -h, --help            Show this help\n\n");
    printf("WORKFLOW:\n");
    printf("  1. Run: %s --create-template\n", program);
    printf("  2. Edit terrain_params.cfg\n");
    printf("  3. Run: %s\n", program);
    printf("  4. View images in %s/\n", TUNE_DEFAULT_OUTPUT);
    printf("  5. Update config with best values, repeat\n");
}

int main(int argc, char *argv[])
{
    const char *config_path = TUNE_DEFAULT_CONFIG;
    const char *output_override = NULL;
    uint32_t seed_override = 0;
    bool seed_set = false;
    bool create_template = false;
    bool single_only = false;
    bool dry_run = false;
    bool verbose = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) config_path = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) output_override = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seed") == 0) {
            if (i + 1 < argc) {
                seed_override = (uint32_t)strtoul(argv[++i], NULL, 10);
                seed_set = true;
            }
        } else if (strcmp(argv[i], "--create-template") == 0) {
            create_template = true;
        } else if (strcmp(argv[i], "--single") == 0) {
            single_only = true;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        }
    }

    // Create template if requested
    if (create_template) {
        return tune_config_save_template(config_path) ? 0 : 1;
    }

    // Load configuration
    TuneConfig config;
    tune_config_load(config_path, &config);

    // Apply overrides
    if (output_override) {
        strncpy(config.output_dir, output_override, sizeof(config.output_dir) - 1);
    }
    if (seed_set) {
        config.seed = seed_override;
    }
    if (single_only) {
        config.octaves_splay = false;
        config.lacunarity_splay = false;
        config.persistence_splay = false;
        config.scale_splay = false;
    }

    // Generate variations
    TerrainVariation variations[MAX_TUNE_VARIATIONS];
    int variation_count = tune_generate_variations(&config, variations);

    printf("Terrain Tuning Tool\n");
    printf("===================\n");
    printf("Config: %s\n", config_path);
    printf("Output: %s/\n", config.output_dir);
    printf("Seed: %u\n", variations[0].terrain.seed);
    printf("Variations: %d\n", variation_count);
    printf("Image size: %dx%d\n", config.image_width, config.image_height);
    printf("\n");

    if (dry_run) {
        printf("DRY RUN - would generate:\n");
        for (int i = 0; i < variation_count; i++) {
            char fname[256];
            if (config.export_heightmap) {
                tune_make_filename(fname, sizeof(fname), &variations[i], "gray");
                printf("  %s\n", fname);
            }
            if (config.export_colored) {
                tune_make_filename(fname, sizeof(fname), &variations[i], "color");
                printf("  %s\n", fname);
            }
        }
        return 0;
    }

    // Create output directory
    mkdir(config.output_dir, 0755);

    // Generate images
    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    for (int i = 0; i < variation_count; i++) {
        TerrainVariation *var = &variations[i];

        if (verbose) {
            printf("[%d/%d] %s: oct=%d lac=%.2f per=%.2f scl=%.3f\n",
                   i + 1, variation_count, var->label,
                   var->terrain.octaves, var->terrain.lacunarity,
                   var->terrain.persistence, var->terrain.scale);
        } else {
            printf("Generating %s...\n", var->label);
        }

        // Generate terrain
        terrain_generate_seeded(height, &var->terrain);

        // Export heightmap
        if (config.export_heightmap) {
            Image img = tune_terrain_to_grayscale(height, config.image_width, config.image_height);
            char fname[256], path[512];
            tune_make_filename(fname, sizeof(fname), var, "gray");
            snprintf(path, sizeof(path), "%s/%s", config.output_dir, fname);
            ExportImage(img, path);
            UnloadImage(img);
        }

        // Export colored
        if (config.export_colored) {
            Image img = tune_terrain_to_colored(height, config.image_width, config.image_height);
            char fname[256], path[512];
            tune_make_filename(fname, sizeof(fname), var, "color");
            snprintf(path, sizeof(path), "%s/%s", config.output_dir, fname);
            ExportImage(img, path);
            UnloadImage(img);
        }
    }

    // Write index file
    tune_write_index(config.output_dir, &config, variations, variation_count);

    int image_count = variation_count * ((config.export_heightmap ? 1 : 0) + (config.export_colored ? 1 : 0));
    printf("\nDone! Generated %d images in %s/\n", image_count, config.output_dir);
    printf("See %s/INDEX.txt for details\n", config.output_dir);

    return 0;
}
