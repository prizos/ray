# Plan: Chunk-Based Voxel Physics System

## Executive Summary

Replace the current Sparse Voxel Octree (SVO) with a **chunk-based system** optimized for physics simulation. The octree's O(8) neighbor traversal and cache-hostile pointer chasing make it unsuitable for global physics. A chunk-based system provides O(1) neighbor access and cache-friendly memory layout.

## Research Sources

- [GDC: Noita Tech Talk](https://www.gdcvault.com/play/1025695/) - 64×64 chunks, dirty rects, checker pattern threading
- [Hierarchical Voxel Hash](https://link.springer.com/chapter/10.1007/978-3-642-40602-7_33) - O(log log N) neighbor lookup
- [Voxel Terrain Storage](https://zeux.io/2017/03/27/voxel-terrain-storage/) - Chunk size analysis
- [CA Optimization](https://cell-auto.com/optimisation/) - Event-driven updates
- [Physics Sleep/Wake](https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=1908) - Dormant region tracking

## Problem Analysis

### Current System Costs (per physics step with N active cells):

```
Per active cell:
├── 6 neighbor lookups × O(8) tree traversal = O(48) operations
├── Temperature calculation (expensive, called multiple times)
├── Material iteration
└── Tree node expansion/collapse overhead

With 32,776 active cells × 3 passes = ~4.7 million tree traversals/step
```

### Why Octree Fails for Physics:

1. **O(depth) neighbor access** - Must traverse 8 levels for each of 6 neighbors
2. **Cache misses** - Pointer chasing through non-contiguous memory
3. **Collapse/expand overhead** - Uniform node optimization conflicts with physics needs
4. **No spatial locality** - Adjacent cells not adjacent in memory

## Proposed Architecture

### Core Data Structures

```c
#define CHUNK_SIZE 32          // 32×32×32 = 32,768 cells per chunk
#define CHUNK_SIZE_BITS 5      // log2(32) for fast division
#define CHUNK_VOLUME (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

// Single cell - simplified, no dynamic arrays
typedef struct {
    uint16_t present;              // Bitmask of materials present
    MaterialState materials[MAT_COUNT];  // Fixed array (already optimized)
} Cell3D;

// Chunk - contiguous block of cells
typedef struct Chunk {
    Cell3D cells[CHUNK_VOLUME];    // Flat array, O(1) access

    // Chunk coordinates (world position / CHUNK_SIZE)
    int32_t cx, cy, cz;

    // Cached neighbor pointers (O(1) cross-chunk access)
    struct Chunk *neighbors[6];    // +X, -X, +Y, -Y, +Z, -Z

    // Activity tracking
    uint32_t dirty_min[3];         // Dirty region min bounds
    uint32_t dirty_max[3];         // Dirty region max bounds
    bool is_active;                // Any activity this frame?
    bool is_stable;                // At equilibrium for N frames?
    uint8_t stable_frames;         // Frames since last change

    // For hash table
    struct Chunk *hash_next;       // Collision chain
} Chunk;

// World - sparse chunk storage
typedef struct {
    Chunk **hash_table;            // Hash map: hash(cx,cy,cz) → Chunk*
    uint32_t hash_size;            // Power of 2
    uint32_t hash_mask;            // hash_size - 1

    // Active chunk list (for physics iteration)
    Chunk **active_chunks;
    int active_count;
    int active_capacity;

    // Statistics
    uint32_t chunk_count;
    uint64_t tick;
} ChunkWorld;
```

### Key Operations Complexity

| Operation | Old (SVO) | New (Chunk) |
|-----------|-----------|-------------|
| Get cell | O(8) | O(1) |
| Get neighbor (same chunk) | O(8) | O(1) |
| Get neighbor (cross chunk) | O(8) | O(1) via cached pointer |
| Check if cell exists | O(8) | O(1) hash lookup |
| Iterate active cells | O(N) | O(active chunks × dirty region) |

### Cell Access Pattern

```c
// O(1) cell access within chunk
static inline Cell3D* chunk_get_cell(Chunk *chunk, int lx, int ly, int lz) {
    return &chunk->cells[(lz << (CHUNK_SIZE_BITS * 2)) |
                         (ly << CHUNK_SIZE_BITS) | lx];
}

// O(1) neighbor access (same chunk)
static inline Cell3D* chunk_get_neighbor(Chunk *chunk, int lx, int ly, int lz, int dir) {
    static const int DX[] = {1, -1, 0, 0, 0, 0};
    static const int DY[] = {0, 0, 1, -1, 0, 0};
    static const int DZ[] = {0, 0, 0, 0, 1, -1};

    int nx = lx + DX[dir];
    int ny = ly + DY[dir];
    int nz = lz + DZ[dir];

    // Same chunk - direct access
    if (nx >= 0 && nx < CHUNK_SIZE &&
        ny >= 0 && ny < CHUNK_SIZE &&
        nz >= 0 && nz < CHUNK_SIZE) {
        return chunk_get_cell(chunk, nx, ny, nz);
    }

    // Cross chunk - use cached neighbor pointer
    Chunk *neighbor_chunk = chunk->neighbors[dir];
    if (!neighbor_chunk) return NULL;

    // Wrap coordinates
    nx = (nx + CHUNK_SIZE) % CHUNK_SIZE;
    ny = (ny + CHUNK_SIZE) % CHUNK_SIZE;
    nz = (nz + CHUNK_SIZE) % CHUNK_SIZE;

    return chunk_get_cell(neighbor_chunk, nx, ny, nz);
}
```

### Activity Tracking (Dirty Rectangles)

```c
// Mark cell as modified - expands dirty region
static inline void chunk_mark_dirty(Chunk *chunk, int lx, int ly, int lz) {
    if (!chunk->is_active) {
        chunk->is_active = true;
        chunk->dirty_min[0] = chunk->dirty_max[0] = lx;
        chunk->dirty_min[1] = chunk->dirty_max[1] = ly;
        chunk->dirty_min[2] = chunk->dirty_max[2] = lz;
    } else {
        if (lx < chunk->dirty_min[0]) chunk->dirty_min[0] = lx;
        if (lx > chunk->dirty_max[0]) chunk->dirty_max[0] = lx;
        if (ly < chunk->dirty_min[1]) chunk->dirty_min[1] = ly;
        if (ly > chunk->dirty_max[1]) chunk->dirty_max[1] = ly;
        if (lz < chunk->dirty_min[2]) chunk->dirty_min[2] = lz;
        if (lz > chunk->dirty_max[2]) chunk->dirty_max[2] = lz;
    }
    chunk->stable_frames = 0;
    chunk->is_stable = false;
}

// Physics only iterates dirty region
void chunk_physics_step(Chunk *chunk, float dt) {
    if (!chunk->is_active) return;

    // Expand dirty region by 1 for neighbor interactions
    int x0 = (chunk->dirty_min[0] > 0) ? chunk->dirty_min[0] - 1 : 0;
    int x1 = (chunk->dirty_max[0] < CHUNK_SIZE-1) ? chunk->dirty_max[0] + 1 : CHUNK_SIZE-1;
    // ... same for y, z

    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                process_cell(chunk, x, y, z, dt);
            }
        }
    }
}
```

### Equilibrium Detection

```c
// After physics step, check if chunk reached equilibrium
void chunk_check_equilibrium(Chunk *chunk) {
    if (!chunk->is_active) {
        chunk->stable_frames++;
        if (chunk->stable_frames > 60) {  // 1 second at 60 FPS
            chunk->is_stable = true;
        }
        return;
    }

    // Reset for next frame
    chunk->is_active = false;
    chunk->dirty_min[0] = chunk->dirty_min[1] = chunk->dirty_min[2] = CHUNK_SIZE;
    chunk->dirty_max[0] = chunk->dirty_max[1] = chunk->dirty_max[2] = 0;
}
```

### Multi-threading (Checker Pattern)

```c
// Noita-style 4-pass checker pattern for safe parallel updates
void world_physics_step_parallel(ChunkWorld *world, float dt) {
    // Pass 0: chunks where (cx + cy + cz) % 4 == 0
    // Pass 1: chunks where (cx + cy + cz) % 4 == 1
    // Pass 2: chunks where (cx + cy + cz) % 4 == 2
    // Pass 3: chunks where (cx + cy + cz) % 4 == 3

    for (int pass = 0; pass < 4; pass++) {
        #pragma omp parallel for
        for (int i = 0; i < world->active_count; i++) {
            Chunk *chunk = world->active_chunks[i];
            if ((chunk->cx + chunk->cy + chunk->cz) % 4 == pass) {
                chunk_physics_step(chunk, dt);
            }
        }
    }
}
```

### Temperature Caching

```c
typedef struct {
    double moles;
    double thermal_energy;
    double cached_temperature;  // Cached result
    bool temp_dirty;            // Invalidate on energy change
} MaterialState;

static inline double material_get_temperature_cached(MaterialState *state, MaterialType type) {
    if (state->temp_dirty) {
        state->cached_temperature = material_compute_temperature(state, type);
        state->temp_dirty = false;
    }
    return state->cached_temperature;
}

// Call when energy changes
static inline void material_invalidate_temp(MaterialState *state) {
    state->temp_dirty = true;
}
```

## Implementation Phases

### Phase 1: Archive Old System
- [ ] Create `archive/svo_v1/` directory
- [ ] Move current `svo.h`, `svo.c`, `svo_physics.c` to archive
- [ ] Keep archive buildable for reference

### Phase 2: Core Chunk System
- [ ] Create `include/chunk.h` with data structures
- [ ] Implement `chunk.c`:
  - [ ] `chunk_create()` / `chunk_free()`
  - [ ] `chunk_get_cell()` - O(1) local access
  - [ ] `chunk_get_neighbor()` - O(1) with cached pointers
  - [ ] `chunk_mark_dirty()` - dirty region tracking
- [ ] Implement `chunk_world.c`:
  - [ ] Hash table for chunk storage
  - [ ] `world_get_chunk()` - create on demand
  - [ ] `world_get_cell()` - unified API
  - [ ] Neighbor pointer maintenance

### Phase 3: Physics on Chunks
- [ ] Create `chunk_physics.c`:
  - [ ] Heat conduction (O(1) neighbors)
  - [ ] Liquid flow
  - [ ] Gas diffusion
  - [ ] Dirty region iteration only
  - [ ] Equilibrium detection

### Phase 4: API Compatibility Layer
- [ ] Create wrapper functions matching old SVO API:
  - [ ] `svo_get_cell()` → `world_get_cell()`
  - [ ] `svo_add_water_at()` → `world_add_water_at()`
  - [ ] `svo_physics_step()` → `world_physics_step()`
- [ ] Update `render.c` to use new system

### Phase 5: Migrate Tests
- [ ] Update test helpers to use chunk API
- [ ] All existing tests should pass unchanged
- [ ] Add timing comparisons (old vs new)

### Phase 6: New Unit Tests
- [ ] `test_chunk_unit.c`:
  - [ ] Cell access is O(1) - verify constant time regardless of world size
  - [ ] Neighbor access same chunk - O(1)
  - [ ] Neighbor access cross chunk - O(1) via cached pointer
  - [ ] Hash table operations - O(1) average
  - [ ] Dirty region tracking correctness
- [ ] `test_chunk_perf.c`:
  - [ ] Measure ns per cell access
  - [ ] Measure ns per neighbor lookup
  - [ ] Compare with old SVO times
  - [ ] Verify O(dirty region) not O(world size)

### Phase 7: Optimization
- [ ] Temperature caching
- [ ] Multi-threaded physics (checker pattern)
- [ ] Memory pooling for chunks
- [ ] SIMD for cell iteration (optional)

## Expected Performance Improvement

| Metric | Old SVO | New Chunk | Improvement |
|--------|---------|-----------|-------------|
| Neighbor lookup | ~50ns (8 levels) | ~2ns (array index) | 25× |
| Physics step (32k cells) | ~4500ms | ~50ms (estimate) | 90× |
| Memory per chunk | Variable | 32KB fixed | Predictable |
| Cache behavior | Many misses | Sequential access | 10× hit rate |

## Files to Create

```
include/
├── chunk.h           # Core data structures
├── chunk_world.h     # World/hash table API
└── chunk_physics.h   # Physics API

src/
├── chunk.c           # Chunk operations
├── chunk_world.c     # World management
└── chunk_physics.c   # Physics simulation

archive/svo_v1/
├── svo.h
├── svo.c
└── svo_physics.c

tests/
├── test_chunk_unit.c    # O(1) guarantees
├── test_chunk_perf.c    # Timing benchmarks
└── test_chunk_physics.c # Physics correctness
```

## Success Criteria

1. All existing tests pass with new system
2. `test_water_does_not_flow_through_solid` drops from 4500ms to <100ms
3. Cell access verified O(1) via benchmarks
4. Neighbor access verified O(1) via benchmarks
5. Physics cost proportional to dirty region, not world size
6. Game runs smoothly with full terrain
