#ifndef WATER_H
#define WATER_H

#include "raylib.h"
#include <stdint.h>
#include <stdbool.h>

// ============ FIXED-POINT MATH ============
// 16.16 format for network determinism

typedef int32_t fixed16_t;

#define FIXED_SHIFT 16
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_HALF (1 << (FIXED_SHIFT - 1))

// Conversion macros
#define FLOAT_TO_FIXED(f) ((fixed16_t)((f) * FIXED_ONE))
#define FIXED_TO_FLOAT(f) ((float)(f) / FIXED_ONE)
#define INT_TO_FIXED(i) ((fixed16_t)((i) << FIXED_SHIFT))
#define FIXED_TO_INT(f) ((int)((f) >> FIXED_SHIFT))

// Fixed-point arithmetic with rounding (prevents numerical drift)
static inline fixed16_t fixed_mul(fixed16_t a, fixed16_t b) {
    int64_t result = (int64_t)a * b;
    // Round to nearest instead of truncating (add half before shift)
    if (result >= 0) {
        return (fixed16_t)((result + FIXED_HALF) >> FIXED_SHIFT);
    } else {
        return (fixed16_t)((result - FIXED_HALF) >> FIXED_SHIFT);
    }
}

static inline fixed16_t fixed_div(fixed16_t a, fixed16_t b) {
    int64_t result = ((int64_t)a << FIXED_SHIFT);
    // Round to nearest
    if ((result >= 0) == (b >= 0)) {
        return (fixed16_t)((result + b/2) / b);
    } else {
        return (fixed16_t)((result - b/2) / b);
    }
}

// ============ WATER CONSTANTS ============

#define WATER_RESOLUTION 160        // Match terrain resolution
#define WATER_CELL_SIZE 2.5f        // Match terrain scale

// Simulation parameters
#define WATER_UPDATE_HZ 60.0f                 // Higher update rate for smoother flow
#define WATER_UPDATE_DT (1.0f / WATER_UPDATE_HZ)
#define WATER_GRAVITY FLOAT_TO_FIXED(40.0f)   // Increased gravity for faster flow
#define WATER_DAMPING FLOAT_TO_FIXED(0.95f)   // More damping for stability
#define WATER_PIPE_AREA FLOAT_TO_FIXED(1.0f)  // Cross-sectional area of pipes
#define WATER_CELL_WIDTH FLOAT_TO_FIXED(2.5f) // Cell width in world units

// Depth limits
#define WATER_MIN_DEPTH FLOAT_TO_FIXED(0.01f)  // Below this, cell won't generate outflow
#define WATER_MAX_DEPTH FLOAT_TO_FIXED(20.0f)  // Maximum water depth

// Flow thresholds for stability
#define WATER_MIN_HEAD_DIFF FLOAT_TO_FIXED(0.001f)  // Ignore tiny pressure differences (prevents numerical drift)

// Edge drainage - only at actual map boundaries
#define WATER_EDGE_DRAIN_RATE FLOAT_TO_FIXED(0.3f)  // Gentle edge drainage

// Waterfall threshold (terrain drop > 1 unit triggers waterfall)
#define WATER_FALL_THRESHOLD INT_TO_FIXED(1)

// ============ WATER DATA STRUCTURES ============

// Per-cell water state (24 bytes)
typedef struct {
    fixed16_t water_height;   // Water depth at this cell
    fixed16_t flow_north;     // Pipe flow to north neighbor
    fixed16_t flow_south;     // Pipe flow to south neighbor
    fixed16_t flow_east;      // Pipe flow to east neighbor
    fixed16_t flow_west;      // Pipe flow to west neighbor
    fixed16_t flow_down;      // Vertical flow (waterfalls)
} WaterCell;

// Complete water simulation state
typedef struct {
    WaterCell cells[WATER_RESOLUTION][WATER_RESOLUTION];
    fixed16_t terrain_height[WATER_RESOLUTION][WATER_RESOLUTION];  // Cached terrain

    uint32_t tick;            // Simulation tick counter
    float accumulator;        // Time accumulator for fixed timestep

    fixed16_t total_water;    // Total water in system (for conservation check)
    uint32_t checksum;        // CRC32 for network sync verification

    bool initialized;         // Whether system has been initialized
} WaterState;

// ============ WATER API ============

// Initialize water simulation from terrain heightmap
void water_init(WaterState *water, const int terrain[WATER_RESOLUTION][WATER_RESOLUTION]);

// Reset water state (remove all water)
void water_reset(WaterState *water);

// Update water simulation (call every frame with deltaTime)
// Returns number of simulation steps taken
int water_update(WaterState *water, float deltaTime);

// Perform a single simulation step (fixed timestep)
void water_step(WaterState *water);

// Add water at a cell (amount in fixed-point)
void water_add(WaterState *water, int x, int z, fixed16_t amount);

// Remove water at a cell (amount in fixed-point, returns actual removed)
fixed16_t water_remove(WaterState *water, int x, int z, fixed16_t amount);

// Add water at world position (converts to cell coordinates)
void water_add_at_world(WaterState *water, float world_x, float world_z, float amount);

// Get water depth at cell (returns fixed-point)
fixed16_t water_get_depth(const WaterState *water, int x, int z);

// Get water depth at world position (returns float)
float water_get_depth_at_world(const WaterState *water, float world_x, float world_z);

// Get total water surface height at cell (terrain + water depth)
fixed16_t water_get_surface_height(const WaterState *water, int x, int z);

// Calculate total water in system (for conservation verification)
fixed16_t water_calculate_total(const WaterState *water);

// Calculate checksum for network sync
uint32_t water_calculate_checksum(const WaterState *water);

// ============ UTILITY FUNCTIONS ============

// Convert world position to cell coordinates
// World starts at (0,0) and extends to (RESOLUTION * CELL_SIZE, RESOLUTION * CELL_SIZE)
static inline void water_world_to_cell(float world_x, float world_z, int *cell_x, int *cell_z) {
    *cell_x = (int)(world_x / WATER_CELL_SIZE);
    *cell_z = (int)(world_z / WATER_CELL_SIZE);

    // Clamp to valid range
    if (*cell_x < 0) *cell_x = 0;
    if (*cell_x >= WATER_RESOLUTION) *cell_x = WATER_RESOLUTION - 1;
    if (*cell_z < 0) *cell_z = 0;
    if (*cell_z >= WATER_RESOLUTION) *cell_z = WATER_RESOLUTION - 1;
}

// Convert cell coordinates to world position (center of cell)
static inline void water_cell_to_world(int cell_x, int cell_z, float *world_x, float *world_z) {
    *world_x = (cell_x * WATER_CELL_SIZE) + (WATER_CELL_SIZE / 2.0f);
    *world_z = (cell_z * WATER_CELL_SIZE) + (WATER_CELL_SIZE / 2.0f);
}

// Check if cell coordinates are valid
static inline bool water_cell_valid(int x, int z) {
    return x >= 0 && x < WATER_RESOLUTION && z >= 0 && z < WATER_RESOLUTION;
}

// Check if cell is at map edge
static inline bool water_cell_is_edge(int x, int z) {
    return x == 0 || x == WATER_RESOLUTION - 1 || z == 0 || z == WATER_RESOLUTION - 1;
}

#endif // WATER_H
