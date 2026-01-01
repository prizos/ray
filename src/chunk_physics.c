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

            double hc_i = cell->materials[type_i].moles * MATERIAL_PROPS[type_i].molar_heat_capacity;
            double hc_j = cell->materials[type_j].moles * MATERIAL_PROPS[type_j].molar_heat_capacity;

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
            double Cp = MATERIAL_PROPS[type].molar_heat_capacity;
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
                double Cp = MATERIAL_PROPS[ntype].molar_heat_capacity;
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
                              MATERIAL_PROPS[ctype].molar_heat_capacity) / cell_hc;
            cell->materials[ctype].thermal_energy -= heat_flow * fraction;
            material_invalidate_temp(&cell->materials[ctype]);
        }

        CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
            double fraction = (neighbor->materials[ntype].moles *
                              MATERIAL_PROPS[ntype].molar_heat_capacity) / neighbor_hc;
            neighbor->materials[ntype].thermal_energy += heat_flow * fraction;
            material_invalidate_temp(&neighbor->materials[ntype]);
        }

        // Mark both cells active
        world_mark_cell_active(world, gx, gy, gz);
        world_mark_cell_active(world, gx + DIR_DX[dir], gy + DIR_DY[dir], gz + DIR_DZ[dir]);
    }
}

// ============ LIQUID FLOW ============

// Flow rates - derived from physics but scaled for game feel
#define GRAVITY_FLOW_RATE 0.3       // Base rate for gravity-driven flow
#define HORIZONTAL_FLOW_RATE 0.2    // Rate for horizontal equalization
#define DISPLACEMENT_RATE 0.25      // Rate for density-based swapping

// Execute volume-balanced material swap between cells
// Physical model: equal volumes are exchanged - liquid volume going down
// equals gas volume going up, conserving total cell volume occupancy.
static void execute_material_swap(Cell3D *upper, Cell3D *lower,
                                   MaterialType sink_type, MaterialType rise_type,
                                   double dt) {
    // Get moles available for swapping
    double sink_moles = upper->materials[sink_type].moles;
    double rise_moles = lower->materials[rise_type].moles;

    if (sink_moles < MOLES_EPSILON || rise_moles < MOLES_EPSILON) return;

    // Get molar volumes
    double sink_mol_vol = MATERIAL_PROPS[sink_type].molar_volume;
    double rise_mol_vol = MATERIAL_PROPS[rise_type].molar_volume;

    if (sink_mol_vol < 1e-10 || rise_mol_vol < 1e-10) return;

    // Calculate volumes available
    double sink_volume = sink_moles * sink_mol_vol;
    double rise_volume = rise_moles * rise_mol_vol;

    // Calculate swap rate based on density difference (bigger diff = faster swap)
    double sink_dens = material_get_density(sink_type);
    double rise_dens = material_get_density(rise_type);
    if (rise_dens < 0.001) rise_dens = 0.001;

    double density_ratio = sink_dens / rise_dens;
    double swap_efficiency = fmin(1.0, (density_ratio - 1.0) * 0.5);
    double swap_rate = swap_efficiency * DISPLACEMENT_RATE * dt * 60.0;

    // The swap volume is limited by:
    // 1. How much sinking material wants to move (sink_volume * swap_rate)
    // 2. How much rising material is available to displace (rise_volume)
    double swap_volume = fmin(sink_volume * swap_rate, rise_volume);

    // Also limit by available sinking material
    swap_volume = fmin(swap_volume, sink_volume);

    if (swap_volume < 1e-12) return;

    // Calculate moles to transfer based on volume
    double swap_sink_moles = swap_volume / sink_mol_vol;
    double swap_rise_moles = swap_volume / rise_mol_vol;

    if (swap_sink_moles < MOLES_EPSILON || swap_rise_moles < MOLES_EPSILON) return;

    // Calculate energy per mole (conserve energy)
    double sink_energy_per_mole = upper->materials[sink_type].thermal_energy /
                                   upper->materials[sink_type].moles;
    double rise_energy_per_mole = lower->materials[rise_type].thermal_energy /
                                   lower->materials[rise_type].moles;

    double sink_energy = swap_sink_moles * sink_energy_per_mole;
    double rise_energy = swap_rise_moles * rise_energy_per_mole;

    // Execute transfer (conserves moles and energy for each material type)
    // Move sinking material down
    upper->materials[sink_type].moles -= swap_sink_moles;
    upper->materials[sink_type].thermal_energy -= sink_energy;
    cell_add_material(lower, sink_type, swap_sink_moles, sink_energy);

    // Move rising material up
    lower->materials[rise_type].moles -= swap_rise_moles;
    lower->materials[rise_type].thermal_energy -= rise_energy;
    cell_add_material(upper, rise_type, swap_rise_moles, rise_energy);

    // Invalidate temperature caches
    material_invalidate_temp(&upper->materials[sink_type]);
    material_invalidate_temp(&lower->materials[rise_type]);
    if (CELL_HAS_MATERIAL(lower, sink_type))
        material_invalidate_temp(&lower->materials[sink_type]);
    if (CELL_HAS_MATERIAL(upper, rise_type))
        material_invalidate_temp(&upper->materials[rise_type]);

    // Cleanup empty materials
    if (upper->materials[sink_type].moles < MOLES_EPSILON)
        cell_remove_material(upper, sink_type);
    if (lower->materials[rise_type].moles < MOLES_EPSILON)
        cell_remove_material(lower, rise_type);
}

static void process_cell_liquid_flow(ChunkWorld *world, Chunk *chunk,
                                      int lx, int ly, int lz, double dt) {
    Cell3D *cell = chunk_get_cell(chunk, lx, ly, lz);
    if (cell->present == 0) return;

    int gx = chunk->cx * CHUNK_SIZE + lx;
    int gy = chunk->cy * CHUNK_SIZE + ly;
    int gz = chunk->cz * CHUNK_SIZE + lz;

    // Check each liquid material
    CELL_FOR_EACH_MATERIAL(cell, type) {
        Phase phase = MATERIAL_PROPS[type].phase;
        if (phase != PHASE_LIQUID) continue;

        double available_moles = cell->materials[type].moles;
        if (available_moles < MOLES_EPSILON) continue;

        // Get our density
        double our_density = material_get_density(type);

        bool blocked_below = false;

        // Check cell below - NULL neighbors (chunk boundaries) act as barriers
        Cell3D *below = chunk_get_neighbor_cell(chunk, lx, ly, lz, DIR_NEG_Y);
        if (!below) {
            blocked_below = true;
        } else {
            // Find what's below and if we can displace it
            MaterialType displace_type = MAT_NONE;
            double below_density = 0;
            bool has_solid_below = false;

            CELL_FOR_EACH_MATERIAL(below, btype) {
                Phase bphase = MATERIAL_PROPS[btype].phase;

                if (bphase == PHASE_SOLID) {
                    has_solid_below = true;
                    break;
                }

                double bdens = material_get_density(btype);

                // Can displace if we're denser and it's a fluid
                if ((bphase == PHASE_GAS || bphase == PHASE_LIQUID) && our_density > bdens) {
                    // Pick the lightest material to displace
                    if (displace_type == MAT_NONE || bdens < below_density) {
                        displace_type = btype;
                        below_density = bdens;
                    }
                }
            }

            if (has_solid_below) {
                blocked_below = true;
            } else {
                // Determine if we can flow down and how
                // Cases:
                // 1. below is empty -> free flow
                // 2. below has only gas -> flow through it (gas rises naturally)
                // 3. below has same liquid -> flow/merge
                // 4. below has lighter liquid -> displace it
                // 5. below has denser liquid -> blocked (float on top)

                bool can_flow = false;
                bool below_has_same_liquid = CELL_HAS_MATERIAL(below, type);
                bool below_has_only_gas = true;
                bool below_has_denser_liquid = false;

                CELL_FOR_EACH_MATERIAL(below, check_type) {
                    Phase check_phase = MATERIAL_PROPS[check_type].phase;
                    if (check_phase == PHASE_LIQUID) {
                        below_has_only_gas = false;
                        if (check_type != type) {
                            double check_dens = material_get_density(check_type);
                            if (check_dens >= our_density) {
                                below_has_denser_liquid = true;
                            }
                        }
                    }
                }

                if (below->present == 0 || below_has_only_gas || below_has_same_liquid) {
                    // Free flow: empty, gas-only, or same liquid below
                    can_flow = true;
                } else if (displace_type != MAT_NONE && !below_has_denser_liquid) {
                    // Has lighter material to displace
                    can_flow = true;
                }

                if (can_flow) {
                    // Use density-based swap if there's lighter material below
                    if (displace_type != MAT_NONE && CELL_HAS_MATERIAL(below, displace_type)) {
                        // SWAP: liquid sinks, lighter material rises
                        execute_material_swap(cell, below, type, displace_type, dt);
                        world_mark_cell_active(world, gx, gy, gz);
                        world_mark_cell_active(world, gx, gy - 1, gz);
                        // Update available moles after swap
                        available_moles = CELL_HAS_MATERIAL(cell, type) ?
                                          cell->materials[type].moles : 0;
                    } else {
                        // Free flow into empty/compatible cell (no swap needed)
                        double flow_moles = available_moles * GRAVITY_FLOW_RATE * dt * 60.0;
                        if (flow_moles > available_moles) flow_moles = available_moles;

                        if (flow_moles >= MOLES_EPSILON) {
                            double energy_per_mole = cell->materials[type].thermal_energy /
                                                     cell->materials[type].moles;
                            double flow_energy = flow_moles * energy_per_mole;

                            cell->materials[type].moles -= flow_moles;
                            cell->materials[type].thermal_energy -= flow_energy;
                            material_invalidate_temp(&cell->materials[type]);
                            available_moles = cell->materials[type].moles;

                            if (available_moles < MOLES_EPSILON) {
                                cell_remove_material(cell, type);
                                available_moles = 0;
                            }

                            cell_add_material(below, type, flow_moles, flow_energy);

                            world_mark_cell_active(world, gx, gy - 1, gz);
                            if (available_moles > MOLES_EPSILON) {
                                world_mark_cell_active(world, gx, gy, gz);
                            }
                        }
                    }
                } else {
                    // Blocked: denser liquid below
                    blocked_below = true;
                }
            }
        }

        // Horizontal spreading: equalize with neighbors when blocked below
        if (available_moles >= MOLES_EPSILON && blocked_below) {
            static const int h_dirs[] = {DIR_POS_X, DIR_NEG_X, DIR_POS_Z, DIR_NEG_Z};
            int open_sides = 0;      // Count of sides where water can spread
            int blocked_sides = 0;   // Count of sides blocked by solid

            // First pass: check horizontal neighbors
            for (int i = 0; i < 4; i++) {
                int dir = h_dirs[i];
                Cell3D *neighbor = chunk_get_neighbor_cell(chunk, lx, ly, lz, dir);
                if (!neighbor) { open_sides++; continue; }

                // Check for solid wall
                bool has_solid = false;
                CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
                    Phase nphase = MATERIAL_PROPS[ntype].phase;
                    if (nphase == PHASE_SOLID) {
                        has_solid = true;
                        break;
                    }
                }
                if (has_solid) {
                    blocked_sides++;
                    continue;
                }

                // Check if neighbor is supported (floor or liquid below)
                int gnx = gx + DIR_DX[dir];
                int gnz = gz + DIR_DZ[dir];
                const Cell3D *nb = world_get_cell(world, gnx, gy - 1, gnz);
                bool neighbor_supported = false;

                if (!nb) {
                    neighbor_supported = true;
                } else if (nb->present == 0) {
                    neighbor_supported = CELL_HAS_MATERIAL(neighbor, type);
                } else {
                    CELL_FOR_EACH_MATERIAL(nb, nbtype) {
                        Phase nbphase = MATERIAL_PROPS[nbtype].phase;
                        if (nbphase == PHASE_SOLID || nbphase == PHASE_LIQUID) {
                            neighbor_supported = true;
                            break;
                        }
                    }
                }

                if (neighbor_supported) {
                    open_sides++;  // Can spread this direction
                }
            }

            // Second pass: do horizontal spreading
            for (int i = 0; i < 4; i++) {
                int dir = h_dirs[i];

                Cell3D *neighbor = chunk_get_neighbor_cell(chunk, lx, ly, lz, dir);
                if (!neighbor) continue;

                // Check neighbor doesn't have solid blocking
                bool blocked = false;
                CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
                    Phase nphase = MATERIAL_PROPS[ntype].phase;
                    if (nphase == PHASE_SOLID) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) continue;

                // Check neighbor is supported
                int gnx = gx + DIR_DX[dir];
                int gnz = gz + DIR_DZ[dir];
                const Cell3D *nb = world_get_cell(world, gnx, gy - 1, gnz);
                bool neighbor_supported = false;

                if (!nb) {
                    neighbor_supported = true;
                } else if (nb->present == 0) {
                    neighbor_supported = CELL_HAS_MATERIAL(neighbor, type);
                } else {
                    CELL_FOR_EACH_MATERIAL(nb, nbtype) {
                        Phase nbphase = MATERIAL_PROPS[nbtype].phase;
                        if (nbphase == PHASE_SOLID || nbphase == PHASE_LIQUID) {
                            neighbor_supported = true;
                            break;
                        }
                    }
                }

                if (!neighbor_supported) continue;

                // Get neighbor's liquid
                double neighbor_moles = 0;
                if (CELL_HAS_MATERIAL(neighbor, type)) {
                    neighbor_moles = neighbor->materials[type].moles;
                }

                double gradient = available_moles - neighbor_moles;
                if (gradient <= MOLES_EPSILON) continue;

                double flow_moles = gradient * HORIZONTAL_FLOW_RATE * dt * 60.0 / 4.0;
                if (flow_moles < MOLES_EPSILON) continue;
                if (flow_moles > available_moles * 0.25) flow_moles = available_moles * 0.25;

                double energy_per_mole = cell->materials[type].thermal_energy /
                                         cell->materials[type].moles;
                double flow_energy = flow_moles * energy_per_mole;

                cell->materials[type].moles -= flow_moles;
                cell->materials[type].thermal_energy -= flow_energy;
                material_invalidate_temp(&cell->materials[type]);
                available_moles = cell->materials[type].moles;

                if (available_moles < MOLES_EPSILON) {
                    cell_remove_material(cell, type);
                    available_moles = 0;
                }

                cell_add_material(neighbor, type, flow_moles, flow_energy);

                world_mark_cell_active(world, gx + DIR_DX[dir], gy, gz + DIR_DZ[dir]);
                if (available_moles > MOLES_EPSILON) {
                    world_mark_cell_active(world, gx, gy, gz);
                }

                if (available_moles < MOLES_EPSILON) break;
            }

            // Upward flow: spread up when horizontal neighbors are nearly equalized
            // This allows containers to fill from bottom up (both vacuum and enclosed)
            if (available_moles > 10.0) {
                // Check that horizontal neighbors have similar water levels (equilibrium)
                bool at_equilibrium = true;
                double min_neighbor = available_moles;
                for (int i = 0; i < 4; i++) {
                    int dir = h_dirs[i];
                    Cell3D *neighbor = chunk_get_neighbor_cell(chunk, lx, ly, lz, dir);
                    if (!neighbor) continue;

                    bool has_solid = false;
                    CELL_FOR_EACH_MATERIAL(neighbor, ntype) {
                        Phase nphase = MATERIAL_PROPS[ntype].phase;
                        if (nphase == PHASE_SOLID) {
                            has_solid = true;
                            break;
                        }
                    }
                    if (has_solid) continue;  // Wall, skip

                    double neighbor_moles = CELL_HAS_MATERIAL(neighbor, type) ?
                                           neighbor->materials[type].moles : 0;
                    if (neighbor_moles < min_neighbor) min_neighbor = neighbor_moles;
                    if (available_moles - neighbor_moles > 2.0) {
                        at_equilibrium = false;  // Not equalized yet
                    }
                }

                if (at_equilibrium && min_neighbor > 5.0) {
                    Cell3D *above = chunk_get_neighbor_cell(chunk, lx, ly, lz, DIR_POS_Y);
                    if (above) {
                        bool above_blocked = false;
                        CELL_FOR_EACH_MATERIAL(above, atype) {
                            Phase aphase = MATERIAL_PROPS[atype].phase;
                            if (aphase == PHASE_SOLID) {
                                above_blocked = true;
                                break;
                            }
                        }

                        if (!above_blocked) {
                            double above_moles = CELL_HAS_MATERIAL(above, type) ?
                                                 above->materials[type].moles : 0;
                            double gradient = available_moles - above_moles - 5.0;

                            if (gradient > MOLES_EPSILON) {
                                double flow_moles = gradient * HORIZONTAL_FLOW_RATE * 0.5 * dt * 60.0;
                                if (flow_moles > available_moles * 0.2) flow_moles = available_moles * 0.2;

                                if (flow_moles >= MOLES_EPSILON) {
                                    double energy_per_mole = cell->materials[type].thermal_energy /
                                                             cell->materials[type].moles;
                                    double flow_energy = flow_moles * energy_per_mole;

                                    cell->materials[type].moles -= flow_moles;
                                    cell->materials[type].thermal_energy -= flow_energy;
                                    material_invalidate_temp(&cell->materials[type]);

                                    if (cell->materials[type].moles < MOLES_EPSILON) {
                                        cell_remove_material(cell, type);
                                    }

                                    cell_add_material(above, type, flow_moles, flow_energy);

                                    world_mark_cell_active(world, gx, gy + 1, gz);
                                    world_mark_cell_active(world, gx, gy, gz);
                                }
                            }
                        }
                    }
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
        Phase phase = MATERIAL_PROPS[type].phase;
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
                Phase nphase = MATERIAL_PROPS[ntype].phase;
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

    // Second pass: phase transitions (must happen after heat, before flow)
    if (flags & PHYSICS_PHASE_TRANS) {
        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    Cell3D *cell = chunk_get_cell(chunk, x, y, z);
                    if (cell->present == 0) continue;

                    // Check each material for phase transition
                    // Use bitmask iteration to avoid issues with modifying during iteration
                    uint32_t present_copy = cell->present;
                    for (MaterialType type = (MaterialType)1; type < MAT_COUNT; type = (MaterialType)(type + 1)) {
                        if (!(present_copy & (1u << type))) continue;
                        if (!CELL_HAS_MATERIAL(cell, type)) continue;

                        double temp = material_get_temperature(&cell->materials[type], type);
                        MaterialType new_type = material_check_transition(type, temp);

                        if (new_type != MAT_NONE) {
                            material_convert_phase(cell, type, new_type);
                            // Mark cell and chunk as active
                            int gx = chunk->cx * CHUNK_SIZE + x;
                            int gy = chunk->cy * CHUNK_SIZE + y;
                            int gz = chunk->cz * CHUNK_SIZE + z;
                            world_mark_cell_active(world, gx, gy, gz);
                        }
                    }
                }
            }
        }
    }

    // Third pass: liquid flow
    if (flags & PHYSICS_LIQUID_FLOW) {
        for (int z = z0; z <= z1; z++) {
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    process_cell_liquid_flow(world, chunk, x, y, z, dt);
                }
            }
        }
    }

    // Fourth pass: gas diffusion
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
