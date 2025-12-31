#ifndef CHUNK_H
#define CHUNK_H

#include "raylib.h"
#include "terrain.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============ WORLD CONSTANTS ============

#define CHUNK_SIZE 32              // Cells per axis per chunk (32×32×32)
#define CHUNK_SIZE_BITS 5          // log2(32) for fast division/modulo
#define CHUNK_SIZE_MASK 31         // CHUNK_SIZE - 1 for fast modulo
#define CHUNK_VOLUME (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)  // 32768 cells

#define WORLD_SIZE_CHUNKS 8        // Chunks per axis (8×8×8 = 512 chunks max)
#define WORLD_SIZE_CELLS (WORLD_SIZE_CHUNKS * CHUNK_SIZE)    // 256 cells per axis

#define WORLD_GROUND_Y 128         // Y=128 is ground level (in cell coords)
#define VOXEL_CELL_SIZE 2.5f       // World units per physics cell (renamed to avoid conflict with game.h)

// ============ PHYSICS CONSTANTS ============

#define INITIAL_TEMP_K 293.0       // 20°C - initial temperature for new matter
#define TEMP_EPSILON 0.1           // Temperature tolerance for equilibrium
#define MOLES_EPSILON 1e-10        // Moles tolerance for empty check
#define EQUILIBRIUM_FRAMES 60      // Frames of no activity before marking stable

// ============ NEIGHBOR DIRECTIONS ============

typedef enum {
    DIR_POS_X = 0,  // +X
    DIR_NEG_X = 1,  // -X
    DIR_POS_Y = 2,  // +Y (up)
    DIR_NEG_Y = 3,  // -Y (down)
    DIR_POS_Z = 4,  // +Z
    DIR_NEG_Z = 5,  // -Z
    DIR_COUNT = 6
} Direction;

// Direction offsets
static const int DIR_DX[6] = { 1, -1,  0,  0,  0,  0};
static const int DIR_DY[6] = { 0,  0,  1, -1,  0,  0};
static const int DIR_DZ[6] = { 0,  0,  0,  0,  1, -1};

// Opposite direction lookup
static const Direction DIR_OPPOSITE[6] = {
    DIR_NEG_X, DIR_POS_X, DIR_NEG_Y, DIR_POS_Y, DIR_NEG_Z, DIR_POS_Z
};

// ============ MATERIAL TYPES ============

typedef enum {
    MAT_NONE = 0,
    MAT_AIR,
    MAT_WATER,
    MAT_ROCK,
    MAT_DIRT,
    MAT_NITROGEN,
    MAT_OXYGEN,
    MAT_CARBON_DIOXIDE,
    MAT_STEAM,
    MAT_COUNT
} MaterialType;

// ============ PHASE ENUM ============

typedef enum {
    PHASE_SOLID,
    PHASE_LIQUID,
    PHASE_GAS
} Phase;

// ============ MATERIAL STATE ============

typedef struct {
    double moles;
    double thermal_energy;
    // Cached temperature (invalidated when energy changes)
    double cached_temp;
    bool temp_valid;
} MaterialState;

// ============ MATERIAL ENTRY (API compatibility) ============

typedef struct {
    MaterialType type;
    MaterialState state;
} MaterialEntry;

// ============ CELL STRUCTURE ============

typedef struct {
    MaterialState materials[MAT_COUNT];
    uint16_t present;  // Bitmask: bit i set = materials[i] is valid
} Cell3D;

// O(1) material access macros
#define CELL_HAS_MATERIAL(cell, type) (((cell)->present >> (type)) & 1)
#define CELL_MATERIAL_COUNT(cell) (__builtin_popcount((cell)->present))

#define CELL_FOR_EACH_MATERIAL(cell, var) \
    for (MaterialType var = (MaterialType)1; var < MAT_COUNT; var = (MaterialType)(var + 1)) \
        if (CELL_HAS_MATERIAL(cell, var))

// ============ MATERIAL PROPERTIES ============

typedef struct {
    const char *name;
    const char *formula;
    double molar_mass;
    double molar_volume_solid;
    double molar_volume_liquid;
    double molar_volume_gas;
    double molar_heat_capacity_solid;
    double molar_heat_capacity_liquid;
    double molar_heat_capacity_gas;
    double melting_point;
    double boiling_point;
    double enthalpy_fusion;
    double enthalpy_vaporization;
    double thermal_conductivity;
    double viscosity;
    bool is_oxidizer;
    bool is_fuel;
    double ignition_temp;
    double enthalpy_combustion;
    Color color_solid;
    Color color_liquid;
    Color color_gas;
} MaterialProperties;

extern const MaterialProperties MATERIAL_PROPS[MAT_COUNT];

// ============ CHUNK STRUCTURE ============

typedef struct Chunk Chunk;

struct Chunk {
    // Flat array of cells - O(1) access via index
    Cell3D cells[CHUNK_VOLUME];

    // Chunk position in chunk coordinates
    int32_t cx, cy, cz;

    // Cached neighbor chunk pointers for O(1) cross-chunk access
    // Updated when chunks are created/destroyed
    Chunk *neighbors[DIR_COUNT];

    // Dirty region tracking (local coordinates within chunk)
    // Only cells within dirty region are processed
    uint8_t dirty_min_x, dirty_min_y, dirty_min_z;
    uint8_t dirty_max_x, dirty_max_y, dirty_max_z;

    // Activity flags
    bool is_active;        // Has activity this frame
    bool is_stable;        // At equilibrium, skip physics
    uint8_t stable_frames; // Frames since last activity

    // Hash table chain
    Chunk *hash_next;

    // Index in active list (-1 if not active)
    int32_t active_list_idx;
};

// ============ CHUNK WORLD STRUCTURE ============

#define CHUNK_HASH_SIZE 1024   // Power of 2
#define CHUNK_HASH_MASK (CHUNK_HASH_SIZE - 1)

typedef struct {
    // Hash table for sparse chunk storage
    Chunk *hash_table[CHUNK_HASH_SIZE];

    // Active chunk list for physics iteration
    Chunk **active_chunks;
    int active_count;
    int active_capacity;

    // Statistics
    uint32_t chunk_count;
    uint64_t tick;
    float accumulator;
} ChunkWorld;

// ============ COORDINATE CONVERSION ============

// World coords to cell coords
static inline void world_to_cell(float wx, float wy, float wz,
                                  int *cx, int *cy, int *cz) {
    *cx = (int)floorf(wx / VOXEL_CELL_SIZE) + WORLD_SIZE_CELLS / 2;
    *cy = (int)floorf(wy / VOXEL_CELL_SIZE) + WORLD_GROUND_Y;
    *cz = (int)floorf(wz / VOXEL_CELL_SIZE) + WORLD_SIZE_CELLS / 2;
}

// Cell coords to world coords (center of cell)
static inline void cell_to_world(int cx, int cy, int cz,
                                  float *wx, float *wy, float *wz) {
    *wx = (cx - WORLD_SIZE_CELLS / 2 + 0.5f) * VOXEL_CELL_SIZE;
    *wy = (cy - WORLD_GROUND_Y + 0.5f) * VOXEL_CELL_SIZE;
    *wz = (cz - WORLD_SIZE_CELLS / 2 + 0.5f) * VOXEL_CELL_SIZE;
}

// Cell coords to chunk coords
static inline void cell_to_chunk(int cx, int cy, int cz,
                                  int *chunk_x, int *chunk_y, int *chunk_z,
                                  int *local_x, int *local_y, int *local_z) {
    *chunk_x = cx >> CHUNK_SIZE_BITS;
    *chunk_y = cy >> CHUNK_SIZE_BITS;
    *chunk_z = cz >> CHUNK_SIZE_BITS;
    *local_x = cx & CHUNK_SIZE_MASK;
    *local_y = cy & CHUNK_SIZE_MASK;
    *local_z = cz & CHUNK_SIZE_MASK;
}

// ============ CELL INDEX CALCULATION ============

// O(1) index into chunk's cell array
static inline int cell_index(int lx, int ly, int lz) {
    return (lz << (CHUNK_SIZE_BITS * 2)) | (ly << CHUNK_SIZE_BITS) | lx;
}

// ============ CELL FUNCTIONS ============

void cell_init(Cell3D *cell);
void cell_free(Cell3D *cell);
Cell3D cell_clone(const Cell3D *src);
void cell_add_material(Cell3D *cell, MaterialType type, double moles, double energy);
void cell_remove_material(Cell3D *cell, MaterialType type);
bool cells_match(const Cell3D *a, const Cell3D *b);

static inline MaterialState* cell_get_material(Cell3D *cell, MaterialType type) {
    return CELL_HAS_MATERIAL(cell, type) ? &cell->materials[type] : NULL;
}

static inline const MaterialState* cell_get_material_const(const Cell3D *cell, MaterialType type) {
    return CELL_HAS_MATERIAL(cell, type) ? &cell->materials[type] : NULL;
}

// Legacy API compatibility
MaterialEntry* cell_find_material(Cell3D *cell, MaterialType type);
const MaterialEntry* cell_find_material_const(const Cell3D *cell, MaterialType type);

// ============ MATERIAL FUNCTIONS ============

double material_get_temperature(MaterialState *state, MaterialType type);
double material_get_mass(const MaterialState *state, MaterialType type);
double material_get_volume(const MaterialState *state, MaterialType type, Phase phase);
Phase material_get_phase(MaterialType type, double temp_k);
Phase material_get_phase_from_energy(const MaterialState *state, MaterialType type);
double get_effective_heat_capacity(const MaterialState *state, MaterialType type);

// Invalidate cached temperature (call when energy changes)
static inline void material_invalidate_temp(MaterialState *state) {
    state->temp_valid = false;
}

// Cell temperature (weighted average)
double cell_get_temperature(Cell3D *cell);
double cell_get_total_volume(const Cell3D *cell);

// ============ CHUNK FUNCTIONS ============

// Create/destroy chunks
Chunk* chunk_create(int cx, int cy, int cz);
void chunk_free(Chunk *chunk);

// O(1) cell access within chunk
static inline Cell3D* chunk_get_cell(Chunk *chunk, int lx, int ly, int lz) {
    return &chunk->cells[cell_index(lx, ly, lz)];
}

static inline const Cell3D* chunk_get_cell_const(const Chunk *chunk, int lx, int ly, int lz) {
    return &chunk->cells[cell_index(lx, ly, lz)];
}

// O(1) neighbor access (handles cross-chunk via cached pointers)
Cell3D* chunk_get_neighbor_cell(Chunk *chunk, int lx, int ly, int lz, Direction dir);

// Mark cell as dirty (expands dirty region)
void chunk_mark_dirty(Chunk *chunk, int lx, int ly, int lz);

// Reset dirty state for next frame
void chunk_reset_dirty(Chunk *chunk);

// Check if chunk is at equilibrium
void chunk_check_equilibrium(Chunk *chunk);

// ============ CHUNK WORLD FUNCTIONS ============

// Initialize/cleanup world
void world_init(ChunkWorld *world);
void world_cleanup(ChunkWorld *world);

// Initialize with terrain
void world_init_terrain(ChunkWorld *world, int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]);

// Get/create chunk at chunk coordinates
Chunk* world_get_chunk(ChunkWorld *world, int cx, int cy, int cz);
Chunk* world_get_or_create_chunk(ChunkWorld *world, int cx, int cy, int cz);

// Get cell at cell coordinates
const Cell3D* world_get_cell(ChunkWorld *world, int x, int y, int z);
Cell3D* world_get_cell_for_write(ChunkWorld *world, int x, int y, int z);

// Mark cell as active (adds chunk to active list, marks dirty)
void world_mark_cell_active(ChunkWorld *world, int x, int y, int z);

// Update neighbor pointers for a chunk
void world_update_chunk_neighbors(ChunkWorld *world, Chunk *chunk);

// ============ PHYSICS ============

// Physics system flags (bitmask)
typedef enum {
    PHYSICS_NONE          = 0,
    PHYSICS_HEAT_INTERNAL = 1 << 0,  // Internal equilibration within cells
    PHYSICS_HEAT_CONDUCT  = 1 << 1,  // Heat conduction between cells
    PHYSICS_LIQUID_FLOW   = 1 << 2,  // Liquid flow (gravity-driven)
    PHYSICS_GAS_DIFFUSE   = 1 << 3,  // Gas diffusion

    // Common combinations
    PHYSICS_HEAT_ALL      = PHYSICS_HEAT_INTERNAL | PHYSICS_HEAT_CONDUCT,
    PHYSICS_MATTER_ALL    = PHYSICS_LIQUID_FLOW | PHYSICS_GAS_DIFFUSE,
    PHYSICS_ALL           = PHYSICS_HEAT_ALL | PHYSICS_MATTER_ALL
} PhysicsFlags;

// Run physics with specific systems enabled
void world_physics_step_flags(ChunkWorld *world, float dt, PhysicsFlags flags);

// Run all physics systems (convenience wrapper for game loop)
void world_physics_step(ChunkWorld *world, float dt);

// ============ TOOL APIs ============

void world_add_heat_at(ChunkWorld *world, float wx, float wy, float wz, double energy);
void world_remove_heat_at(ChunkWorld *world, float wx, float wy, float wz, double energy);
void world_add_water_at(ChunkWorld *world, float wx, float wy, float wz, double moles);

// Cell info for UI
typedef struct {
    int cell_x, cell_y, cell_z;
    bool valid;
    int material_count;
    MaterialType primary_material;
    double temperature;
    Phase primary_phase;
} CellInfo;

CellInfo world_get_cell_info(ChunkWorld *world, float wx, float wy, float wz);

// Debug metrics (only available when DEBUG_METRICS is defined)
#ifdef DEBUG_METRICS
void world_update_debug_metrics(ChunkWorld *world);
#endif

// ============ SVO API COMPATIBILITY LAYER ============
// These wrap the chunk system with SVO-style names for easy migration

// Type alias for compatibility
typedef ChunkWorld MatterSVO;

// Compatibility functions (implemented as wrappers)
#define svo_init(svo, terrain) world_init_terrain(svo, terrain)
#define svo_cleanup(svo) world_cleanup(svo)
#define svo_get_cell(svo, x, y, z) world_get_cell(svo, x, y, z)
#define svo_get_cell_for_write(svo, x, y, z) world_get_cell_for_write(svo, x, y, z)
#define svo_world_to_cell(wx, wy, wz, cx, cy, cz) world_to_cell(wx, wy, wz, cx, cy, cz)
#define svo_cell_to_world(cx, cy, cz, wx, wy, wz) cell_to_world(cx, cy, cz, wx, wy, wz)
#define svo_mark_cell_active(svo, x, y, z) world_mark_cell_active(svo, x, y, z)
#define svo_physics_step(svo, dt) world_physics_step(svo, dt)
#define svo_add_heat_at(svo, wx, wy, wz, e) world_add_heat_at(svo, wx, wy, wz, e)
#define svo_remove_heat_at(svo, wx, wy, wz, e) world_remove_heat_at(svo, wx, wy, wz, e)
#define svo_add_water_at(svo, wx, wy, wz, m) world_add_water_at(svo, wx, wy, wz, m)
#define svo_get_cell_info(svo, wx, wy, wz) world_get_cell_info(svo, wx, wy, wz)
#ifdef DEBUG_METRICS
#define svo_update_debug_metrics(svo) world_update_debug_metrics(svo)
#endif

// Legacy cell function aliases
#define cell3d_init(c) cell_init(c)
#define cell3d_free(c) cell_free(c)
#define cell3d_clone(c) cell_clone(c)
#define cell3d_add_material(c, t, m, e) cell_add_material(c, t, m, e)
#define cell3d_remove_material(c, t) cell_remove_material(c, t)
#define cell3d_get_material(c, t) cell_get_material(c, t)
#define cell3d_get_material_const(c, t) cell_get_material_const(c, t)
#define cell3d_find_material(c, t) cell_find_material(c, t)
#define cell3d_find_material_const(c, t) cell_find_material_const(c, t)

// Constants for compatibility
#define SVO_SIZE WORLD_SIZE_CELLS
#define SVO_GROUND_Y WORLD_GROUND_Y
#define SVO_CELL_SIZE VOXEL_CELL_SIZE

#endif // CHUNK_H
