/**
 * Common Test Infrastructure
 *
 * Shared utilities, assertions, and helpers for all test tiers.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include "chunk.h"

// ============ TIMING UTILITIES ============

static inline double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// ============ TEST METRICS ============

typedef struct {
    uint64_t cells_processed;
    uint64_t physics_steps;
    uint64_t active_nodes;
    uint64_t neighbor_lookups;
    double elapsed_ms;
} TestMetrics;

static TestMetrics g_test_metrics = {0};

static inline void test_metrics_reset(void) {
    memset(&g_test_metrics, 0, sizeof(g_test_metrics));
}

static inline void test_metrics_print(void) {
    if (g_test_metrics.physics_steps > 0 || g_test_metrics.cells_processed > 0) {
        printf(" [%.1fms", g_test_metrics.elapsed_ms);
        if (g_test_metrics.physics_steps > 0)
            printf(", %llu steps", (unsigned long long)g_test_metrics.physics_steps);
        if (g_test_metrics.active_nodes > 0)
            printf(", %llu active", (unsigned long long)g_test_metrics.active_nodes);
        if (g_test_metrics.cells_processed > 0)
            printf(", %llu cells", (unsigned long long)g_test_metrics.cells_processed);
        printf("]");
    } else if (g_test_metrics.elapsed_ms > 0.1) {
        printf(" [%.1fms]", g_test_metrics.elapsed_ms);
    }
}

// ============ TEST RESULT TRACKING ============

typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    const char *current_suite;
    double test_start_time;
    double suite_start_time;
    double total_time_ms;
} TestContext;

static TestContext g_test_ctx = {0, 0, 0, NULL, 0, 0, 0};

static inline void test_suite_begin(const char *name) {
    g_test_ctx.current_suite = name;
    g_test_ctx.suite_start_time = get_time_ms();
    printf("\n=== %s ===\n\n", name);
}

static inline void test_suite_end(void) {
    double suite_elapsed = get_time_ms() - g_test_ctx.suite_start_time;
    printf("\n  Suite time: %.1fms\n", suite_elapsed);
    g_test_ctx.total_time_ms += suite_elapsed;
}

static inline void test_summary(void) {
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed",
           g_test_ctx.tests_passed, g_test_ctx.tests_run);
    if (g_test_ctx.tests_failed > 0) {
        printf(" (%d FAILED)\n", g_test_ctx.tests_failed);
    } else {
        printf(" (ALL PASSED)\n");
    }
    printf("Total time: %.1fms\n", g_test_ctx.total_time_ms);
    printf("========================================\n\n");
}

static inline int test_exit_code(void) {
    return g_test_ctx.tests_failed > 0 ? 1 : 0;
}

// ============ ASSERTIONS ============

#define TEST_BEGIN(name) \
    do { \
        test_metrics_reset(); \
        g_test_ctx.test_start_time = get_time_ms(); \
        printf("  %s... ", name); \
        fflush(stdout); \
    } while(0)

#define TEST_PASS() \
    do { \
        g_test_ctx.tests_run++; \
        g_test_ctx.tests_passed++; \
        g_test_metrics.elapsed_ms = get_time_ms() - g_test_ctx.test_start_time; \
        printf("PASS"); \
        test_metrics_print(); \
        printf("\n"); \
        return true; \
    } while(0)

#define TEST_FAIL(msg, ...) \
    do { \
        g_test_ctx.tests_run++; \
        g_test_ctx.tests_failed++; \
        g_test_metrics.elapsed_ms = get_time_ms() - g_test_ctx.test_start_time; \
        printf("FAIL: " msg, ##__VA_ARGS__); \
        test_metrics_print(); \
        printf("\n"); \
        return false; \
    } while(0)

#define ASSERT(cond, msg, ...) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg, ##__VA_ARGS__); \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    ASSERT((a) == (b), msg " (got %d, expected %d)", (int)(a), (int)(b))

#define ASSERT_FLOAT_EQ(a, b, eps, msg) \
    ASSERT(fabs((a) - (b)) <= (eps), msg " (got %.6f, expected %.6f)", (double)(a), (double)(b))

#define ASSERT_TRUE(cond, msg) ASSERT(cond, msg)
#define ASSERT_FALSE(cond, msg) ASSERT(!(cond), msg)

// ============ METRICS RECORDING ============

#define TEST_RECORD_PHYSICS_STEP() \
    do { g_test_metrics.physics_steps++; } while(0)

#define TEST_RECORD_ACTIVE_NODES(n) \
    do { g_test_metrics.active_nodes += (n); } while(0)

#define TEST_RECORD_CELLS(n) \
    do { g_test_metrics.cells_processed += (n); } while(0)

// ============ CONSTANTS ============

// Standard test temperatures (Kelvin)
#define TEST_AMBIENT_TEMP_K 293.15   // 20°C
#define TEST_FIRE_TEMP_K 400.0       // ~127°C
#define TEST_COLD_TEMP_K 243.15      // -30°C
#define TEST_IGNITION_TEMP_K 533.0   // ~260°C

// ============ ENERGY CALCULATION ============

/**
 * Calculate thermal energy for a material at a given temperature.
 * For single-phase materials: E = n * Cp * T
 *
 * See docs/physics.md for derivation.
 */
static inline double calculate_material_energy(MaterialType type, double moles, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double Cp = props->molar_heat_capacity;
    return moles * Cp * temp_k;
}

#endif // TEST_COMMON_H
