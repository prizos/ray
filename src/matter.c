#include "matter.h"
#include "water.h"  // For WaterState definition in sync functions
#include "noise.h"
#include <string.h>
#include <stdlib.h>


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

// ============ PHASEABLE SUBSTANCE PROPERTY GETTERS ============

fixed16_t get_phaseable_melting_point(PhaseableSubstance ps, const MatterCell *cell) {
    fixed16_t mp;
    switch (ps) {
        case PHASEABLE_H2O:      mp = WATER_MELTING_POINT; break;
        case PHASEABLE_SILICATE: mp = SILICATE_MELTING_POINT; break;
        case PHASEABLE_N2:       mp = NITROGEN_MELTING_POINT; break;
        case PHASEABLE_O2:       mp = OXYGEN_MELTING_POINT; break;
        default: return 0;
    }

    // Apply geology modifier for silicate
    if (ps == PHASEABLE_SILICATE && cell) {
        if (cell->geology_type == GEOLOGY_BEDROCK) {
            mp = fixed_mul(mp, GEOLOGY_BEDROCK_MELT_MULT);
        } else if (cell->geology_type == GEOLOGY_TOPSOIL) {
            mp = fixed_mul(mp, GEOLOGY_TOPSOIL_MELT_MULT);
        }
    }
    return mp;
}

fixed16_t get_phaseable_boiling_point(PhaseableSubstance ps) {
    switch (ps) {
        case PHASEABLE_H2O:      return WATER_BOILING_POINT;
        case PHASEABLE_SILICATE: return SILICATE_BOILING_POINT;
        case PHASEABLE_N2:       return NITROGEN_BOILING_POINT;
        case PHASEABLE_O2:       return OXYGEN_BOILING_POINT;
        default: return 0;
    }
}

fixed16_t get_phaseable_latent_fusion(PhaseableSubstance ps) {
    switch (ps) {
        case PHASEABLE_H2O:      return LATENT_HEAT_H2O_FUSION;
        case PHASEABLE_SILICATE: return LATENT_HEAT_SILICATE_FUSION;
        case PHASEABLE_N2:       return LATENT_HEAT_N2_FUSION;
        case PHASEABLE_O2:       return LATENT_HEAT_O2_FUSION;
        default: return 0;
    }
}

fixed16_t get_phaseable_latent_vaporization(PhaseableSubstance ps) {
    switch (ps) {
        case PHASEABLE_H2O:      return LATENT_HEAT_H2O_VAPORIZATION;
        case PHASEABLE_SILICATE: return LATENT_HEAT_SILICATE_VAPORIZATION;
        case PHASEABLE_N2:       return LATENT_HEAT_N2_VAPORIZATION;
        case PHASEABLE_O2:       return LATENT_HEAT_O2_VAPORIZATION;
        default: return 0;
    }
}

fixed16_t get_phaseable_specific_heat(PhaseableSubstance ps, Phase phase) {
    switch (ps) {
        case PHASEABLE_H2O:
            switch (phase) {
                case PHASE_SOLID:  return SPECIFIC_HEAT_H2O_SOLID;
                case PHASE_LIQUID: return SPECIFIC_HEAT_H2O_LIQUID;
                case PHASE_GAS:    return SPECIFIC_HEAT_H2O_GAS;
            }
            break;
        case PHASEABLE_SILICATE:
            switch (phase) {
                case PHASE_SOLID:  return SPECIFIC_HEAT_SILICATE_SOLID;
                case PHASE_LIQUID: return SPECIFIC_HEAT_SILICATE_LIQUID;
                case PHASE_GAS:    return SPECIFIC_HEAT_SILICATE_GAS;
            }
            break;
        case PHASEABLE_N2:
            switch (phase) {
                case PHASE_SOLID:  return SPECIFIC_HEAT_N2_SOLID;
                case PHASE_LIQUID: return SPECIFIC_HEAT_N2_LIQUID;
                case PHASE_GAS:    return SPECIFIC_HEAT_N2_GAS;
            }
            break;
        case PHASEABLE_O2:
            switch (phase) {
                case PHASE_SOLID:  return SPECIFIC_HEAT_O2_SOLID;
                case PHASE_LIQUID: return SPECIFIC_HEAT_O2_LIQUID;
                case PHASE_GAS:    return SPECIFIC_HEAT_O2_GAS;
            }
            break;
        default:
            break;
    }
    return FLOAT_TO_FIXED(1.0f);  // Default
}

// ============ CELL OPERATIONS ============

// Get total mass of a phaseable substance (all 3 phases)
fixed16_t cell_get_phaseable_total(const MatterCell *cell, PhaseableSubstance ps) {
    if (ps >= PHASEABLE_COUNT) return 0;
    const PhaseMass *pm = &cell->phase_mass[ps];
    return pm->solid + pm->liquid + pm->gas;
}

// Get total H2O mass in a cell (all phases) - legacy compatibility
fixed16_t cell_total_h2o(const MatterCell *cell) {
    return cell_get_phaseable_total(cell, PHASEABLE_H2O);
}

void cell_add_mass(MatterCell *cell, Substance s, fixed16_t amount) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return;
    if (amount <= 0) return;

    // Route to correct storage based on substance type
    switch (s) {
        case SUBST_H2O:
            // Add as liquid by default
            cell->phase_mass[PHASEABLE_H2O].liquid += amount;
            break;
        case SUBST_SILICATE:
            // Add as solid by default
            cell->phase_mass[PHASEABLE_SILICATE].solid += amount;
            break;
        case SUBST_NITROGEN:
            // Add as gas by default (normal conditions)
            cell->phase_mass[PHASEABLE_N2].gas += amount;
            break;
        case SUBST_OXYGEN:
            // Add as gas by default
            cell->phase_mass[PHASEABLE_O2].gas += amount;
            break;
        case SUBST_CO2:
            cell->co2_gas += amount;
            break;
        case SUBST_SMOKE:
            cell->smoke_gas += amount;
            break;
        case SUBST_ASH:
            cell->ash_solid += amount;
            break;
        case SUBST_CELLULOSE:
            cell->cellulose_solid += amount;
            break;
        default:
            break;
    }
}

fixed16_t cell_remove_mass(MatterCell *cell, Substance s, fixed16_t amount) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return 0;
    if (amount <= 0) return 0;

    fixed16_t *target = NULL;
    fixed16_t available = 0;

    switch (s) {
        case SUBST_H2O:
            // Remove from liquid first, then steam, then ice
            if (cell->phase_mass[PHASEABLE_H2O].liquid >= amount) {
                cell->phase_mass[PHASEABLE_H2O].liquid -= amount;
                return amount;
            }
            available = cell_get_phaseable_total(cell, PHASEABLE_H2O);
            break;
        case SUBST_SILICATE:
            target = &cell->phase_mass[PHASEABLE_SILICATE].solid;
            available = *target;
            break;
        case SUBST_NITROGEN:
            target = &cell->phase_mass[PHASEABLE_N2].gas;
            available = *target;
            break;
        case SUBST_OXYGEN:
            target = &cell->phase_mass[PHASEABLE_O2].gas;
            available = *target;
            break;
        case SUBST_CO2:
            target = &cell->co2_gas;
            available = *target;
            break;
        case SUBST_SMOKE:
            target = &cell->smoke_gas;
            available = *target;
            break;
        case SUBST_ASH:
            target = &cell->ash_solid;
            available = *target;
            break;
        case SUBST_CELLULOSE:
            target = &cell->cellulose_solid;
            available = *target;
            break;
        default:
            return 0;
    }

    if (target) {
        fixed16_t removed = (amount < available) ? amount : available;
        *target -= removed;
        return removed;
    }
    return 0;
}

void cell_add_energy(MatterCell *cell, fixed16_t joules) {
    cell->energy += joules;
    if (cell->energy < 0) cell->energy = 0;
}

fixed16_t cell_get_mass(const MatterCell *cell, Substance s) {
    if (s <= SUBST_NONE || s >= SUBST_COUNT) return 0;

    switch (s) {
        case SUBST_H2O:      return cell_get_phaseable_total(cell, PHASEABLE_H2O);
        case SUBST_SILICATE: return cell_get_phaseable_total(cell, PHASEABLE_SILICATE);
        case SUBST_NITROGEN: return cell_get_phaseable_total(cell, PHASEABLE_N2);
        case SUBST_OXYGEN:   return cell_get_phaseable_total(cell, PHASEABLE_O2);
        case SUBST_CO2:      return cell->co2_gas;
        case SUBST_SMOKE:    return cell->smoke_gas;
        case SUBST_ASH:      return cell->ash_solid;
        case SUBST_CELLULOSE: return cell->cellulose_solid;
        default: return 0;
    }
}

fixed16_t cell_get_fuel_mass(const MatterCell *cell) {
    // Only cellulose is fuel
    return cell->cellulose_solid;
}

bool cell_can_combust(const MatterCell *cell, Substance fuel) {
    if (fuel != SUBST_CELLULOSE) return false;

    const SubstanceProps *p = &SUBST_PROPS[fuel];

    if (!p->is_fuel) return false;
    if (cell->cellulose_solid < FLOAT_TO_FIXED(0.01f)) return false;
    if (cell->temperature < p->ignition_temp) return false;

    // Need oxidizer (oxygen gas)
    if (cell->phase_mass[PHASEABLE_O2].gas < FLOAT_TO_FIXED(0.001f)) return false;

    // Water suppression: liquid water prevents combustion (Theory 5)
    if (cell->phase_mass[PHASEABLE_H2O].liquid > FLOAT_TO_FIXED(0.1f)) return false;

    return true;
}

void cell_update_cache(MatterCell *cell) {
    cell->total_mass = 0;
    cell->thermal_mass = 0;
    cell->solid_mass = 0;
    cell->liquid_mass = 0;
    cell->gas_mass = 0;

    // Process all phaseable substances with phase-specific specific heats
    for (int ps = 0; ps < PHASEABLE_COUNT; ps++) {
        const PhaseMass *pm = &cell->phase_mass[ps];

        // Solid phase
        if (pm->solid > 0) {
            cell->total_mass += pm->solid;
            cell->solid_mass += pm->solid;
            cell->thermal_mass += fixed_mul(pm->solid,
                get_phaseable_specific_heat(ps, PHASE_SOLID));
        }

        // Liquid phase
        if (pm->liquid > 0) {
            cell->total_mass += pm->liquid;
            cell->liquid_mass += pm->liquid;
            cell->thermal_mass += fixed_mul(pm->liquid,
                get_phaseable_specific_heat(ps, PHASE_LIQUID));
        }

        // Gas phase
        if (pm->gas > 0) {
            cell->total_mass += pm->gas;
            cell->gas_mass += pm->gas;
            cell->thermal_mass += fixed_mul(pm->gas,
                get_phaseable_specific_heat(ps, PHASE_GAS));
        }
    }

    // Process non-phaseable substances
    // CO2 - always gas
    if (cell->co2_gas > 0) {
        cell->total_mass += cell->co2_gas;
        cell->gas_mass += cell->co2_gas;
        cell->thermal_mass += fixed_mul(cell->co2_gas, SUBST_PROPS[SUBST_CO2].specific_heat);
    }

    // Smoke - always gas
    if (cell->smoke_gas > 0) {
        cell->total_mass += cell->smoke_gas;
        cell->gas_mass += cell->smoke_gas;
        cell->thermal_mass += fixed_mul(cell->smoke_gas, SUBST_PROPS[SUBST_SMOKE].specific_heat);
    }

    // Ash - always solid
    if (cell->ash_solid > 0) {
        cell->total_mass += cell->ash_solid;
        cell->solid_mass += cell->ash_solid;
        cell->thermal_mass += fixed_mul(cell->ash_solid, SUBST_PROPS[SUBST_ASH].specific_heat);
    }

    // Cellulose - always solid
    if (cell->cellulose_solid > 0) {
        cell->total_mass += cell->cellulose_solid;
        cell->solid_mass += cell->cellulose_solid;
        cell->thermal_mass += fixed_mul(cell->cellulose_solid, SUBST_PROPS[SUBST_CELLULOSE].specific_heat);
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

    // Update geology type based on lava presence
    cell_update_geology(cell);
}

fixed16_t cell_get_temperature(const MatterCell *cell) {
    return cell->temperature;
}

// Get effective melting point for silicate based on geology type
fixed16_t cell_get_silicate_melting_point(const MatterCell *cell) {
    return get_phaseable_melting_point(PHASEABLE_SILICATE, cell);
}

// Update geology type based on phase state (lava detection)
void cell_update_geology(MatterCell *cell) {
    // If there's significant liquid silicate, it's lava
    if (cell->phase_mass[PHASEABLE_SILICATE].liquid > FLOAT_TO_FIXED(0.1f)) {
        cell->geology_type = GEOLOGY_LAVA;
    }
    // If lava has solidified, convert to rock
    else if (cell->geology_type == GEOLOGY_LAVA &&
             cell->phase_mass[PHASEABLE_SILICATE].liquid < FLOAT_TO_FIXED(0.01f)) {
        cell->geology_type = GEOLOGY_ROCK;  // Cooled lava becomes rock
    }
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

            fixed16_t fuel = cell->cellulose_solid;
            fixed16_t o2 = CELL_O2_GAS(cell);

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
            cell->cellulose_solid -= actual_burn;
            CELL_O2_GAS(cell) -= fixed_mul(actual_burn, FLOAT_TO_FIXED(0.33f));

            // Produce products
            // C6H10O5 + 6O2 → 6CO2 + 5H2O (simplified ratios)
            cell->co2_gas += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.8f));
            CELL_H2O_STEAM(cell) += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.1f));  // Steam from combustion
            cell->ash_solid += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.03f));
            cell->smoke_gas += fixed_mul(actual_burn, FLOAT_TO_FIXED(0.07f));

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

// Helper to get pointer to a gas field in cell
static fixed16_t* cell_get_gas_ptr(MatterCell *cell, int gas_index) {
    switch (gas_index) {
        case 0: return &cell->phase_mass[PHASEABLE_N2].gas;
        case 1: return &cell->phase_mass[PHASEABLE_O2].gas;
        case 2: return &cell->co2_gas;
        case 3: return &cell->smoke_gas;
        default: return NULL;
    }
}

void matter_diffuse_gases(MatterState *state) {
    // Temporary array for mass changes (4 gas types)
    static fixed16_t mass_delta[MATTER_RES][MATTER_RES][4];
    memset(mass_delta, 0, sizeof(mass_delta));

    fixed16_t diffusion_rate = FLOAT_TO_FIXED(0.1f);

    // Gas indices: 0=N2, 1=O2, 2=CO2, 3=Smoke
    const int num_gases = 4;

    for (int x = 1; x < MATTER_RES - 1; x++) {
        for (int z = 1; z < MATTER_RES - 1; z++) {
            MatterCell *cell = &state->cells[x][z];

            int dx[4] = {-1, 1, 0, 0};
            int dz[4] = {0, 0, -1, 1};

            for (int g = 0; g < num_gases; g++) {
                fixed16_t *my_ptr = cell_get_gas_ptr(cell, g);
                if (!my_ptr) continue;
                fixed16_t my_mass = *my_ptr;

                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int nz = z + dz[d];
                    MatterCell *neighbor = &state->cells[nx][nz];
                    fixed16_t *their_ptr = cell_get_gas_ptr(neighbor, g);
                    if (!their_ptr) continue;
                    fixed16_t their_mass = *their_ptr;

                    // Diffuse toward equilibrium
                    fixed16_t diff = their_mass - my_mass;
                    fixed16_t transfer = fixed_mul(diff, diffusion_rate);

                    mass_delta[x][z][g] += transfer;
                }
            }
        }
    }

    // Apply mass changes
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            for (int g = 0; g < num_gases; g++) {
                fixed16_t *ptr = cell_get_gas_ptr(&state->cells[x][z], g);
                if (!ptr) continue;
                *ptr += mass_delta[x][z][g];
                if (*ptr < 0) *ptr = 0;
            }
        }
    }
}

// ============ INITIALIZATION ============

// Initialize geology type based on terrain height
void matter_init_geology(MatterState *state, const int terrain[MATTER_RES][MATTER_RES]) {
    // Find max terrain height for depth calculation
    int max_height = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            if (terrain[x][z] > max_height) max_height = terrain[x][z];
        }
    }

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];
            int height = terrain[x][z];

            // Depth from surface (for geology layers)
            cell->depth_from_surface = 0;  // Surface cell

            // Geology type based on terrain height
            // Higher terrain = older rock = bedrock
            // Mid terrain = rock
            // Lower terrain = topsoil (more weathering, organic matter)
            if (height >= 10) {
                cell->geology_type = GEOLOGY_BEDROCK;
            } else if (height >= 5) {
                cell->geology_type = GEOLOGY_ROCK;
            } else if (height >= 1) {
                cell->geology_type = GEOLOGY_TOPSOIL;
            } else {
                cell->geology_type = GEOLOGY_NONE;  // Below ground level
            }
        }
    }
}

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

            // Ground: silicate (solid phase)
            CELL_SILICATE_SOLID(cell) = FLOAT_TO_FIXED(1.0f);

            // Atmosphere: N2 78%, O2 21% (normalized to small amount per cell)
            CELL_N2_GAS(cell) = FLOAT_TO_FIXED(0.078f);
            CELL_O2_GAS(cell) = FLOAT_TO_FIXED(0.021f);

            // Vegetation: cellulose (sparse, noise-based)
            float veg_value = noise_fbm2d((float)x, (float)z, &veg_noise);
            float veg_density = (veg_value + 1.0f) * 0.5f;  // Normalize to 0-1

            float threshold = 0.55f;  // Higher = sparser vegetation

            if (height > 2 && height < 10 && veg_density > threshold) {
                float patch_strength = (veg_density - threshold) / (1.0f - threshold);
                fixed16_t cellulose_amount = FLOAT_TO_FIXED(0.1f + 0.25f * patch_strength);
                cell->cellulose_solid = cellulose_amount;
            }

            // Set initial temperature to ambient (20°C = 293K)
            cell_update_cache(cell);
            cell->energy = fixed_mul(cell->thermal_mass, AMBIENT_TEMP);
            cell->temperature = AMBIENT_TEMP;
        }
    }

    // Initialize geology layers based on terrain
    matter_init_geology(state, terrain);

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

    // Note: O2 is NOT displaced by water in hermetic simulation
    // Combustion check handles water suppression directly (checks h2o_liquid)

    // Heat conduction
    matter_conduct_heat(state);

    // Process combustion
    matter_process_combustion(state);

    // Process phase transitions (evaporation, condensation, melting, freezing)
    // This handles ALL phaseable substances: H2O, Silicate, N2, O2
    matter_process_phase_transitions(state);

    // Flow liquids (water flows fast, lava flows slow with viscosity)
    matter_flow_liquids(state);

    // Note: No atmospheric replenishment - hermetic simulation
    // Gases are conserved; O2 consumed by fire is not replaced

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
        // DEBUG: capture before state
        float old_temp = FIXED_TO_FLOAT(cell->temperature);
        float old_liquid = FIXED_TO_FLOAT(CELL_H2O_LIQUID(cell));

        // Add water as liquid phase (at current temperature)
        CELL_H2O_LIQUID(cell) += mass;

        // Add thermal energy for the new water (at ambient temperature)
        fixed16_t water_thermal = fixed_mul(mass, SPECIFIC_HEAT_H2O_LIQUID);
        cell->energy += fixed_mul(water_thermal, AMBIENT_TEMP);

        // CRITICAL: Update cache after adding water
        cell_update_cache(cell);

        // DEBUG: Log water addition
        static int add_counter = 0;
        if (add_counter++ % 60 == 0) {
            float new_temp = FIXED_TO_FLOAT(cell->temperature);
            float new_liquid = FIXED_TO_FLOAT(CELL_H2O_LIQUID(cell));
            TraceLog(LOG_INFO, "ADD_WATER [%d,%d]: mass=%.2f, liquid %.2f->%.2f, temp %.1fC->%.1fC",
                     cx, cz, FIXED_TO_FLOAT(mass), old_liquid, new_liquid,
                     old_temp - 273.15f, new_temp - 273.15f);
        }
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
            total += cell_get_mass(&state->cells[x][z], s);
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

    static int debug_counter = 0;

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Get water depth from water simulation
            fixed16_t depth = water->cells[x][z].water_height;

            // Convert depth to liquid water mass
            // WATER_MASS_PER_DEPTH = 1.0 g per unit depth (simplified)
            fixed16_t new_liquid = fixed_mul(depth, WATER_MASS_PER_DEPTH);

            MatterCell *cell = &matter->cells[x][z];
            fixed16_t old_liquid = CELL_H2O_LIQUID(cell);

            if (new_liquid != old_liquid) {
                // Adjust energy for mass change
                fixed16_t delta = new_liquid - old_liquid;

                // DEBUG: Log significant water changes
                float delta_f = FIXED_TO_FLOAT(delta);
                float old_temp = FIXED_TO_FLOAT(cell->temperature);
                float old_energy = FIXED_TO_FLOAT(cell->energy);
                float old_thermal = FIXED_TO_FLOAT(cell->thermal_mass);

                if (delta > 0) {
                    // Water added - bring in at ambient temperature
                    fixed16_t delta_thermal = fixed_mul(delta, SPECIFIC_HEAT_H2O_LIQUID);
                    cell->energy += fixed_mul(delta_thermal, AMBIENT_TEMP);
                } else {
                    // Water removed - energy leaves with water
                    fixed16_t delta_thermal = fixed_mul(-delta, SPECIFIC_HEAT_H2O_LIQUID);
                    cell->energy -= fixed_mul(delta_thermal, cell->temperature);
                    if (cell->energy < 0) cell->energy = 0;
                }
                CELL_H2O_LIQUID(cell) = new_liquid;

                // CRITICAL: Update cache to recalculate thermal mass and temperature
                cell_update_cache(cell);

                // DEBUG: Log after update
                if (debug_counter++ % 60 == 0 && (delta_f > 0.5f || delta_f < -0.5f)) {
                    float new_temp = FIXED_TO_FLOAT(cell->temperature);
                    float new_energy = FIXED_TO_FLOAT(cell->energy);
                    float new_thermal = FIXED_TO_FLOAT(cell->thermal_mass);
                    TraceLog(LOG_INFO, "SYNC [%d,%d]: delta=%.2f, temp %.1f->%.1f, energy %.1f->%.1f, thermal %.1f->%.1f",
                             x, z, delta_f, old_temp - 273.15f, new_temp - 273.15f,
                             old_energy, new_energy, old_thermal, new_thermal);
                }
            }
        }
    }
}

void matter_sync_to_water(const MatterState *matter, struct WaterState *water) {
    if (!matter || !water) return;

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            const MatterCell *cell = &matter->cells[x][z];

            // Ice acts as solid terrain - blocks water flow
            // Ice is synced to ice_height (acts like additional terrain)
            // Only liquid water goes into water_height (can flow)
            fixed16_t ice_mass = CELL_H2O_ICE(cell);
            fixed16_t liquid_mass = CELL_H2O_LIQUID(cell);

            // Convert mass to depth
            fixed16_t ice_depth = fixed_div(ice_mass, WATER_MASS_PER_DEPTH);
            fixed16_t liquid_depth = fixed_div(liquid_mass, WATER_MASS_PER_DEPTH);

            // Ice raises the effective terrain height - water cannot flow into it
            water->ice_height[x][z] = ice_depth;
            // Only liquid water can flow
            water->cells[x][z].water_height = liquid_depth;
        }
    }
}

// ============ PHASE TRANSITIONS ============

// Process phase transition for a single phaseable substance in a cell
void cell_process_phase_transition(MatterCell *cell, PhaseableSubstance ps) {
    PhaseMass *pm = &cell->phase_mass[ps];

    // Get substance properties (with geology modifier for silicate melting)
    fixed16_t mp = get_phaseable_melting_point(ps, cell);
    fixed16_t bp = get_phaseable_boiling_point(ps);
    fixed16_t lh_fusion = get_phaseable_latent_fusion(ps);
    fixed16_t lh_vapor = get_phaseable_latent_vaporization(ps);

    // Rate limits to prevent instability
    fixed16_t rate = PHASE_TRANSITION_RATE;  // 0.1 g/tick

    // === MELTING (solid → liquid above melting point) ===
    if (cell->temperature >= mp && pm->solid > 0) {
        fixed16_t excess_temp = cell->temperature - mp;
        if (excess_temp > 0) {
            // Calculate max melt based on available excess energy
            fixed16_t excess_energy = fixed_mul(excess_temp, cell->thermal_mass);
            fixed16_t max_by_energy = fixed_div(excess_energy, lh_fusion);
            fixed16_t max_by_mass = pm->solid;

            fixed16_t melt = rate;
            if (melt > max_by_energy) melt = max_by_energy;
            if (melt > max_by_mass) melt = max_by_mass;

            if (melt > 0) {
                pm->solid -= melt;
                pm->liquid += melt;
                cell->energy -= fixed_mul(melt, lh_fusion);  // Consume latent heat
            }
        }
    }

    // === FREEZING (liquid → solid below melting point) ===
    if (cell->temperature < mp && pm->liquid > 0) {
        fixed16_t undercool = mp - cell->temperature;
        // Faster freezing when colder
        fixed16_t freeze = rate;
        if (undercool > FLOAT_TO_FIXED(10.0f)) {
            freeze = fixed_mul(rate, FLOAT_TO_FIXED(2.0f));  // Double rate for strong undercooling
        }
        if (freeze > pm->liquid) freeze = pm->liquid;

        if (freeze > 0) {
            pm->liquid -= freeze;
            pm->solid += freeze;
            cell->energy += fixed_mul(freeze, lh_fusion);  // Release latent heat
        }
    }

    // === BOILING/EVAPORATION (liquid → gas above boiling point) ===
    if (cell->temperature >= bp && pm->liquid > 0) {
        fixed16_t excess_temp = cell->temperature - bp;
        if (excess_temp > 0) {
            fixed16_t excess_energy = fixed_mul(excess_temp, cell->thermal_mass);
            fixed16_t max_by_energy = fixed_div(excess_energy, lh_vapor);
            fixed16_t max_by_mass = pm->liquid;

            fixed16_t evap = rate;
            if (evap > max_by_energy) evap = max_by_energy;
            if (evap > max_by_mass) evap = max_by_mass;

            if (evap > 0) {
                pm->liquid -= evap;
                pm->gas += evap;
                cell->energy -= fixed_mul(evap, lh_vapor);  // Consume latent heat
            }
        }
    }

    // === CONDENSATION (gas → liquid below boiling point) ===
    if (cell->temperature < bp && pm->gas > 0) {
        fixed16_t undercool = bp - cell->temperature;
        fixed16_t condense = rate;
        if (undercool > FLOAT_TO_FIXED(10.0f)) {
            condense = fixed_mul(rate, FLOAT_TO_FIXED(2.0f));
        }
        if (condense > pm->gas) condense = pm->gas;

        if (condense > 0) {
            pm->gas -= condense;
            pm->liquid += condense;
            cell->energy += fixed_mul(condense, lh_vapor);  // Release latent heat
        }
    }

    // === SUBLIMATION (solid → gas, skipping liquid) ===
    // Occurs when temperature jumps past both transition points
    // or for substances where liquid range is very narrow
    if (cell->temperature >= bp && pm->solid > 0 && mp >= bp - FLOAT_TO_FIXED(20.0f)) {
        // Direct sublimation for cryogenic substances with narrow liquid range
        fixed16_t excess_temp = cell->temperature - bp;
        if (excess_temp > 0) {
            fixed16_t total_latent = lh_fusion + lh_vapor;
            fixed16_t excess_energy = fixed_mul(excess_temp, cell->thermal_mass);
            fixed16_t max_by_energy = fixed_div(excess_energy, total_latent);
            fixed16_t max_by_mass = pm->solid;

            fixed16_t sublime = rate;
            if (sublime > max_by_energy) sublime = max_by_energy;
            if (sublime > max_by_mass) sublime = max_by_mass;

            if (sublime > 0) {
                pm->solid -= sublime;
                pm->gas += sublime;
                cell->energy -= fixed_mul(sublime, total_latent);
            }
        }
    }

    // === DEPOSITION (gas → solid, skipping liquid) ===
    if (cell->temperature < mp && pm->gas > 0) {
        fixed16_t undercool = mp - cell->temperature;
        if (undercool > FLOAT_TO_FIXED(10.0f)) {
            fixed16_t total_latent = lh_fusion + lh_vapor;
            fixed16_t deposit = rate;
            if (deposit > pm->gas) deposit = pm->gas;

            if (deposit > 0) {
                pm->gas -= deposit;
                pm->solid += deposit;
                cell->energy += fixed_mul(deposit, total_latent);
            }
        }
    }
}

// Process phase transitions for ALL phaseable substances in all cells
void matter_process_phase_transitions(MatterState *state) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            MatterCell *cell = &state->cells[x][z];

            // Process all phaseable substances
            for (int ps = 0; ps < PHASEABLE_COUNT; ps++) {
                cell_process_phase_transition(cell, (PhaseableSubstance)ps);
            }
        }
    }
}

// ============ LIQUID FLOW ============
// Flow liquids (water and lava) based on terrain slope and viscosity

// Viscosity constants (relative to water = 1.0)
#define WATER_VISCOSITY     FLOAT_TO_FIXED(1.0f)
#define LAVA_VISCOSITY      FLOAT_TO_FIXED(10000.0f)   // Lava flows MUCH slower

// Helper: get flow rate based on viscosity (inverse relationship)
static fixed16_t get_flow_rate(fixed16_t viscosity) {
    // Base flow rate for water
    fixed16_t base_rate = FLOAT_TO_FIXED(0.1f);
    // Divide by viscosity to slow down high-viscosity liquids
    return fixed_div(base_rate, viscosity);
}

// Flow a specific liquid type between cells
static void flow_liquid_type(MatterState *state, PhaseableSubstance ps, fixed16_t viscosity) {
    // Temporary delta array for this liquid
    static fixed16_t liquid_delta[MATTER_RES][MATTER_RES];
    memset(liquid_delta, 0, sizeof(liquid_delta));

    fixed16_t flow_rate = get_flow_rate(viscosity);
    fixed16_t min_liquid = FLOAT_TO_FIXED(0.01f);  // Minimum to flow

    // Neighbor offsets
    const int dx[4] = {-1, 1, 0, 0};
    const int dz[4] = {0, 0, -1, 1};

    for (int x = 1; x < MATTER_RES - 1; x++) {
        for (int z = 1; z < MATTER_RES - 1; z++) {
            MatterCell *cell = &state->cells[x][z];
            fixed16_t my_liquid = cell->phase_mass[ps].liquid;

            if (my_liquid < min_liquid) continue;

            // Surface height = terrain + liquid depth
            fixed16_t my_surface = INT_TO_FIXED(cell->terrain_height) + my_liquid;

            for (int d = 0; d < 4; d++) {
                int nx = x + dx[d];
                int nz = z + dz[d];
                MatterCell *neighbor = &state->cells[nx][nz];

                // Neighbor surface height
                fixed16_t their_liquid = neighbor->phase_mass[ps].liquid;
                fixed16_t their_surface = INT_TO_FIXED(neighbor->terrain_height) + their_liquid;

                // Flow from higher to lower surface
                fixed16_t surface_diff = my_surface - their_surface;
                if (surface_diff > FLOAT_TO_FIXED(0.01f)) {
                    // Calculate flow amount
                    fixed16_t flow = fixed_mul(surface_diff, flow_rate);

                    // Limit by available liquid (max 25% per neighbor)
                    fixed16_t max_flow = my_liquid / 4;
                    if (flow > max_flow) flow = max_flow;

                    // Accumulate deltas (will be applied after all calculations)
                    liquid_delta[x][z] -= flow;
                    liquid_delta[nx][nz] += flow;
                }
            }
        }
    }

    // Apply deltas
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            state->cells[x][z].phase_mass[ps].liquid += liquid_delta[x][z];
            if (state->cells[x][z].phase_mass[ps].liquid < 0) {
                state->cells[x][z].phase_mass[ps].liquid = 0;
            }
        }
    }
}

// Flow all liquid types with appropriate viscosities
void matter_flow_liquids(MatterState *state) {
    // Flow water (low viscosity - fast)
    flow_liquid_type(state, PHASEABLE_H2O, WATER_VISCOSITY);

    // Flow lava (high viscosity - slow)
    // Only flow if there's actually liquid silicate (lava) in the system
    bool has_lava = false;
    for (int x = 0; x < MATTER_RES && !has_lava; x++) {
        for (int z = 0; z < MATTER_RES && !has_lava; z++) {
            if (state->cells[x][z].phase_mass[PHASEABLE_SILICATE].liquid > FLOAT_TO_FIXED(0.01f)) {
                has_lava = true;
            }
        }
    }
    if (has_lava) {
        flow_liquid_type(state, PHASEABLE_SILICATE, LAVA_VISCOSITY);
    }

    // Flow cryogenic liquids (N2, O2) - similar viscosity to water
    // These would only exist in very cold environments
    flow_liquid_type(state, PHASEABLE_N2, WATER_VISCOSITY);
    flow_liquid_type(state, PHASEABLE_O2, WATER_VISCOSITY);
}

