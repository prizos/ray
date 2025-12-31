#include "chunk.h"
#include <stdio.h>
#include <string.h>

// ============ PHYSICS CONSTANTS ============

#define PHYSICS_DT 0.016f           // Fixed timestep (60 FPS)
#define HEAT_TRANSFER_RATE 0.1      // Rate of heat transfer between cells
#define INTERNAL_EQUIL_RATE 0.5     // Rate of internal equilibration
#define WATER_FLOW_RATE 0.2         // Rate of water flow between cells
#define GAS_DIFFUSION_RATE 0.05     // Rate of gas diffusion

// ============ INTERNAL EQUILIBRATION ============

static void cell_internal_equilibration(Cell3D *cell, double dt) {
    if (CELL_MATERIAL_COUNT(cell) < 2) return;

    // Each pair of materials exchanges heat
    for (MaterialType type_i = (MaterialType)1; type_i < MAT_COUNT; type_i = (MaterialType)(type_i + 1)) {
        if (!CELL_HAS_MATERIAL(cell, type_i)) continue;

        for (MaterialType type_j = (MaterialType)(type_i + 1); type_j < MAT_COUNT; type_j = (MaterialType)(type_j + 1)) {
            if (!CELL_HAS_MATERIAL(cell, type_j)) continue;

            double T_i = material_get_temperature(&cell->materials[type_i], type_i);
            double T_j = material_get_temperature(&cell->materials[type_j], type_j);
            double dT = T_i - T_j;

            if (fabs(dT) < TEMP_EPSILON) continue;

            double k_i = MATERIAL_PROPS[type_i].thermal_conductivity;
            double k_j = MATERIAL_PROPS[type_j].thermal_conductivity;
            double k_eff = (k_i > 0 && k_j > 0) ? sqrt(k_i * k_j) : (k_i + k_j) / 2;

            double heat_transfer = k_eff * dT * dt * INTERNAL_EQUIL_RATE;

            double hc_i = cell->materials[type_i].moles * get_effective_heat_capacity(&cell->materials[type_i], type_i);
            double hc_j = cell->materials[type_j].moles * get_effective_heat_capacity(&cell->materials[type_j], type_j);

            if (hc_i > 0 && hc_j > 0) {
                double max_transfer = fabs(dT) * hc_i * hc_j / (hc_i + hc_j);
                if (fabs(heat_transfer) > max_transfer) {
                    heat_transfer = (heat_transfer > 0) ? max_transfer : -max_transfer;
                }

                cell->materials[type_i].thermal_energy -= heat_transfer;
                cell->materials[type_j].thermal_energy += heat_transfer;
                material_invalidate_temp(&cell->materials[type_i]);
                material_invalidate_temp(&cell->materials[type_j]);
            }
        }
    }
}

// ============ HEAT CONDUCTION ============

static void process_cell_heat_conduction(ChunkWorld *world, Chunk *chunk,
                                          int lx, int ly, int lz, double dt) {
    Cell3D *cell = chunk_get_cell(chunk, lx, ly, lz);
    if (cell->present == 0) return;

    // Calculate cell properties
    int cell_mat_count = CELL_MATERIAL_COUNT(cell);
    if (cell_mat_count == 0) return;

    double cell_hc = 0;
    double cell_temp = 0;
    {
        double weighted_temp = 0;
        CELL_FOR_EACH_MATERIAL(cell, type) {
            double Cp = get_effective_heat_capacity(&cell->materials[type], type);
            double hc = cell->materials[type].moles * Cp;
            cell_hc += hc;
            weighted_temp += material_get_temperature(&cell->materials[type], type) * hc;
        }
        if (cell_hc > 0) cell_temp = weighted_temp / cell_hc;
    }

    if (cell_hc < 1e-10) return;

    // Get global cell coordinates for marking active
    int gx = chunk->cx * CHUNK_SIZE + lx;
    int gy = chunk->cy * CHUNK_SIZE + ly;
    int gz = chunk->cz * CHUNK_SIZE + lz;

    // Check all 6 neighbors
    for (int dir = 0; dir < DIR_COUNT; dir++) {
        Cell3D *neighbor = chunk_get_neighbor_cell(chunk, lx, ly, lz, dir);
        if (!neighbor || neighbor->present == 0) continue;

        // Calculate neighbor properties
        int neighbor_mat_count = CELL_MATERIAL_COUNT(neighbor);
        if (neighbor_mat_count == 0) continue;

        double neighbor_hc = 0;
        double neighbor_temp = 0;
        {
            double weighted_temp = 0;
            CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
                double Cp = get_effective_heat_capacity(&neighbor->materials[ntype], ntype);
                double hc = neighbor->materials[ntype].moles * Cp;
                neighbor_hc += hc;
                weighted_temp += material_get_temperature(&neighbor->materials[ntype], ntype) * hc;
            }
            if (neighbor_hc > 0) neighbor_temp = weighted_temp / neighbor_hc;
        }

        if (neighbor_hc < 1e-10) continue;

        // Temperature difference
        double dT = cell_temp - neighbor_temp;
        if (fabs(dT) < 0.01) continue;

        // Calculate thermal conductivity
        double k_cell = 0, k_neighbor = 0;
        CELL_FOR_EACH_MATERIAL(cell, ctype) {
            k_cell += MATERIAL_PROPS[ctype].thermal_conductivity;
        }
        k_cell /= cell_mat_count;

        CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
            k_neighbor += MATERIAL_PROPS[ntype].thermal_conductivity;
        }
        k_neighbor /= neighbor_mat_count;

        double k_eff = (k_cell > 0 && k_neighbor > 0) ?
                       2 * k_cell * k_neighbor / (k_cell + k_neighbor) :
                       (k_cell + k_neighbor) / 2;

        // Heat flow
        double heat_flow = k_eff * dT * dt * HEAT_TRANSFER_RATE;

        // Limit to not overshoot
        double max_flow = dT * cell_hc * neighbor_hc / (cell_hc + neighbor_hc);
        if (heat_flow > max_flow) heat_flow = max_flow;

        if (heat_flow < 1e-6) continue;

        // Transfer heat
        CELL_FOR_EACH_MATERIAL(cell, ctype) {
            double fraction = (cell->materials[ctype].moles *
                              get_effective_heat_capacity(&cell->materials[ctype], ctype)) / cell_hc;
            cell->materials[ctype].thermal_energy -= heat_flow * fraction;
            material_invalidate_temp(&cell->materials[ctype]);
        }

        CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
            double fraction = (neighbor->materials[ntype].moles *
                              get_effective_heat_capacity(&neighbor->materials[ntype], ntype)) / neighbor_hc;
            neighbor->materials[ntype].thermal_energy += heat_flow * fraction;
            material_invalidate_temp(&neighbor->materials[ntype]);
        }

        // Mark both cells active
        world_mark_cell_active(world, gx, gy, gz);
        world_mark_cell_active(world, gx + DIR_DX[dir], gy + DIR_DY[dir], gz + DIR_DZ[dir]);
    }
}

// ============ LIQUID FLOW ============

static void process_cell_liquid_flow(ChunkWorld *world, Chunk *chunk,
                                      int lx, int ly, int lz, double dt) {
    Cell3D *cell = chunk_get_cell(chunk, lx, ly, lz);
    if (cell->present == 0) return;

    int gx = chunk->cx * CHUNK_SIZE + lx;
    int gy = chunk->cy * CHUNK_SIZE + ly;
    int gz = chunk->cz * CHUNK_SIZE + lz;

    // Check each liquid material
    CELL_FOR_EACH_MATERIAL(cell, type) {
        Phase phase = material_get_phase_from_energy(&cell->materials[type], type);
        if (phase != PHASE_LIQUID) continue;

        double available_moles = cell->materials[type].moles;
        if (available_moles < MOLES_EPSILON) continue;

        // Try to flow down first (gravity)
        Cell3D *below = chunk_get_neighbor_cell(chunk, lx, ly, lz, DIR_NEG_Y);
        if (below) {
            // Check if below is not solid
            bool can_flow_down = true;
            CELL_FOR_EACH_MATERIAL(below, btype) {
                Phase bphase = material_get_phase_from_energy(&below->materials[btype], btype);
                if (bphase == PHASE_SOLID) {
                    can_flow_down = false;
                    break;
                }
            }

            if (can_flow_down) {
                double flow_moles = available_moles * WATER_FLOW_RATE * dt * 60.0;
                if (flow_moles > available_moles) flow_moles = available_moles;
                if (flow_moles < MOLES_EPSILON) continue;

                double energy_per_mole = cell->materials[type].thermal_energy / cell->materials[type].moles;
                double flow_energy = flow_moles * energy_per_mole;

                cell->materials[type].moles -= flow_moles;
                cell->materials[type].thermal_energy -= flow_energy;
                material_invalidate_temp(&cell->materials[type]);

                if (cell->materials[type].moles < MOLES_EPSILON) {
                    cell_remove_material(cell, type);
                }

                cell_add_material(below, type, flow_moles, flow_energy);

                world_mark_cell_active(world, gx, gy - 1, gz);
                if (cell->materials[type].moles > MOLES_EPSILON) {
                    world_mark_cell_active(world, gx, gy, gz);
                }
            }
        }
    }
}

// ============ GAS DIFFUSION ============

static void process_cell_gas_diffusion(ChunkWorld *world, Chunk *chunk,
                                        int lx, int ly, int lz, double dt) {
    Cell3D *cell = chunk_get_cell(chunk, lx, ly, lz);
    if (cell->present == 0) return;

    int gx = chunk->cx * CHUNK_SIZE + lx;
    int gy = chunk->cy * CHUNK_SIZE + ly;
    int gz = chunk->cz * CHUNK_SIZE + lz;

    CELL_FOR_EACH_MATERIAL(cell, type) {
        Phase phase = material_get_phase_from_energy(&cell->materials[type], type);
        if (phase != PHASE_GAS) continue;

        double available_moles = cell->materials[type].moles;
        if (available_moles < MOLES_EPSILON) continue;

        // Diffuse to all 6 neighbors
        for (int dir = 0; dir < DIR_COUNT; dir++) {
            Cell3D *neighbor = chunk_get_neighbor_cell(chunk, lx, ly, lz, dir);
            if (!neighbor) continue;

            // Check neighbor doesn't have solid blocking
            bool blocked = false;
            CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
                Phase nphase = material_get_phase_from_energy(&neighbor->materials[ntype], ntype);
                if (nphase == PHASE_SOLID) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) continue;

            // Get neighbor's gas of same type
            double neighbor_moles = 0;
            if (CELL_HAS_MATERIAL(neighbor, type)) {
                neighbor_moles = neighbor->materials[type].moles;
            }

            // Diffuse based on concentration gradient
            double gradient = available_moles - neighbor_moles;
            if (gradient <= 0) continue;

            // Bias upward for buoyancy (hot gas rises)
            double bias = 1.0;
            if (dir == DIR_POS_Y) bias = 1.5;  // Up
            else if (dir == DIR_NEG_Y) bias = 0.5;  // Down

            double flow_moles = gradient * GAS_DIFFUSION_RATE * bias * dt * 60.0 / 6.0;
            if (flow_moles < MOLES_EPSILON) continue;
            if (flow_moles > available_moles * 0.1) flow_moles = available_moles * 0.1;

            double energy_per_mole = cell->materials[type].thermal_energy / cell->materials[type].moles;
            double flow_energy = flow_moles * energy_per_mole;

            cell->materials[type].moles -= flow_moles;
            cell->materials[type].thermal_energy -= flow_energy;
            material_invalidate_temp(&cell->materials[type]);

            if (cell->materials[type].moles < MOLES_EPSILON) {
                cell_remove_material(cell, type);
            }

            cell_add_material(neighbor, type, flow_moles, flow_energy);

            world_mark_cell_active(world, gx + DIR_DX[dir], gy + DIR_DY[dir], gz + DIR_DZ[dir]);
            if (cell->materials[type].moles > MOLES_EPSILON) {
                world_mark_cell_active(world, gx, gy, gz);
            }
        }
    }
}

// ============ CHUNK PHYSICS STEP ============

static void chunk_physics_step_flags(ChunkWorld *world, Chunk *chunk, double dt, PhysicsFlags flags) {
    // Note: is_active tracks whether material moved THIS frame (set during processing).
    // We don't check it here - being in snapshot means we should process.
    // is_stable is set after many frames with no activity.
    if (chunk->is_stable) return;
    if (flags == PHYSICS_NONE) return;

    // Expand dirty region by 1 for neighbor interactions
    int x0 = (chunk->dirty_min_x > 0) ? chunk->dirty_min_x - 1 : 0;
    int x1 = (chunk->dirty_max_x < CHUNK_SIZE - 1) ? chunk->dirty_max_x + 1 : CHUNK_SIZE - 1;
    int y0 = (chunk->dirty_min_y > 0) ? chunk->dirty_min_y - 1 : 0;
    int y1 = (chunk->dirty_max_y < CHUNK_SIZE - 1) ? chunk->dirty_max_y + 1 : CHUNK_SIZE - 1;
    int z0 = (chunk->dirty_min_z > 0) ? chunk->dirty_min_z - 1 : 0;
    int z1 = (chunk->dirty_max_z < CHUNK_SIZE - 1) ? chunk->dirty_max_z + 1 : CHUNK_SIZE - 1;

    // First pass: heat systems
    if (flags & PHYSICS_HEAT_ALL) {
        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    Cell3D *cell = chunk_get_cell(chunk, x, y, z);

                    // Internal equilibration
                    if (flags & PHYSICS_HEAT_INTERNAL) {
                        cell_internal_equilibration(cell, dt);
                    }

                    // Heat conduction
                    if (flags & PHYSICS_HEAT_CONDUCT) {
                        process_cell_heat_conduction(world, chunk, x, y, z, dt);
                    }
                }
            }
        }
    }

    // Second pass: liquid flow
    if (flags & PHYSICS_LIQUID_FLOW) {
        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    process_cell_liquid_flow(world, chunk, x, y, z, dt);
                }
            }
        }
    }

    // Third pass: gas diffusion
    if (flags & PHYSICS_GAS_DIFFUSE) {
        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    process_cell_gas_diffusion(world, chunk, x, y, z, dt);
                }
            }
        }
    }
}

// ============ WORLD PHYSICS STEP ============

void world_physics_step_flags(ChunkWorld *world, float dt, PhysicsFlags flags) {
    if (flags == PHYSICS_NONE) return;

    world->accumulator += dt;

    while (world->accumulator >= PHYSICS_DT) {
        world->accumulator -= PHYSICS_DT;
        world->tick++;

        if (world->active_count == 0) continue;

        // Take snapshot of active chunks (list may change during processing)
        int snapshot_count = world->active_count;
        Chunk **snapshot = (Chunk**)malloc(sizeof(Chunk*) * snapshot_count);
        if (!snapshot) continue;
        memcpy(snapshot, world->active_chunks, sizeof(Chunk*) * snapshot_count);

        // Reset active list and is_active flag for re-marking
        // is_active will be set true again by world_mark_cell_active if material moves
        for (int i = 0; i < snapshot_count; i++) {
            if (snapshot[i]) {
                snapshot[i]->active_list_idx = -1;
                snapshot[i]->is_active = false;
            }
        }
        world->active_count = 0;

        // Process each chunk with specified flags
        for (int i = 0; i < snapshot_count; i++) {
            Chunk *chunk = snapshot[i];
            if (!chunk) continue;

            chunk_physics_step_flags(world, chunk, PHYSICS_DT, flags);
        }

        // Check equilibrium and reset dirty regions only for inactive chunks
        for (int i = 0; i < snapshot_count; i++) {
            Chunk *chunk = snapshot[i];
            if (!chunk) continue;

            chunk_check_equilibrium(chunk);

            // Only reset dirty region if nothing changed this frame.
            // If is_active is true (material moved), keep the dirty region
            // so the affected cells are processed next frame.
            if (!chunk->is_active) {
                chunk_reset_dirty(chunk);
            }
        }

        free(snapshot);
    }
}

// Convenience wrapper - runs all physics systems
void world_physics_step(ChunkWorld *world, float dt) {
    world_physics_step_flags(world, dt, PHYSICS_ALL);
}
