#ifndef NOISE_H
#define NOISE_H

#include <stdint.h>

// ============ NOISE CONFIGURATION ============

typedef struct {
    uint32_t seed;           // Random seed for reproducibility
    int octaves;             // Number of noise layers (1-8)
    float lacunarity;        // Frequency multiplier per octave (typically 2.0)
    float persistence;       // Amplitude multiplier per octave (typically 0.5)
    float scale;             // Base frequency scale
} NoiseConfig;

// ============ NOISE FUNCTIONS ============

// Initialize noise generator with a seed
void noise_init(uint32_t seed);

// Get the current seed
uint32_t noise_get_seed(void);

// 2D Simplex noise, returns value in range [-1, 1]
float noise_simplex2d(float x, float y);

// 3D Simplex noise, returns value in range [-1, 1]
float noise_simplex3d(float x, float y, float z);

// 2D Fractal Brownian Motion (layered noise)
// Returns value in range approximately [-1, 1]
float noise_fbm2d(float x, float y, const NoiseConfig *config);

// 3D Fractal Brownian Motion
float noise_fbm3d(float x, float y, float z, const NoiseConfig *config);

// Utility: remap value from [-1,1] to [0,1]
float noise_normalize(float value);

// Utility: remap and scale to integer range
int noise_to_int(float value, int min, int max);

// Default terrain configuration
NoiseConfig noise_terrain_default(uint32_t seed);

#endif // NOISE_H
