#include "noise.h"
#include <math.h>
#include <stdlib.h>

// ============ SIMPLEX NOISE IMPLEMENTATION ============
// Based on Stefan Gustavson's implementation with seeded permutation

// Permutation table (will be shuffled based on seed)
static uint8_t perm[512];
static uint8_t perm_mod12[512];

// Current seed
static uint32_t current_seed = 0;

// Gradient vectors for 2D
static const float grad2[][2] = {
    { 1.0f,  1.0f}, {-1.0f,  1.0f}, { 1.0f, -1.0f}, {-1.0f, -1.0f},
    { 1.0f,  0.0f}, {-1.0f,  0.0f}, { 0.0f,  1.0f}, { 0.0f, -1.0f}
};

// Gradient vectors for 3D (12 edges of a cube)
static const float grad3[][3] = {
    { 1.0f,  1.0f,  0.0f}, {-1.0f,  1.0f,  0.0f}, { 1.0f, -1.0f,  0.0f}, {-1.0f, -1.0f,  0.0f},
    { 1.0f,  0.0f,  1.0f}, {-1.0f,  0.0f,  1.0f}, { 1.0f,  0.0f, -1.0f}, {-1.0f,  0.0f, -1.0f},
    { 0.0f,  1.0f,  1.0f}, { 0.0f, -1.0f,  1.0f}, { 0.0f,  1.0f, -1.0f}, { 0.0f, -1.0f, -1.0f}
};

// Skewing factors for 2D simplex
#define F2 0.3660254037844386f   // (sqrt(3) - 1) / 2
#define G2 0.21132486540518713f  // (3 - sqrt(3)) / 6

// Skewing factors for 3D simplex
#define F3 0.3333333333333333f   // 1/3
#define G3 0.1666666666666667f   // 1/6

// Fast floor function
static inline int fast_floor(float x) {
    int xi = (int)x;
    return x < xi ? xi - 1 : xi;
}

// Dot product helpers
static inline float dot2(const float g[2], float x, float y) {
    return g[0] * x + g[1] * y;
}

static inline float dot3(const float g[3], float x, float y, float z) {
    return g[0] * x + g[1] * y + g[2] * z;
}

// Simple xorshift PRNG for shuffling
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void noise_init(uint32_t seed) {
    current_seed = seed;

    // Initialize with identity permutation
    for (int i = 0; i < 256; i++) {
        perm[i] = (uint8_t)i;
    }

    // Fisher-Yates shuffle using seeded PRNG
    uint32_t state = seed;
    if (state == 0) state = 1;  // Avoid zero state

    for (int i = 255; i > 0; i--) {
        uint32_t j = xorshift32(&state) % (i + 1);
        uint8_t temp = perm[i];
        perm[i] = perm[j];
        perm[j] = temp;
    }

    // Duplicate for wraparound
    for (int i = 0; i < 256; i++) {
        perm[256 + i] = perm[i];
        perm_mod12[i] = perm[i] % 12;
        perm_mod12[256 + i] = perm_mod12[i];
    }
}

uint32_t noise_get_seed(void) {
    return current_seed;
}

float noise_simplex2d(float x, float y) {
    float n0, n1, n2;  // Noise contributions from three corners

    // Skew input space to determine simplex cell
    float s = (x + y) * F2;
    int i = fast_floor(x + s);
    int j = fast_floor(y + s);

    // Unskew back to (x, y) space
    float t = (float)(i + j) * G2;
    float X0 = i - t;
    float Y0 = j - t;
    float x0 = x - X0;  // Distance from cell origin
    float y0 = y - Y0;

    // Determine which simplex we're in
    int i1, j1;  // Offsets for second corner
    if (x0 > y0) {
        i1 = 1; j1 = 0;  // Lower triangle
    } else {
        i1 = 0; j1 = 1;  // Upper triangle
    }

    // Offsets for remaining corners
    float x1 = x0 - i1 + G2;
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;

    // Hash coordinates for gradient indices
    int ii = i & 255;
    int jj = j & 255;
    int gi0 = perm[ii + perm[jj]] % 8;
    int gi1 = perm[ii + i1 + perm[jj + j1]] % 8;
    int gi2 = perm[ii + 1 + perm[jj + 1]] % 8;

    // Calculate contributions from each corner
    float t0 = 0.5f - x0*x0 - y0*y0;
    if (t0 < 0) {
        n0 = 0.0f;
    } else {
        t0 *= t0;
        n0 = t0 * t0 * dot2(grad2[gi0], x0, y0);
    }

    float t1 = 0.5f - x1*x1 - y1*y1;
    if (t1 < 0) {
        n1 = 0.0f;
    } else {
        t1 *= t1;
        n1 = t1 * t1 * dot2(grad2[gi1], x1, y1);
    }

    float t2 = 0.5f - x2*x2 - y2*y2;
    if (t2 < 0) {
        n2 = 0.0f;
    } else {
        t2 *= t2;
        n2 = t2 * t2 * dot2(grad2[gi2], x2, y2);
    }

    // Scale to [-1, 1]
    return 70.0f * (n0 + n1 + n2);
}

float noise_simplex3d(float x, float y, float z) {
    float n0, n1, n2, n3;  // Noise contributions from four corners

    // Skew input space
    float s = (x + y + z) * F3;
    int i = fast_floor(x + s);
    int j = fast_floor(y + s);
    int k = fast_floor(z + s);

    // Unskew back
    float t = (float)(i + j + k) * G3;
    float X0 = i - t;
    float Y0 = j - t;
    float Z0 = k - t;
    float x0 = x - X0;
    float y0 = y - Y0;
    float z0 = z - Z0;

    // Determine which simplex we're in
    int i1, j1, k1;  // Offsets for second corner
    int i2, j2, k2;  // Offsets for third corner

    if (x0 >= y0) {
        if (y0 >= z0) {
            i1=1; j1=0; k1=0; i2=1; j2=1; k2=0;  // XYZ order
        } else if (x0 >= z0) {
            i1=1; j1=0; k1=0; i2=1; j2=0; k2=1;  // XZY order
        } else {
            i1=0; j1=0; k1=1; i2=1; j2=0; k2=1;  // ZXY order
        }
    } else {
        if (y0 < z0) {
            i1=0; j1=0; k1=1; i2=0; j2=1; k2=1;  // ZYX order
        } else if (x0 < z0) {
            i1=0; j1=1; k1=0; i2=0; j2=1; k2=1;  // YZX order
        } else {
            i1=0; j1=1; k1=0; i2=1; j2=1; k2=0;  // YXZ order
        }
    }

    // Offsets for corners
    float x1 = x0 - i1 + G3;
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3;
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3;
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;

    // Hash coordinates for gradient indices
    int ii = i & 255;
    int jj = j & 255;
    int kk = k & 255;
    int gi0 = perm_mod12[ii + perm[jj + perm[kk]]];
    int gi1 = perm_mod12[ii + i1 + perm[jj + j1 + perm[kk + k1]]];
    int gi2 = perm_mod12[ii + i2 + perm[jj + j2 + perm[kk + k2]]];
    int gi3 = perm_mod12[ii + 1 + perm[jj + 1 + perm[kk + 1]]];

    // Calculate contributions
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    if (t0 < 0) {
        n0 = 0.0f;
    } else {
        t0 *= t0;
        n0 = t0 * t0 * dot3(grad3[gi0], x0, y0, z0);
    }

    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    if (t1 < 0) {
        n1 = 0.0f;
    } else {
        t1 *= t1;
        n1 = t1 * t1 * dot3(grad3[gi1], x1, y1, z1);
    }

    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    if (t2 < 0) {
        n2 = 0.0f;
    } else {
        t2 *= t2;
        n2 = t2 * t2 * dot3(grad3[gi2], x2, y2, z2);
    }

    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    if (t3 < 0) {
        n3 = 0.0f;
    } else {
        t3 *= t3;
        n3 = t3 * t3 * dot3(grad3[gi3], x3, y3, z3);
    }

    // Scale to [-1, 1]
    return 32.0f * (n0 + n1 + n2 + n3);
}

float noise_fbm2d(float x, float y, const NoiseConfig *config) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = config->scale;
    float max_value = 0.0f;  // For normalization

    for (int i = 0; i < config->octaves; i++) {
        total += noise_simplex2d(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;

        amplitude *= config->persistence;
        frequency *= config->lacunarity;
    }

    // Normalize to [-1, 1]
    return total / max_value;
}

float noise_fbm3d(float x, float y, float z, const NoiseConfig *config) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = config->scale;
    float max_value = 0.0f;

    for (int i = 0; i < config->octaves; i++) {
        total += noise_simplex3d(x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;

        amplitude *= config->persistence;
        frequency *= config->lacunarity;
    }

    return total / max_value;
}

float noise_normalize(float value) {
    return (value + 1.0f) * 0.5f;
}

int noise_to_int(float value, int min, int max) {
    float normalized = noise_normalize(value);
    return min + (int)(normalized * (float)(max - min));
}

NoiseConfig noise_terrain_default(uint32_t seed) {
    NoiseConfig config = {
        .seed = seed,
        .octaves = 6,
        .lacunarity = 2.0f,
        .persistence = 0.5f,
        .scale = 0.02f  // Adjust for terrain size
    };
    return config;
}
