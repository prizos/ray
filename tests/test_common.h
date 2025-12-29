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

// ============ TEST RESULT TRACKING ============

typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    const char *current_suite;
} TestContext;

static TestContext g_test_ctx = {0, 0, 0, NULL};

static inline void test_suite_begin(const char *name) {
    g_test_ctx.current_suite = name;
    printf("\n=== %s ===\n\n", name);
}

static inline void test_suite_end(void) {
    printf("\n");
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
    printf("========================================\n\n");
}

static inline int test_exit_code(void) {
    return g_test_ctx.tests_failed > 0 ? 1 : 0;
}

// ============ ASSERTIONS ============

#define TEST_BEGIN(name) \
    printf("  %s... ", name); \
    fflush(stdout);

#define TEST_PASS() \
    do { \
        g_test_ctx.tests_run++; \
        g_test_ctx.tests_passed++; \
        printf("PASS\n"); \
        return true; \
    } while(0)

#define TEST_FAIL(msg, ...) \
    do { \
        g_test_ctx.tests_run++; \
        g_test_ctx.tests_failed++; \
        printf("FAIL: " msg "\n", ##__VA_ARGS__); \
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
    ASSERT(fabsf((a) - (b)) <= (eps), msg " (got %.6f, expected %.6f)", (float)(a), (float)(b))

#define ASSERT_TRUE(cond, msg) ASSERT(cond, msg)
#define ASSERT_FALSE(cond, msg) ASSERT(!(cond), msg)

// ============ FIXED-POINT MATH ============
// Only define if not already defined by water.h

#ifndef WATER_H  // water.h not included - define our own

typedef int32_t fixed16_t;

#define FIXED_SHIFT 16
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_HALF (1 << (FIXED_SHIFT - 1))

#define FLOAT_TO_FIXED(f) ((fixed16_t)((f) * FIXED_ONE))
#define FIXED_TO_FLOAT(f) ((float)(f) / FIXED_ONE)

static inline fixed16_t fixed_mul(fixed16_t a, fixed16_t b) {
    int64_t result = (int64_t)a * b;
    if (result >= 0) {
        return (fixed16_t)((result + FIXED_HALF) >> FIXED_SHIFT);
    } else {
        return (fixed16_t)((result - FIXED_HALF) >> FIXED_SHIFT);
    }
}

static inline fixed16_t fixed_div(fixed16_t a, fixed16_t b) {
    int64_t result = ((int64_t)a << FIXED_SHIFT);
    if ((result >= 0) == (b >= 0)) {
        return (fixed16_t)((result + b/2) / b);
    } else {
        return (fixed16_t)((result - b/2) / b);
    }
}

#endif // WATER_H

// ============ CONSTANTS ============

#define TEST_AMBIENT_TEMP_K 293.15f
#define TEST_AMBIENT_TEMP FLOAT_TO_FIXED(293.15f)
#define TEST_FIRE_TEMP FLOAT_TO_FIXED(400.0f)
#define TEST_COLD_TEMP FLOAT_TO_FIXED(243.15f)  // 50K below ambient (-30°C)
#define TEST_IGNITION_TEMP FLOAT_TO_FIXED(533.0f)

#endif // TEST_COMMON_H
