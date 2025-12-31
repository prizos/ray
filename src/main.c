#include "raylib.h"
#include "game.h"
#include "render.h"
#include "debug_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// ============ BENCHMARK CONFIGURATION ============

typedef struct {
    int num_trees;
    uint32_t seed;           // Terrain seed (0 = random)
    bool benchmark_mode;
    float benchmark_duration;
    char output_file[256];
    bool debug_mode;         // Enable debug metrics output
} Config;

typedef struct {
    float *samples;
    int sample_count;
    int sample_capacity;
    float min_fps;
    float max_fps;
    float sum_fps;
    float start_time;
} BenchmarkStats;

// ============ HELPER FUNCTIONS ============

static void print_usage(const char *program) {
    printf("Usage: %s [options]\n", program);
    printf("Options:\n");
    printf("  -s, --seed N        Terrain seed for reproducible worlds (default: random)\n");
    printf("  -t, --trees N       Number of trees to spawn (default: auto grid)\n");
    printf("  -b, --benchmark     Run in benchmark mode (auto-exit after duration)\n");
    printf("  -d, --duration N    Benchmark duration in seconds (default: 10)\n");
    printf("  -o, --output FILE   Output file for benchmark results\n");
    printf("  -D, --debug         Enable debug metrics output (1/sec)\n");
    printf("  -h, --help          Show this help message\n");
}

static Config parse_args(int argc, char *argv[]) {
    Config config = {
        .num_trees = -1,  // -1 means use default grid pattern
        .seed = 0,        // 0 means random seed
        .benchmark_mode = false,
        .benchmark_duration = 10.0f,
        .output_file = "",
        .debug_mode = false
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--seed") == 0) {
            if (i + 1 < argc) {
                config.seed = (uint32_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--trees") == 0) {
            if (i + 1 < argc) {
                config.num_trees = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--benchmark") == 0) {
            config.benchmark_mode = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--duration") == 0) {
            if (i + 1 < argc) {
                config.benchmark_duration = (float)atof(argv[++i]);
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                strncpy(config.output_file, argv[++i], sizeof(config.output_file) - 1);
            }
        } else if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--debug") == 0) {
            config.debug_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
    }

    // Auto-generate output filename if benchmark mode but no file specified
    if (config.benchmark_mode && config.output_file[0] == '\0') {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        snprintf(config.output_file, sizeof(config.output_file),
                 "benchmark_%04d%02d%02d_%02d%02d%02d_trees%d.txt",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec,
                 config.num_trees > 0 ? config.num_trees : 0);
    }

    return config;
}

static void stats_init(BenchmarkStats *stats) {
    stats->sample_capacity = 10000;
    stats->samples = (float *)malloc(sizeof(float) * stats->sample_capacity);
    stats->sample_count = 0;
    stats->min_fps = 999999.0f;
    stats->max_fps = 0.0f;
    stats->sum_fps = 0.0f;
    stats->start_time = (float)GetTime();
}

static void stats_record(BenchmarkStats *stats, float fps) {
    if (stats->sample_count < stats->sample_capacity) {
        stats->samples[stats->sample_count++] = fps;
    }
    if (fps < stats->min_fps) stats->min_fps = fps;
    if (fps > stats->max_fps) stats->max_fps = fps;
    stats->sum_fps += fps;
}

static void stats_write(BenchmarkStats *stats, const Config *config, const GameState *state) {
    float avg_fps = stats->sample_count > 0 ? stats->sum_fps / stats->sample_count : 0;
    float duration = (float)GetTime() - stats->start_time;

    // Count total voxels
    int total_voxels = 0;
    for (int t = 0; t < state->tree_count; t++) {
        total_voxels += state->trees[t].voxel_count;
    }

    // Calculate percentiles
    float p1 = 0, p5 = 0, p50 = 0, p95 = 0, p99 = 0;
    if (stats->sample_count > 0) {
        // Simple sort for percentiles (bubble sort, fine for small arrays)
        float *sorted = (float *)malloc(sizeof(float) * stats->sample_count);
        memcpy(sorted, stats->samples, sizeof(float) * stats->sample_count);
        for (int i = 0; i < stats->sample_count - 1; i++) {
            for (int j = 0; j < stats->sample_count - i - 1; j++) {
                if (sorted[j] > sorted[j + 1]) {
                    float tmp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = tmp;
                }
            }
        }
        p1 = sorted[(int)(stats->sample_count * 0.01)];
        p5 = sorted[(int)(stats->sample_count * 0.05)];
        p50 = sorted[(int)(stats->sample_count * 0.50)];
        p95 = sorted[(int)(stats->sample_count * 0.95)];
        p99 = sorted[(int)(stats->sample_count * 0.99)];
        free(sorted);
    }

    // Print to console
    printf("\n========== BENCHMARK RESULTS ==========\n");
    printf("Trees: %d\n", state->tree_count);
    printf("Total Voxels: %d\n", total_voxels);
    printf("Duration: %.2f seconds\n", duration);
    printf("Samples: %d\n", stats->sample_count);
    printf("\nFPS Statistics:\n");
    printf("  Min:  %.2f\n", stats->min_fps);
    printf("  Max:  %.2f\n", stats->max_fps);
    printf("  Avg:  %.2f\n", avg_fps);
    printf("  P1:   %.2f\n", p1);
    printf("  P5:   %.2f\n", p5);
    printf("  P50:  %.2f\n", p50);
    printf("  P95:  %.2f\n", p95);
    printf("  P99:  %.2f\n", p99);
    printf("========================================\n");

    // Write to file
    if (config->output_file[0] != '\0') {
        FILE *f = fopen(config->output_file, "w");
        if (f) {
            fprintf(f, "# Benchmark Results\n");
            fprintf(f, "# Generated: %s", ctime(&(time_t){time(NULL)}));
            fprintf(f, "\n");
            fprintf(f, "trees=%d\n", state->tree_count);
            fprintf(f, "total_voxels=%d\n", total_voxels);
            fprintf(f, "duration_seconds=%.2f\n", duration);
            fprintf(f, "sample_count=%d\n", stats->sample_count);
            fprintf(f, "\n");
            fprintf(f, "fps_min=%.2f\n", stats->min_fps);
            fprintf(f, "fps_max=%.2f\n", stats->max_fps);
            fprintf(f, "fps_avg=%.2f\n", avg_fps);
            fprintf(f, "fps_p1=%.2f\n", p1);
            fprintf(f, "fps_p5=%.2f\n", p5);
            fprintf(f, "fps_p50=%.2f\n", p50);
            fprintf(f, "fps_p95=%.2f\n", p95);
            fprintf(f, "fps_p99=%.2f\n", p99);
            fprintf(f, "\n# Raw FPS samples\n");
            for (int i = 0; i < stats->sample_count; i++) {
                fprintf(f, "%.2f\n", stats->samples[i]);
            }
            fclose(f);
            printf("Results written to: %s\n", config->output_file);
        }
    }
}

static void stats_cleanup(BenchmarkStats *stats) {
    if (stats->samples) {
        free(stats->samples);
        stats->samples = NULL;
    }
}

// ============ MAIN ============

int main(int argc, char *argv[])
{
    Config config = parse_args(argc, argv);

    // Initialize debug metrics (enabled via -D/--debug or compile-time DEBUG_METRICS)
#ifdef DEBUG_METRICS
    // If compiled with DEBUG_METRICS, enable by default unless explicitly disabled
    debug_metrics_init(true, 1.0);
#else
    // Runtime flag only works if DEBUG_METRICS is defined at compile time
    debug_metrics_init(config.debug_mode, 1.0);
#endif

    // Initialize window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Tree Growth Simulator");
    SetTargetFPS(0);  // Uncapped for benchmarking

    // Initialize game state
    static GameState state = {0};
    game_init_full(&state, config.num_trees, config.seed);

    // Initialize rendering
    render_init();

    // Benchmark stats
    BenchmarkStats stats = {0};
    if (config.benchmark_mode) {
        stats_init(&stats);
        printf("Benchmark mode: %d trees, %.1f seconds\n",
               state.tree_count, config.benchmark_duration);
    }

    // Let trees grow for a bit before benchmarking
    if (config.benchmark_mode) {
        printf("Growing trees for 2 seconds...\n");
        float grow_time = 0;
        while (grow_time < 2.0f && !WindowShouldClose()) {
            game_update(&state);
            render_frame(&state);
            grow_time += GetFrameTime();
        }
        stats.start_time = (float)GetTime();  // Reset start time
    }

    // Main game loop
    while (!WindowShouldClose() && state.running) {
        float fps = GetFPS();

        // Record FPS in benchmark mode
        if (config.benchmark_mode) {
            stats_record(&stats, fps);

            // Check if benchmark duration elapsed
            float elapsed = (float)GetTime() - stats.start_time;
            if (elapsed >= config.benchmark_duration) {
                break;
            }
        }

        game_update(&state);
        render_frame(&state);

        // Update debug metrics (emits once per second if enabled)
#ifdef DEBUG_METRICS
        svo_update_debug_metrics(&state.matter_svo);
#endif
        debug_metrics_update(GetTime());
    }

    // Write benchmark results
    if (config.benchmark_mode) {
        stats_write(&stats, &config, &state);
        stats_cleanup(&stats);
    }

    // Cleanup
    render_cleanup();
    game_cleanup(&state);
    CloseWindow();

    return 0;
}
