// Test Simplex noise implementation
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "../include/noise.h"

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

int main(void) {
    printf("=== Noise System Tests ===\n\n");

    // Test 1: Seed reproducibility
    printf("Test 1: Seed reproducibility...\n");
    noise_init(12345);
    float val1 = noise_simplex2d(10.5f, 20.3f);
    float val2 = noise_simplex2d(50.0f, 100.0f);

    noise_init(12345);  // Same seed
    float val1_repeat = noise_simplex2d(10.5f, 20.3f);
    float val2_repeat = noise_simplex2d(50.0f, 100.0f);

    ASSERT(val1 == val1_repeat, "Same seed should produce same result");
    ASSERT(val2 == val2_repeat, "Same seed should produce same result (2)");
    printf("  PASS: Same seed produces identical output\n");

    // Test 2: Different seeds produce different results
    printf("Test 2: Different seeds produce different results...\n");
    noise_init(12345);
    float seed1_val = noise_simplex2d(25.0f, 25.0f);

    noise_init(54321);
    float seed2_val = noise_simplex2d(25.0f, 25.0f);

    ASSERT(seed1_val != seed2_val, "Different seeds should produce different results");
    printf("  PASS: Different seeds produce different output\n");

    // Test 3: Range check [-1, 1]
    printf("Test 3: Output range [-1, 1]...\n");
    noise_init(42);
    float min_val = 1.0f, max_val = -1.0f;
    for (int i = 0; i < 10000; i++) {
        float x = (float)(i % 100) * 0.1f;
        float y = (float)(i / 100) * 0.1f;
        float v = noise_simplex2d(x, y);
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
    }
    ASSERT(min_val >= -1.0f && max_val <= 1.0f, "Output should be in [-1, 1]");
    printf("  PASS: Range is [%.4f, %.4f]\n", min_val, max_val);

    // Test 4: FBM produces varied terrain
    printf("Test 4: FBM terrain variation...\n");
    NoiseConfig config = {
        .seed = 12345,
        .octaves = 6,
        .lacunarity = 2.0f,
        .persistence = 0.5f,
        .scale = 0.02f
    };
    noise_init(config.seed);

    float fbm_min = 1.0f, fbm_max = -1.0f;
    for (int x = 0; x < 100; x++) {
        for (int y = 0; y < 100; y++) {
            float v = noise_fbm2d((float)x, (float)y, &config);
            if (v < fbm_min) fbm_min = v;
            if (v > fbm_max) fbm_max = v;
        }
    }
    float range = fbm_max - fbm_min;
    ASSERT(range > 0.5f, "FBM should produce significant variation");
    printf("  PASS: FBM range is %.4f (min=%.4f, max=%.4f)\n", range, fbm_min, fbm_max);

    // Test 5: 3D noise works
    printf("Test 5: 3D Simplex noise...\n");
    noise_init(99999);
    float v3d_1 = noise_simplex3d(1.0f, 2.0f, 3.0f);
    float v3d_2 = noise_simplex3d(1.1f, 2.0f, 3.0f);
    ASSERT(v3d_1 != v3d_2, "3D noise should vary with position");
    ASSERT(v3d_1 >= -1.0f && v3d_1 <= 1.0f, "3D noise should be in range");
    printf("  PASS: 3D noise values: %.4f, %.4f\n", v3d_1, v3d_2);

    // Test 6: Utility functions
    printf("Test 6: Utility functions...\n");
    ASSERT(fabs(noise_normalize(-1.0f) - 0.0f) < 0.001f, "normalize(-1) should be 0");
    ASSERT(fabs(noise_normalize(1.0f) - 1.0f) < 0.001f, "normalize(1) should be 1");
    ASSERT(fabs(noise_normalize(0.0f) - 0.5f) < 0.001f, "normalize(0) should be 0.5");
    ASSERT(noise_to_int(-1.0f, 0, 10) == 0, "to_int(-1, 0, 10) should be 0");
    ASSERT(noise_to_int(1.0f, 0, 10) == 10, "to_int(1, 0, 10) should be 10");
    printf("  PASS: Utility functions work correctly\n");

    printf("\n=== All noise tests passed! ===\n");
    return 0;
}
