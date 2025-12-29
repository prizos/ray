#include "matter.h"
#include "water.h"  // For WaterState definition in sync functions
#include "noise.h"
#include <string.h>
#include <stdlib.h>

// Forward declaration for static helper
static void matter_process_o2_displacement(MatterState *state);

// ============ SUBSTANCE PROPERTIES TABLE ============
// Real physical constants (simplified for simulation)

const SubstanceProps SUBST_PROPS[SUBST_COUNT] = {
    [SUBST_NONE] = {
        .name = "None",
        .formula = "",
        .specific_heat = 0,
        .color = BLACK
    },

    [SUBST_SILICATE] = {
        .name = "Silicate",
        .formula = "SiO2",
        .molecular_weight = FLOAT_TO_FIXED(60.08f),
        .is_polar = false,
        .melting_point = FLOAT_TO_FIXED(1986.0f),   // Won't melt in sim
        .boiling_point = FLOAT_TO_FIXED(2503.0f),
        .density_solid = FLOAT_TO_FIXED(2650.0f),
        .density_liquid = 0,
        .density_gas = 0,
        .specific_heat = FLOAT_TO_FIXED(0.7f),
        .conductivity = FLOAT_TO_FIXED(1.4f),
        .porosity = 0,                               // NON-POROUS for now
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 139, 119, 101, 255 }       // Brown/tan
    },

    [SUBST_H2O] = {
        .name = "Water",
        .formula = "H2O",
        .molecular_weight = FLOAT_TO_FIXED(18.015f),
        .is_polar = true,
        .melting_point = FLOAT_TO_FIXED(273.15f),    // 0°C
        .boiling_point = FLOAT_TO_FIXED(373.15f),    // 100°C
        .density_solid = FLOAT_TO_FIXED(917.0f),     // Ice floats
        .density_liquid = FLOAT_TO_FIXED(1000.0f),
        .density_gas = FLOAT_TO_FIXED(0.6f),
        .specific_heat = FLOAT_TO_FIXED(4.18f),      // Very high
        .conductivity = FLOAT_TO_FIXED(0.6f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 64, 164, 223, 180 }        // Blue
    },

    [SUBST_NITROGEN] = {
        .name = "Nitrogen",
        .formula = "N2",
        .molecular_weight = FLOAT_TO_FIXED(28.01f),
        .is_polar = false,
        .melting_point = FLOAT_TO_FIXED(63.0f),      // Always gas at sim temps
        .boiling_point = FLOAT_TO_FIXED(77.0f),
        .density_solid = 0,
        .density_liquid = 0,
        .density_gas = FLOAT_TO_FIXED(1.25f),
        .specific_heat = FLOAT_TO_FIXED(1.04f),
        .conductivity = FLOAT_TO_FIXED(0.024f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 200, 200, 255, 50 }        // Faint blue
    },

    [SUBST_OXYGEN] = {
        .name = "Oxygen",
        .formula = "O2",
        .molecular_weight = FLOAT_TO_FIXED(32.0f),
        .is_polar = false,
        .melting_point = FLOAT_TO_FIXED(54.0f),
        .boiling_point = FLOAT_TO_FIXED(90.0f),
        .density_solid = 0,
        .density_liquid = 0,
        .density_gas = FLOAT_TO_FIXED(1.43f),
        .specific_heat = FLOAT_TO_FIXED(0.92f),
        .conductivity = FLOAT_TO_FIXED(0.024f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = true,                          // KEY for combustion
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 200, 220, 255, 50 }
    },

    [SUBST_CO2] = {
        .name = "Carbon Dioxide",
        .formula = "CO2",
        .molecular_weight = FLOAT_TO_FIXED(44.01f),
        .is_polar = false,
        .melting_point = 0,                           // Sublimes
        .boiling_point = FLOAT_TO_FIXED(195.0f),
        .density_solid = 0,
        .density_liquid = 0,
        .density_gas = FLOAT_TO_FIXED(1.98f),
        .specific_heat = FLOAT_TO_FIXED(0.84f),
        .conductivity = FLOAT_TO_FIXED(0.015f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 180, 180, 180, 30 }
    },

    [SUBST_SMOKE] = {
        .name = "Smoke",
        .formula = "C",
        .molecular_weight = FLOAT_TO_FIXED(12.01f),
        .is_polar = false,
        .melting_point = 0,
        .boiling_point = 0,
        .density_solid = 0,
        .density_liquid = 0,
        .density_gas = FLOAT_TO_FIXED(1.1f),
        .specific_heat = FLOAT_TO_FIXED(1.0f),
        .conductivity = FLOAT_TO_FIXED(0.02f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 60, 60, 60, 150 }          // Dark gray
    },

    [SUBST_ASH] = {
        .name = "Ash",
        .formula = "iteiteite",
        .molecular_weight = FLOAT_TO_FIXED(50.0f),
        .is_polar = false,
        .melting_point = FLOAT_TO_FIXED(1500.0f),
        .boiling_point = 0,
        .density_solid = FLOAT_TO_FIXED(700.0f),
        .density_liquid = 0,
        .density_gas = 0,
        .specific_heat = FLOAT_TO_FIXED(0.8f),
        .conductivity = FLOAT_TO_FIXED(0.1f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = false,
        .ignition_temp = 0,
        .heat_of_combustion = 0,
        .color = (Color){ 80, 80, 80, 255 }          // Gray
    },

    [SUBST_CELLULOSE] = {
        .name = "Cellulose",
        .formula = "C6H10O5",
        .molecular_weight = FLOAT_TO_FIXED(162.14f),
        .is_polar = false,
        .melting_point = 0,                           // Doesn't melt, decomposes
        .boiling_point = 0,
        .density_solid = FLOAT_TO_FIXED(1500.0f),
        .density_liquid = 0,
        .density_gas = 0,
        .specific_heat = FLOAT_TO_FIXED(1.3f),
        .conductivity = FLOAT_TO_FIXED(0.05f),
        .porosity = 0,
        .permeability = 0,
        .is_oxidizer = false,
        .is_fuel = true,                              // Can burn!
        .ignition_temp = FLOAT_TO_FIXED(533.0f),      // ~260°C
        .heat_of_combustion = FLOAT_TO_FIXED(17.5f),  // MJ/kg = 17.5 J/g
        .color = (Color){ 86, 141, 58, 255 }          // Green
    }
};

// ============ PHASE DETERMINATION ============

Phase substance_get_phase(Substance s, fixed16_t temp_kelvin) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return PHASE_SOLID;

    const SubstanceProps *p = &SUBST_PROPS[s];

    // Handle substances that don't change phase at sim temps
    if (p->melting_point == 0 && p->boiling_point == 0) {
        // Use density to determine default phase
        if (p->density_gas > 0) return PHASE_GAS;
        if (p->density_liquid > 0) return PHASE_LIQUID;
        return PHASE_SOLID;
    }

    // Check phase based on temperature
    if (p->melting_point > 0 && temp_kelvin < p->melting_point) {
        return PHASE_SOLID;
    }
    if (p->boiling_point > 0 && temp_kelvin >= p->boiling_point) {
        return PHASE_GAS;
    }
    if (p->melting_point > 0 && p->boiling_point > 0) {
        return PHASE_LIQUID;  // Between melt and boil
    }

    // Default based on which densities are defined
    if (p->density_solid > 0) return PHASE_SOLID;
    if (p->density_liquid > 0) return PHASE_LIQUID;
    return PHASE_GAS;
}

// ============ CELL OPERATIONS ============

void cell_add_mass(MatterCell *cell, Substance s, fixed16_t amount) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return;
    if (amount <= 0) return;

    cell->mass[s] += amount;
}

fixed16_t cell_remove_mass(MatterCell *cell, Substance s, fixed16_t amount) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return 0;
    if (amount <= 0) return 0;

    fixed16_t available = cell->mass[s];
    fixed16_t removed = (amount < available) ? amount : available;
    cell->mass[s] -= removed;
    return removed;
}

void cell_add_energy(MatterCell *cell, fixed16_t joules) {
    cell->energy += joules;
    if (cell->energy < 0) cell->energy = 0;
}

fixed16_t cell_get_mass(const MatterCell *cell, Substance s) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return 0;
    return cell->mass[s];
}

fixed16_t cell_get_fuel_mass(const MatterCell *cell) {
    fixed16_t total = 0;
    for (int s = 0; s < SUBST_COUNT; s++) {
        if (SUBST_PROPS[s].is_fuel && cell->mass[s] > 0) {
            total += cell->mass[s];
        }
    }
    return total;
}

bool cell_can_combust(const MatterCell *cell, Substance fuel) {
    if (fuel <= SUBST_NONE || fuel >= SUBST_COUNT) return false;

    const SubstanceProps *p = &SUBST_PROPS[fuel];

    if (!p->is_fuel) return false;
    if (cell->mass[fuel] < FLOAT_TO_FIXED(0.01f)) return false;
    if (cell->temperature < p->ignition_temp) return false;

    // Need oxidizer (oxygen)
    if (cell->mass[SUBST_OXYGEN] < FLOAT_TO_FIXED(0.001f)) return false;

    // Water suppression: liquid water prevents combustion (Theory 5)
    if (cell->h2o_liquid > FLOAT_TO_FIXED(0.1f)) return false;

    return true;
}

// Get total H2O mass in a cell (all phases)
fixed16_t cell_total_h2o(const MatterCell *cell) {
    return cell->h2o_ice + cell->h2o_liquid + cell->h2o_steam;
}

void cell_update_cache(MatterCell *cell) {
    cell->total_mass = 0;
    cell->thermal_mass = 0;
    cell->solid_mass = 0;
    cell->liquid_mass = 0;
    cell->gas_mass = 0;

    // Process non-H2O substances (H2O is tracked separately by phase)
    for (int s = 0; s < SUBST_COUNT; s++) {
        if (s == SUBST_H2O) continue;  // Skip - handled separately
        if (cell->mass[s] <= 0) continue;

        cell->total_mass += cell->mass[s];
        cell->thermal_mass += fixed_mul(cell->mass[s],
                                        SUBST_PROPS[s].specific_heat);

        Phase p = substance_get_phase(s, cell->temperature);
        switch (p) {
            case PHASE_SOLID:  cell->solid_mass  += cell->mass[s]; break;
            case PHASE_LIQUID: cell->liquid_mass += cell->mass[s]; break;
            case PHASE_GAS:    cell->gas_mass    += cell->mass[s]; break;
        }
    }

    // Add H2O phases with phase-specific specific heats
    if (cell->h2o_ice > 0) {
        cell->total_mass += cell->h2o_ice;
        cell->solid_mass += cell->h2o_ice;
        cell->thermal_mass += fixed_mul(cell->h2o_ice, SPECIFIC_HEAT_ICE);
    }
    if (cell->h2o_liquid > 0) {
        cell->total_mass += cell->h2o_liquid;
        cell->liquid_mass += cell->h2o_liquid;
        cell->thermal_mass += fixed_mul(cell->h2o_liquid, SPECIFIC_HEAT_WATER);
    }
    if (cell->h2o_steam > 0) {
        cell->total_mass += cell->h2o_steam;
        cell->gas_mass += cell->h2o_steam;
        cell->thermal_mass += fixed_mul(cell->h2o_steam, SPECIFIC_HEAT_STEAM);
    }

    // Calculate temperature: T = E / thermal_mass
    if (cell->thermal_mass > FLOAT_TO_FIXED(0.01f)) {
        cell->temperature = fixed_div(cell->energy, cell->thermal_mass);
    } else {
        cell->temperature = AMBIENT_TEMP;
    }

    // Enforce absolute zero floor only - no upper limit (Theory 4 & 6)
    if (cell->temperature < 0) {
        cell->temperature = 0;
        cell->energy = 0;  // At absolute zero, no thermal energy
    }
    // NO upper clamp - temperatures can be arbitrarily high
}

fixed16_t cell_get_temperature(const MatterCell *cell) {
    return cell->temperature;
}

// ============ HEAT CONDUCTION ============
// Pure physics-based heat conduction
// Heat flows from hot to cold proportional to temperature difference
// No arbitrary thresholds or optimization hacks

void matter_conduct_heat(MatterState *state) {
    // Temporary array for energy changes (static to avoid stack overflow)
    static fixed16_t energy_delta[MATTER_RES][MATTER_RES];
    memset(energy_delta, 0, sizeof(energy_delta));

    // Neighbor offsets
    const int dx[4] = {-1, 1, 0, 0};
    const int dz[4] = {0, 0, -1, 1};

    // Process all cells
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];

            // Skip cells with negligible thermal mass
            if (cell->thermal_mass < FLOAT_TO_FIXED(0.001f)) continue;

            // Heat exchange with 4 neighbors
            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d];
                int nz = z + dz[d];

                // Skip out-of-bounds neighbors
                if (nx < 0 || nx >= MATTER_RES || nz < 0 || nz >= MATTER_RES) continue;

                MatterCell *neighbor = &state->cells[nx][nz];
                if (neighbor->thermal_mass < FLOAT_TO_FIXED(0.001f)) continue;

                // Heat flow = conductivity * temperature_difference
                // Positive = receiving heat from neighbor, negative = giving heat
                fixed16_t temp_diff = neighbor->temperature - cell->temperature;
                fixed16_t heat_flow = fixed_mul(temp_diff, CONDUCTION_RATE);

                // Limit transfer to prevent instability (max 5% of donor's energy per neighbor)
                if (heat_flow > 0 && neighbor->energy > 0) {
                    fixed16_t max_transfer = neighbor->energy / 20;
                    if (heat_flow > max_transfer) heat_flow = max_transfer;
                } else if (heat_flow < 0 && cell->energy > 0) {
                    fixed16_t max_transfer = cell->energy / 20;
                    if (heat_flow < -max_transfer) heat_flow = -max_transfer;
                }

                energy_delta[x][z] += heat_flow;
            }

            // Radiative heat exchange with environment
            // Net radiation = rate * (T_cell - T_ambient)
            // Positive excess = lose heat, negative excess = gain heat
            fixed16_t temp_excess = cell->temperature - AMBIENT_TEMP;
            fixed16_t radiation = fixed_mul(temp_excess, RADIATION_RATE);

            // Limit radiation to prevent going negative
            if (radiation > 0) {
                fixed16_t max_rad = cell->energy / 50;
                if (radiation > max_rad) radiation = max_rad;
            }
            // Note: negative radiation (absorbing from environment) is allowed

            energy_delta[x][z] -= radiation;
        }
    }

    // Apply energy changes
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            state->cells[x][z].energy += energy_delta[x][z];

            // Floor at absolute zero (0K)
            if (state->cells[x][z].energy < 0) {
                state->cells[x][z].energy = 0;
            }
        }
    }
}

// ============ COMBUSTION ============

void matter_process_combustion(MatterState *state) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];

            // Check cellulose combustion
            if (!cell_can_combust(cell, SUBST_CELLULOSE)) continue;

            fixed16_t fuel = cell->mass[SUBST_CELLULOSE];
            fixed16_t o2 = cell->mass[SUBST_OXYGEN];

            // Burn rate: fast enough to consume fuel before fire goes out
            // At 30Hz, 0.05 kg/tick = 1.5 kg/sec (fire consumes fuel in 2-7 ticks)
            fixed16_t burn_rate = FLOAT_TO_FIXED(0.05f);  // kg/tick

            // Limit by available fuel and oxygen
            // Stoichiometry: need about 1:1.2 ratio of cellulose:O2
            fixed16_t max_by_fuel = fuel;
            fixed16_t max_by_o2 = fixed_mul(o2, FLOAT_TO_FIXED(3.0f));
            fixed16_t actual_burn = burn_rate;
            if (actual_burn > max_by_fuel) actual_burn = max_by_fuel;
            if (actual_burn > max_by_o2) actual_burn = max_by_o2;

            if (actual_burn <= 0) continue;

            // Consume reactants
            cell->mass[SUBST_CELLULOSE] -= actual_burn;
            cell->mass[SUBST_OXYGEN] -= fixed_mul(actual_burn, FLOAT_TO_FIXED(0.33f));

            // Produce products
            // C6H10O5 + 6O2 → 6CO2 + 5H2O (simplified ratios)
            cell->mass[SUBST_CO2] += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.8f));
            cell->mass[SUBST_H2O] += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.1f));
            cell->mass[SUBST_ASH] += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.03f));
            cell->mass[SUBST_SMOKE] += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.07f));

            // Release heat
            fixed16_t heat = fixed_mul(actual_burn,
                                       SUBST_PROPS[SUBST_CELLULOSE].heat_of_combustion);
            // Scale up heat since heat_of_combustion is in J/g but we're burning kg
            heat = fixed_mul(heat, FLOAT_TO_FIXED(1000.0f));
            cell->energy += heat;
        }
    }
}

// ============ GAS DIFFUSION ============

void matter_diffuse_gases(MatterState *state) {
    // Temporary array for mass changes
    fixed16_t mass_delta[MATTER_RES][MATTER_RES][SUBST_COUNT];
    memset(mass_delta, 0, sizeof(mass_delta));

    fixed16_t diffusion_rate = FLOAT_TO_FIXED(0.1f);

    // Only diffuse gas substances
    Substance gases[] = {SUBST_NITROGEN, SUBST_OXYGEN, SUBST_CO2, SUBST_SMOKE};
    int num_gases = 4;

    for (int x = 1; x < MATTER_RES - 1; x++) {
        for (int z = 1; z < MATTER_RES - 1; z++) {
            MatterCell *cell = &state->cells[x][z];

            int dx[4] = {-1, 1, 0, 0};
            int dz[4] = {0, 0, -1, 1};

            for (int g = 0; g < num_gases; g++) {
                Substance s = gases[g];
                fixed16_t my_mass = cell->mass[s];

                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int nz = z + dz[d];
                    MatterCell *neighbor = &state->cells[nx][nz];
                    fixed16_t their_mass = neighbor->mass[s];

                    // Diffuse toward equilibrium
                    fixed16_t diff = their_mass - my_mass;
                    fixed16_t transfer = fixed_mul(diff, diffusion_rate);

                    mass_delta[x][z][s] += transfer;
                }
            }
        }
    }

    // Apply mass changes
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            for (int s = 0; s < SUBST_COUNT; s++) {
                state->cells[x][z].mass[s] += mass_delta[x][z][s];
                if (state->cells[x][z].mass[s] < 0) {
                    state->cells[x][z].mass[s] = 0;
                }
            }
        }
    }
}

// ============ INITIALIZATION ============

void matter_init(MatterState *state, const int terrain[MATTER_RES][MATTER_RES], uint32_t seed) {
    memset(state, 0, sizeof(MatterState));

    // Use seed for reproducible vegetation placement
    noise_init(seed + 12345);

    // Noise config for vegetation patches
    NoiseConfig veg_noise = {
        .seed = seed + 12345,
        .octaves = 3,
        .lacunarity = 2.0f,
        .persistence = 0.5f,
        .scale = 0.08f
    };

    // Initialize each cell
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];
            memset(cell, 0, sizeof(MatterCell));

            int height = terrain[x][z];
            cell->terrain_height = height;
            cell->light_level = FIXED_ONE;

            // Ground: silicate (non-porous)
            cell->mass[SUBST_SILICATE] = FLOAT_TO_FIXED(1.0f);

            // Atmosphere: N2 78%, O2 21% (normalized to small amount per cell)
            cell->mass[SUBST_NITROGEN] = FLOAT_TO_FIXED(0.078f);
            cell->mass[SUBST_OXYGEN] = FLOAT_TO_FIXED(0.021f);

            // Vegetation: cellulose (sparse, noise-based)
            float veg_value = noise_fbm2d((float)x, (float)z, &veg_noise);
            float veg_density = (veg_value + 1.0f) * 0.5f;  // Normalize to 0-1

            float threshold = 0.55f;  // Higher = sparser vegetation

            if (height > 2 && height < 10 && veg_density > threshold) {
                float patch_strength = (veg_density - threshold) / (1.0f - threshold);
                fixed16_t cellulose_amount = FLOAT_TO_FIXED(0.1f + 0.25f * patch_strength);
                cell->mass[SUBST_CELLULOSE] = cellulose_amount;
            }

            // Set initial temperature to ambient (20°C = 293K)
            cell_update_cache(cell);
            cell->energy = fixed_mul(cell->thermal_mass, AMBIENT_TEMP);
            cell->temperature = AMBIENT_TEMP;
        }
    }

    state->tick = 0;
    state->accumulator = 0.0f;
    state->initialized = true;

    TraceLog(LOG_INFO, "MATTER: Initialized %dx%d grid with seed %u", MATTER_RES, MATTER_RES, seed);
}

void matter_reset(MatterState *state) {
    int terrain[MATTER_RES][MATTER_RES];
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            terrain[x][z] = state->cells[x][z].terrain_height;
        }
    }

    // Use current tick as seed for varied reset
    matter_init(state, terrain, state->tick);
}

// ============ MAIN UPDATE ============

int matter_update(MatterState *state, float delta_time) {
    if (!state->initialized) return 0;

    state->accumulator += delta_time;

    int steps = 0;
    const float dt = MATTER_UPDATE_DT;

    while (state->accumulator >= dt) {
        matter_step(state);
        state->accumulator -= dt;
        steps++;

        if (steps >= 4) {
            state->accumulator = 0.0f;
            break;
        }
    }

    return steps;
}

void matter_step(MatterState *state) {
    // Update cached values for all cells
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            cell_update_cache(&state->cells[x][z]);
        }
    }

    // O2 displacement by water (must come before combustion check)
    matter_process_o2_displacement(state);

    // Heat conduction
    matter_conduct_heat(state);

    // Process combustion
    matter_process_combustion(state);

    // Process phase transitions (evaporation, condensation, melting, freezing)
    matter_process_phase_transitions(state);

    // Replenish atmospheric gases (we're outdoors, unlimited air supply)
    // Only replenish cells not submerged in water
    fixed16_t ambient_o2 = FLOAT_TO_FIXED(0.021f);
    fixed16_t ambient_n2 = FLOAT_TO_FIXED(0.078f);
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];

            // Skip replenishment for submerged cells (O2 is displaced by water)
            if (cell->h2o_liquid > FLOAT_TO_FIXED(0.5f)) continue;

            // Slowly replenish toward ambient (10% per tick)
            fixed16_t o2_diff = ambient_o2 - cell->mass[SUBST_OXYGEN];
            fixed16_t n2_diff = ambient_n2 - cell->mass[SUBST_NITROGEN];
            cell->mass[SUBST_OXYGEN] += o2_diff / 10;
            cell->mass[SUBST_NITROGEN] += n2_diff / 10;
        }
    }

    // Optional: gas diffusion (for smoke spread)
    // matter_diffuse_gases(state);

    // Update caches again after reactions
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            cell_update_cache(&state->cells[x][z]);
        }
    }

    state->tick++;
}

// ============ QUERIES ============

MatterCell* matter_get_cell(MatterState *state, int x, int z) {
    if (!matter_cell_valid(x, z)) return NULL;
    return &state->cells[x][z];
}

const MatterCell* matter_get_cell_const(const MatterState *state, int x, int z) {
    if (!matter_cell_valid(x, z)) return NULL;
    return &state->cells[x][z];
}

void matter_world_to_cell(float world_x, float world_z, int *cell_x, int *cell_z) {
    *cell_x = (int)(world_x / MATTER_CELL_SIZE);
    *cell_z = (int)(world_z / MATTER_CELL_SIZE);
    if (*cell_x < 0) *cell_x = 0;
    if (*cell_x >= MATTER_RES) *cell_x = MATTER_RES - 1;
    if (*cell_z < 0) *cell_z = 0;
    if (*cell_z >= MATTER_RES) *cell_z = MATTER_RES - 1;
}

void matter_cell_to_world(int cell_x, int cell_z, float *world_x, float *world_z) {
    *world_x = (cell_x * MATTER_CELL_SIZE) + (MATTER_CELL_SIZE / 2.0f);
    *world_z = (cell_z * MATTER_CELL_SIZE) + (MATTER_CELL_SIZE / 2.0f);
}

// ============ EXTERNAL INTERACTIONS ============

void matter_add_heat_at(MatterState *state, float world_x, float world_z,
                        fixed16_t energy) {
    int cx, cz;
    matter_world_to_cell(world_x, world_z, &cx, &cz);

    MatterCell *cell = matter_get_cell(state, cx, cz);
    if (cell) {
        cell_add_energy(cell, energy);
    }
}

void matter_add_water_at(MatterState *state, float world_x, float world_z,
                         fixed16_t mass) {
    int cx, cz;
    matter_world_to_cell(world_x, world_z, &cx, &cz);

    MatterCell *cell = matter_get_cell(state, cx, cz);
    if (cell) {
        // Add water as liquid phase (at current temperature)
        cell->h2o_liquid += mass;

        // Add thermal energy for the new water (at ambient temperature)
        fixed16_t water_thermal = fixed_mul(mass, SPECIFIC_HEAT_WATER);
        cell->energy += fixed_mul(water_thermal, AMBIENT_TEMP);
    }
}

void matter_set_light(MatterState *state, int x, int z, fixed16_t level) {
    if (!matter_cell_valid(x, z)) return;
    state->cells[x][z].light_level = level;
}

// ============ CONSERVATION ============

fixed16_t matter_total_energy(const MatterState *state) {
    fixed16_t total = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            total += state->cells[x][z].energy;
        }
    }
    return total;
}

fixed16_t matter_total_mass(const MatterState *state, Substance s) {
    fixed16_t total = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            total += state->cells[x][z].mass[s];
        }
    }
    return total;
}

uint32_t matter_checksum(const MatterState *state) {
    uint32_t sum = state->tick;

    for (int x = 0; x < MATTER_RES; x += 4) {
        for (int z = 0; z < MATTER_RES; z += 4) {
            sum ^= (uint32_t)state->cells[x][z].energy;
            sum ^= (uint32_t)state->cells[x][z].temperature << 16;
        }
    }

    return sum;
}

// ============ WATER SYNC ============

void matter_sync_from_water(MatterState *matter, const struct WaterState *water) {
    if (!matter || !water) return;

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Get water depth from water simulation
            fixed16_t depth = water->cells[x][z].water_height;

            // Convert depth to liquid water mass
            // WATER_MASS_PER_DEPTH = 1.0 g per unit depth (simplified)
            fixed16_t new_liquid = fixed_mul(depth, WATER_MASS_PER_DEPTH);

            MatterCell *cell = &matter->cells[x][z];
            fixed16_t old_liquid = cell->h2o_liquid;

            if (new_liquid != old_liquid) {
                // Adjust energy for mass change
                fixed16_t delta = new_liquid - old_liquid;
                if (delta > 0) {
                    // Water added - bring in at ambient temperature
                    fixed16_t delta_thermal = fixed_mul(delta, SPECIFIC_HEAT_WATER);
                    cell->energy += fixed_mul(delta_thermal, AMBIENT_TEMP);
                } else {
                    // Water removed - energy leaves with water
                    fixed16_t delta_thermal = fixed_mul(-delta, SPECIFIC_HEAT_WATER);
                    cell->energy -= fixed_mul(delta_thermal, cell->temperature);
                    if (cell->energy < 0) cell->energy = 0;
                }
                cell->h2o_liquid = new_liquid;
            }
        }
    }
}

void matter_sync_to_water(const MatterState *matter, struct WaterState *water) {
    if (!matter || !water) return;

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Convert liquid water mass back to depth
            fixed16_t liquid = matter->cells[x][z].h2o_liquid;
            fixed16_t depth = fixed_div(liquid, WATER_MASS_PER_DEPTH);
            water->cells[x][z].water_height = depth;
        }
    }
}

// ============ PHASE TRANSITIONS ============

void matter_process_phase_transitions(MatterState *state) {
    fixed16_t evap_rate = FLOAT_TO_FIXED(0.01f);  // g/tick max evaporation
    fixed16_t condense_rate = FLOAT_TO_FIXED(0.01f);
    fixed16_t melt_rate = FLOAT_TO_FIXED(0.01f);
    fixed16_t freeze_rate = FLOAT_TO_FIXED(0.01f);

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];

            // === EVAPORATION (liquid → steam at boiling point) ===
            if (cell->temperature >= WATER_BOILING_POINT && cell->h2o_liquid > 0) {
                // Calculate excess energy above boiling
                fixed16_t excess_temp = cell->temperature - WATER_BOILING_POINT;
                if (excess_temp > 0) {
                    // How much can we evaporate with available excess energy?
                    fixed16_t excess_energy = fixed_mul(excess_temp, cell->thermal_mass);
                    fixed16_t max_by_energy = fixed_div(excess_energy, LATENT_HEAT_VAPORIZATION);
                    fixed16_t max_by_mass = cell->h2o_liquid;

                    fixed16_t evap = evap_rate;
                    if (evap > max_by_energy) evap = max_by_energy;
                    if (evap > max_by_mass) evap = max_by_mass;
                    if (evap <= 0) continue;

                    // Transfer mass (conservation of mass)
                    cell->h2o_liquid -= evap;
                    cell->h2o_steam += evap;

                    // Consume latent heat (conservation of energy)
                    cell->energy -= fixed_mul(evap, LATENT_HEAT_VAPORIZATION);
                }
            }

            // === CONDENSATION (steam → liquid below boiling point) ===
            if (cell->temperature < WATER_BOILING_POINT && cell->h2o_steam > 0) {
                fixed16_t condense = condense_rate;
                if (condense > cell->h2o_steam) condense = cell->h2o_steam;

                // Transfer mass
                cell->h2o_steam -= condense;
                cell->h2o_liquid += condense;

                // Release latent heat
                cell->energy += fixed_mul(condense, LATENT_HEAT_VAPORIZATION);
            }

            // === MELTING (ice → liquid at melting point) ===
            if (cell->temperature >= WATER_MELTING_POINT && cell->h2o_ice > 0) {
                fixed16_t excess_temp = cell->temperature - WATER_MELTING_POINT;
                if (excess_temp > 0) {
                    fixed16_t excess_energy = fixed_mul(excess_temp, cell->thermal_mass);
                    fixed16_t max_by_energy = fixed_div(excess_energy, LATENT_HEAT_FUSION);
                    fixed16_t max_by_mass = cell->h2o_ice;

                    fixed16_t melt = melt_rate;
                    if (melt > max_by_energy) melt = max_by_energy;
                    if (melt > max_by_mass) melt = max_by_mass;
                    if (melt <= 0) continue;

                    cell->h2o_ice -= melt;
                    cell->h2o_liquid += melt;
                    cell->energy -= fixed_mul(melt, LATENT_HEAT_FUSION);
                }
            }

            // === FREEZING (liquid → ice below melting point) ===
            if (cell->temperature < WATER_MELTING_POINT && cell->h2o_liquid > 0) {
                fixed16_t freeze = freeze_rate;
                if (freeze > cell->h2o_liquid) freeze = cell->h2o_liquid;

                cell->h2o_liquid -= freeze;
                cell->h2o_ice += freeze;
                cell->energy += fixed_mul(freeze, LATENT_HEAT_FUSION);
            }
        }
    }
}

// ============ O2 DISPLACEMENT BY WATER ============

static void matter_process_o2_displacement(MatterState *state) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];

            // Calculate submersion factor (0 = dry, 1 = fully submerged)
            fixed16_t submersion = fixed_div(cell->h2o_liquid, FLOAT_TO_FIXED(1.0f));
            if (submersion > FIXED_ONE) submersion = FIXED_ONE;
            if (submersion < 0) submersion = 0;

            // O2 displaced proportionally by water
            fixed16_t air_fraction = FIXED_ONE - submersion;
            fixed16_t ambient_o2 = FLOAT_TO_FIXED(0.021f);
            cell->mass[SUBST_OXYGEN] = fixed_mul(ambient_o2, air_fraction);
        }
    }
}
