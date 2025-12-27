#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdbool.h>

// Simple test framework macros
#define TEST_PASS 0
#define TEST_FAIL 1

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static int test_##name(void)

#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  Running %s... ", #name); \
    if (test_##name() == TEST_PASS) { \
        tests_passed++; \
        printf("PASS\n"); \
    } else { \
        printf("FAIL\n"); \
    } \
} while (0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("Assertion failed: %s (line %d)\n", #cond, __LINE__); \
        return TEST_FAIL; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("Assertion failed: %s == %s (line %d)\n", #a, #b, __LINE__); \
        return TEST_FAIL; \
    } \
} while (0)

#define ASSERT_FLOAT_EQ(a, b, epsilon) do { \
    float diff = (a) - (b); \
    if (diff < 0) diff = -diff; \
    if (diff > (epsilon)) { \
        printf("Assertion failed: %s ~= %s (line %d)\n", #a, #b, __LINE__); \
        return TEST_FAIL; \
    } \
} while (0)

#define TEST_SUMMARY() do { \
    printf("\n%d/%d tests passed\n", tests_passed, tests_run); \
} while (0)

#define TEST_RESULT() (tests_passed == tests_run ? 0 : 1)

#endif // TEST_H
