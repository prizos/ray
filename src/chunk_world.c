#include "chunk.h"
#include "debug_metrics.h"
#include <stdio.h>
#include <string.h>

// ============ HASH FUNCTION ============

static inline uint32_t chunk_hash(int cx, int cy, int cz) {
    // Simple spatial hash
    uint32_t h = (uint32_t)(cx * 73856093) ^ (uint32_t)(cy * 19349663) ^ (uint32_t)(cz * 83492791);
    return h & CHUNK_HASH_MASK;
}

// ============ WORLD INIT/CLEANUP ============

void world_init(ChunkWorld *world) {
    memset(world, 0, sizeof(ChunkWorld));

    // Initialize hash table
    for (int i = 0; i < CHUNK_HASH_SIZE; i++) {
        world->hash_table[i] = NULL;
    }

    // Initialize active list
    world->active_capacity = 256;
    world->active_chunks = (Chunk**)calloc(world->active_capacity, sizeof(Chunk*));
    world->active_count = 0;

    world->chunk_count = 0;
    world->tick = 0;
    world->accumulator = 0;
}

void world_cleanup(ChunkWorld *world) {
    // Free all chunks in hash table
    for (int i = 0; i < CHUNK_HASH_SIZE; i++) {
        Chunk *chunk = world->hash_table[i];
        while (chunk) {
            Chunk *next = chunk->hash_next;
            chunk_free(chunk);
            chunk = next;
        }
        world->hash_table[i] = NULL;
    }

    // Free active list
    if (world->active_chunks) {
        free(world->active_chunks);
        world->active_chunks = NULL;
    }
    world->active_count = 0;
    world->active_capacity = 0;
    world->chunk_count = 0;
}

// ============ CHUNK LOOKUP ============

Chunk* world_get_chunk(ChunkWorld *world, int cx, int cy, int cz) {
    uint32_t h = chunk_hash(cx, cy, cz);
    Chunk *chunk = world->hash_table[h];

    while (chunk) {
        if (chunk->cx == cx && chunk->cy == cy && chunk->cz == cz) {
            return chunk;
        }
        chunk = chunk->hash_next;
    }
    return NULL;
}

Chunk* world_get_or_create_chunk(ChunkWorld *world, int cx, int cy, int cz) {
    // Check if already exists
    Chunk *existing = world_get_chunk(world, cx, cy, cz);
    if (existing) return existing;

    // Create new chunk
    Chunk *chunk = chunk_create(cx, cy, cz);
    if (!chunk) return NULL;

    // Insert into hash table
    uint32_t h = chunk_hash(cx, cy, cz);
    chunk->hash_next = world->hash_table[h];
    world->hash_table[h] = chunk;
    world->chunk_count++;

    // Update neighbor pointers for this chunk and its neighbors
    world_update_chunk_neighbors(world, chunk);

    return chunk;
}

void world_update_chunk_neighbors(ChunkWorld *world, Chunk *chunk) {
    // Find and link all 6 neighbors
    for (int dir = 0; dir < DIR_COUNT; dir++) {
        int ncx = chunk->cx + DIR_DX[dir];
        int ncy = chunk->cy + DIR_DY[dir];
        int ncz = chunk->cz + DIR_DZ[dir];

        Chunk *neighbor = world_get_chunk(world, ncx, ncy, ncz);
        chunk->neighbors[dir] = neighbor;

        // Also update neighbor's pointer back to us
        if (neighbor) {
            neighbor->neighbors[DIR_OPPOSITE[dir]] = chunk;
        }
    }
}

// ============ CELL ACCESS ============

const Cell3D* world_get_cell(ChunkWorld *world, int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= WORLD_SIZE_CELLS ||
        y < 0 || y >= WORLD_SIZE_CELLS ||
        z < 0 || z >= WORLD_SIZE_CELLS) {
        return NULL;
    }

    int chunk_x, chunk_y, chunk_z;
    int local_x, local_y, local_z;
    cell_to_chunk(x, y, z, &chunk_x, &chunk_y, &chunk_z, &local_x, &local_y, &local_z);

    Chunk *chunk = world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) return NULL;

    return chunk_get_cell_const(chunk, local_x, local_y, local_z);
}

Cell3D* world_get_cell_for_write(ChunkWorld *world, int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= WORLD_SIZE_CELLS ||
        y < 0 || y >= WORLD_SIZE_CELLS ||
        z < 0 || z >= WORLD_SIZE_CELLS) {
        return NULL;
    }

    int chunk_x, chunk_y, chunk_z;
    int local_x, local_y, local_z;
    cell_to_chunk(x, y, z, &chunk_x, &chunk_y, &chunk_z, &local_x, &local_y, &local_z);

    // Get or create chunk
    Chunk *chunk = world_get_or_create_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) return NULL;

    return chunk_get_cell(chunk, local_x, local_y, local_z);
}

// ============ ACTIVE LIST MANAGEMENT ============

static void world_add_to_active_list(ChunkWorld *world, Chunk *chunk) {
    if (chunk->active_list_idx >= 0) return;  // Already in list

    // Expand if needed
    if (world->active_count >= world->active_capacity) {
        int new_capacity = world->active_capacity * 2;
        Chunk **new_list = (Chunk**)realloc(world->active_chunks,
                                             new_capacity * sizeof(Chunk*));
        if (!new_list) return;
        world->active_chunks = new_list;
        world->active_capacity = new_capacity;
    }

    // Mark chunk as active and reset stability
    chunk->is_active = true;
    chunk->is_stable = false;
    chunk->stable_frames = 0;

    chunk->active_list_idx = world->active_count;
    world->active_chunks[world->active_count++] = chunk;
}

void world_mark_cell_active(ChunkWorld *world, int x, int y, int z) {
    // Bounds check
    if (x < 0 || x >= WORLD_SIZE_CELLS ||
        y < 0 || y >= WORLD_SIZE_CELLS ||
        z < 0 || z >= WORLD_SIZE_CELLS) {
        return;
    }

    int chunk_x, chunk_y, chunk_z;
    int local_x, local_y, local_z;
    cell_to_chunk(x, y, z, &chunk_x, &chunk_y, &chunk_z, &local_x, &local_y, &local_z);

    Chunk *chunk = world_get_or_create_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) return;

    chunk_mark_dirty(chunk, local_x, local_y, local_z);
    world_add_to_active_list(world, chunk);
}

// ============ TERRAIN INITIALIZATION ============

// Calculate energy for material at temperature (single-phase: E = n * Cp * T)
static double calculate_energy_for_temperature(double moles, MaterialType type, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    return moles * props->molar_heat_capacity * temp_k;
}

void world_init_terrain(ChunkWorld *world, int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]) {
    world_init(world);

    // Create chunks with air (only where needed)
    // For terrain, create ground chunks

    for (int tz = 0; tz < TERRAIN_RESOLUTION; tz++) {
        for (int tx = 0; tx < TERRAIN_RESOLUTION; tx++) {
            int h = terrain_height[tz][tx];

            // Convert terrain grid coords to cell coords
            float world_x = tx * TERRAIN_SCALE;
            float world_z = tz * TERRAIN_SCALE;
            float world_y = h * TERRAIN_SCALE;

            int cx, cy, cz;
            world_to_cell(world_x, world_y, world_z, &cx, &cy, &cz);

            // Add dirt at surface
            Cell3D *cell = world_get_cell_for_write(world, cx, cy, cz);
            if (cell) {
                cell_init(cell);
                double dirt_moles = 50.0;
                double dirt_energy = calculate_energy_for_temperature(dirt_moles, MAT_DIRT, INITIAL_TEMP_K);
                cell_add_material(cell, MAT_DIRT, dirt_moles, dirt_energy);
            }

            // Add rock below (3 layers)
            for (int dy = 1; dy <= 3 && cy - dy >= 0; dy++) {
                Cell3D *rock_cell = world_get_cell_for_write(world, cx, cy - dy, cz);
                if (rock_cell) {
                    cell_init(rock_cell);
                    double rock_moles = 60.0;
                    double rock_energy = calculate_energy_for_temperature(rock_moles, MAT_ROCK, INITIAL_TEMP_K);
                    cell_add_material(rock_cell, MAT_ROCK, rock_moles, rock_energy);
                }
            }
        }
    }
}

// ============ TOOL APIs ============

void world_add_heat_at(ChunkWorld *world, float wx, float wy, float wz, double energy) {
    int x, y, z;
    world_to_cell(wx, wy, wz, &x, &y, &z);

    Cell3D *cell = world_get_cell_for_write(world, x, y, z);
    if (!cell || cell->present == 0) return;

    // Distribute heat proportionally by heat capacity
    double total_hc = 0;
    CELL_FOR_EACH_MATERIAL(cell, type) {
        double Cp = MATERIAL_PROPS[type].molar_heat_capacity;
        total_hc += cell->materials[type].moles * Cp;
    }

    if (total_hc > 0) {
        CELL_FOR_EACH_MATERIAL(cell, type) {
            double Cp = MATERIAL_PROPS[type].molar_heat_capacity;
            double fraction = (cell->materials[type].moles * Cp) / total_hc;
            cell->materials[type].thermal_energy += energy * fraction;
            if (cell->materials[type].thermal_energy < 0) {
                cell->materials[type].thermal_energy = 0;
            }
            material_invalidate_temp(&cell->materials[type]);
        }
    }

    // Mark active
    world_mark_cell_active(world, x, y, z);

    // Mark neighbors active
    for (int d = 0; d < DIR_COUNT; d++) {
        world_mark_cell_active(world, x + DIR_DX[d], y + DIR_DY[d], z + DIR_DZ[d]);
    }
}

void world_remove_heat_at(ChunkWorld *world, float wx, float wy, float wz, double energy) {
    world_add_heat_at(world, wx, wy, wz, -energy);
}

void world_add_water_at(ChunkWorld *world, float wx, float wy, float wz, double moles) {
    int x, y, z;
    world_to_cell(wx, wy, wz, &x, &y, &z);

    Cell3D *cell = world_get_cell_for_write(world, x, y, z);
    if (!cell) return;

    double energy = calculate_energy_for_temperature(moles, MAT_WATER, INITIAL_TEMP_K);
    cell_add_material(cell, MAT_WATER, moles, energy);

    // Mark active
    world_mark_cell_active(world, x, y, z);

    // Mark neighbors active
    for (int d = 0; d < DIR_COUNT; d++) {
        world_mark_cell_active(world, x + DIR_DX[d], y + DIR_DY[d], z + DIR_DZ[d]);
    }
}

CellInfo world_get_cell_info(ChunkWorld *world, float wx, float wy, float wz) {
    CellInfo info = {0};

    world_to_cell(wx, wy, wz, &info.cell_x, &info.cell_y, &info.cell_z);

    // Need non-const access for temperature caching
    Cell3D *cell = world_get_cell_for_write(world, info.cell_x, info.cell_y, info.cell_z);
    if (!cell) {
        info.valid = false;
        return info;
    }

    info.valid = true;
    info.material_count = CELL_MATERIAL_COUNT(cell);

    if (cell->present != 0) {
        double max_moles = 0;
        MaterialType primary_type = MAT_NONE;

        CELL_FOR_EACH_MATERIAL(cell, type) {
            if (cell->materials[type].moles > max_moles) {
                max_moles = cell->materials[type].moles;
                primary_type = type;
            }
        }

        info.primary_material = primary_type;
        info.temperature = cell_get_temperature(cell);

        if (primary_type != MAT_NONE) {
            info.primary_phase = MATERIAL_PROPS[primary_type].phase;
        }
    }

    return info;
}

// ============ DEBUG METRICS ============

#ifdef DEBUG_METRICS
void world_update_debug_metrics(ChunkWorld *world) {
    uint64_t cells = 0;
    uint64_t materials = 0;

    // Count cells and materials across all chunks
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = world->hash_table[h];
        while (chunk) {
            for (int i = 0; i < CHUNK_VOLUME; i++) {
                Cell3D *cell = &chunk->cells[i];
                if (cell->present != 0) {
                    cells++;
                    materials += CELL_MATERIAL_COUNT(cell);
                }
            }
            chunk = chunk->hash_next;
        }
    }

    // Estimate memory usage
    uint64_t mem_kb = (world->chunk_count * sizeof(Chunk)) / 1024;
    mem_kb += (world->active_capacity * sizeof(Chunk*)) / 1024;

    DEBUG_METRICS_UPDATE_MEMORY(cells, materials, mem_kb);
}
#endif
