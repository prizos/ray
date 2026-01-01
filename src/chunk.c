#include "chunk.h"
#include <stdio.h>
#include <string.h>

// ============ MATERIAL PROPERTIES TABLE ============
// Each material is a single phase with links to related phases.
// Phase transitions convert between linked materials.

const MaterialProperties MATERIAL_PROPS[MAT_COUNT] = {
    // ===== MAT_NONE =====
    [MAT_NONE] = {
        .name = "None", .formula = "-",
        .phase = PHASE_GAS,
        .molar_mass = 0, .molar_volume = 0, .molar_heat_capacity = 1,
        .thermal_conductivity = 0, .viscosity = 0,
        .solid_form = MAT_NONE, .liquid_form = MAT_NONE, .gas_form = MAT_NONE,
        .transition_temp_down = 0, .transition_temp_up = 0,
        .enthalpy_down = 0, .enthalpy_up = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {0, 0, 0, 0}
    },

    // ===== WATER PHASES (H2O) =====
    [MAT_ICE] = {
        .name = "Ice", .formula = "H2O",
        .phase = PHASE_SOLID,
        .molar_mass = 0.018, .molar_volume = 0.0000196, .molar_heat_capacity = 38.0,
        .thermal_conductivity = 2.2, .viscosity = 0,  // Solid, no flow
        .solid_form = MAT_NONE, .liquid_form = MAT_WATER, .gas_form = MAT_STEAM,
        .transition_temp_down = 0, .transition_temp_up = 273.15,  // Melts at 0°C
        .enthalpy_down = 0, .enthalpy_up = 6010,  // Latent heat of fusion
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {200, 220, 255, 200}
    },
    [MAT_WATER] = {
        .name = "Water", .formula = "H2O",
        .phase = PHASE_LIQUID,
        .molar_mass = 0.018, .molar_volume = 0.000018, .molar_heat_capacity = 75.3,
        .thermal_conductivity = 0.6, .viscosity = 0.001,
        .solid_form = MAT_ICE, .liquid_form = MAT_NONE, .gas_form = MAT_STEAM,
        .transition_temp_down = 273.15, .transition_temp_up = 373.15,  // Freezes at 0°C, boils at 100°C
        .enthalpy_down = 6010, .enthalpy_up = 40660,  // Fusion / vaporization
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {64, 164, 223, 180}
    },
    [MAT_STEAM] = {
        .name = "Steam", .formula = "H2O",
        .phase = PHASE_GAS,
        .molar_mass = 0.018, .molar_volume = 0.0245, .molar_heat_capacity = 33.6,
        .thermal_conductivity = 0.025, .viscosity = 0.000013,
        .solid_form = MAT_ICE, .liquid_form = MAT_WATER, .gas_form = MAT_NONE,
        .transition_temp_down = 373.15, .transition_temp_up = 0,  // Condenses at 100°C
        .enthalpy_down = 40660, .enthalpy_up = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {240, 240, 240, 80}
    },

    // ===== ROCK PHASES (SiO2) =====
    [MAT_ROCK] = {
        .name = "Rock", .formula = "SiO2",
        .phase = PHASE_SOLID,
        .molar_mass = 0.060, .molar_volume = 0.0000227, .molar_heat_capacity = 44.4,
        .thermal_conductivity = 1.4, .viscosity = 0,  // Solid
        .solid_form = MAT_NONE, .liquid_form = MAT_MAGMA, .gas_form = MAT_ROCK_VAPOR,
        .transition_temp_down = 0, .transition_temp_up = 1986,  // Melts at 1986K
        .enthalpy_down = 0, .enthalpy_up = 9600,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {128, 128, 128, 255}
    },
    [MAT_MAGMA] = {
        .name = "Magma", .formula = "SiO2",
        .phase = PHASE_LIQUID,
        .molar_mass = 0.060, .molar_volume = 0.0000273, .molar_heat_capacity = 82.6,
        .thermal_conductivity = 1.0, .viscosity = 10000000,  // Very viscous
        .solid_form = MAT_ROCK, .liquid_form = MAT_NONE, .gas_form = MAT_ROCK_VAPOR,
        .transition_temp_down = 1986, .transition_temp_up = 2503,
        .enthalpy_down = 9600, .enthalpy_up = 520000,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {255, 100, 50, 255}
    },
    [MAT_ROCK_VAPOR] = {
        .name = "Rock Vapor", .formula = "SiO2",
        .phase = PHASE_GAS,
        .molar_mass = 0.060, .molar_volume = 0.0245, .molar_heat_capacity = 47.4,
        .thermal_conductivity = 0.1, .viscosity = 0.00005,
        .solid_form = MAT_ROCK, .liquid_form = MAT_MAGMA, .gas_form = MAT_NONE,
        .transition_temp_down = 2503, .transition_temp_up = 0,
        .enthalpy_down = 520000, .enthalpy_up = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {200, 150, 100, 100}
    },

    // ===== DIRT PHASES (soil) =====
    [MAT_DIRT] = {
        .name = "Dirt", .formula = "soil",
        .phase = PHASE_SOLID,
        .molar_mass = 0.050, .molar_volume = 0.00002, .molar_heat_capacity = 40.0,
        .thermal_conductivity = 0.5, .viscosity = 0,
        .solid_form = MAT_NONE, .liquid_form = MAT_MUD, .gas_form = MAT_DIRT_VAPOR,
        .transition_temp_down = 0, .transition_temp_up = 1500,
        .enthalpy_down = 0, .enthalpy_up = 8000,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {139, 90, 43, 255}
    },
    [MAT_MUD] = {
        .name = "Mud", .formula = "soil",
        .phase = PHASE_LIQUID,
        .molar_mass = 0.050, .molar_volume = 0.000025, .molar_heat_capacity = 60.0,
        .thermal_conductivity = 0.4, .viscosity = 5000000,
        .solid_form = MAT_DIRT, .liquid_form = MAT_NONE, .gas_form = MAT_DIRT_VAPOR,
        .transition_temp_down = 1500, .transition_temp_up = 2500,
        .enthalpy_down = 8000, .enthalpy_up = 400000,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {180, 100, 50, 255}
    },
    [MAT_DIRT_VAPOR] = {
        .name = "Dirt Vapor", .formula = "soil",
        .phase = PHASE_GAS,
        .molar_mass = 0.050, .molar_volume = 0.0245, .molar_heat_capacity = 40.0,
        .thermal_conductivity = 0.1, .viscosity = 0.00005,
        .solid_form = MAT_DIRT, .liquid_form = MAT_MUD, .gas_form = MAT_NONE,
        .transition_temp_down = 2500, .transition_temp_up = 0,
        .enthalpy_down = 400000, .enthalpy_up = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {150, 120, 80, 100}
    },

    // ===== NITROGEN PHASES (N2) =====
    [MAT_SOLID_NITROGEN] = {
        .name = "Solid Nitrogen", .formula = "N2",
        .phase = PHASE_SOLID,
        .molar_mass = 0.028, .molar_volume = 0.0000159, .molar_heat_capacity = 25.7,
        .thermal_conductivity = 0.2, .viscosity = 0,
        .solid_form = MAT_NONE, .liquid_form = MAT_LIQUID_NITROGEN, .gas_form = MAT_NITROGEN,
        .transition_temp_down = 0, .transition_temp_up = 63.15,  // Melts at 63K
        .enthalpy_down = 0, .enthalpy_up = 720,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {200, 200, 255, 200}
    },
    [MAT_LIQUID_NITROGEN] = {
        .name = "Liquid Nitrogen", .formula = "N2",
        .phase = PHASE_LIQUID,
        .molar_mass = 0.028, .molar_volume = 0.0000347, .molar_heat_capacity = 56.0,
        .thermal_conductivity = 0.14, .viscosity = 0.000158,
        .solid_form = MAT_SOLID_NITROGEN, .liquid_form = MAT_NONE, .gas_form = MAT_NITROGEN,
        .transition_temp_down = 63.15, .transition_temp_up = 77.36,  // Boils at 77K
        .enthalpy_down = 720, .enthalpy_up = 5560,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {180, 180, 240, 150}
    },
    [MAT_NITROGEN] = {
        .name = "Nitrogen", .formula = "N2",
        .phase = PHASE_GAS,
        .molar_mass = 0.028, .molar_volume = 0.0245, .molar_heat_capacity = 29.1,
        .thermal_conductivity = 0.026, .viscosity = 0.0000178,
        .solid_form = MAT_SOLID_NITROGEN, .liquid_form = MAT_LIQUID_NITROGEN, .gas_form = MAT_NONE,
        .transition_temp_down = 77.36, .transition_temp_up = 0,
        .enthalpy_down = 5560, .enthalpy_up = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {220, 220, 255, 20}
    },

    // ===== OXYGEN PHASES (O2) =====
    [MAT_SOLID_OXYGEN] = {
        .name = "Solid Oxygen", .formula = "O2",
        .phase = PHASE_SOLID,
        .molar_mass = 0.032, .molar_volume = 0.0000139, .molar_heat_capacity = 23.0,
        .thermal_conductivity = 0.17, .viscosity = 0,
        .solid_form = MAT_NONE, .liquid_form = MAT_LIQUID_OXYGEN, .gas_form = MAT_OXYGEN,
        .transition_temp_down = 0, .transition_temp_up = 54.36,  // Melts at 54K
        .enthalpy_down = 0, .enthalpy_up = 444,
        .is_oxidizer = true, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {180, 200, 255, 200}
    },
    [MAT_LIQUID_OXYGEN] = {
        .name = "Liquid Oxygen", .formula = "O2",
        .phase = PHASE_LIQUID,
        .molar_mass = 0.032, .molar_volume = 0.0000280, .molar_heat_capacity = 53.0,
        .thermal_conductivity = 0.15, .viscosity = 0.000195,
        .solid_form = MAT_SOLID_OXYGEN, .liquid_form = MAT_NONE, .gas_form = MAT_OXYGEN,
        .transition_temp_down = 54.36, .transition_temp_up = 90.19,  // Boils at 90K
        .enthalpy_down = 444, .enthalpy_up = 6820,
        .is_oxidizer = true, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {150, 180, 255, 150}
    },
    [MAT_OXYGEN] = {
        .name = "Oxygen", .formula = "O2",
        .phase = PHASE_GAS,
        .molar_mass = 0.032, .molar_volume = 0.0245, .molar_heat_capacity = 29.4,
        .thermal_conductivity = 0.027, .viscosity = 0.0000207,
        .solid_form = MAT_SOLID_OXYGEN, .liquid_form = MAT_LIQUID_OXYGEN, .gas_form = MAT_NONE,
        .transition_temp_down = 90.19, .transition_temp_up = 0,
        .enthalpy_down = 6820, .enthalpy_up = 0,
        .is_oxidizer = true, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {200, 220, 255, 20}
    },

    // ===== CARBON DIOXIDE PHASES (CO2) =====
    [MAT_DRY_ICE] = {
        .name = "Dry Ice", .formula = "CO2",
        .phase = PHASE_SOLID,
        .molar_mass = 0.044, .molar_volume = 0.0000286, .molar_heat_capacity = 47.0,
        .thermal_conductivity = 0.15, .viscosity = 0,
        .solid_form = MAT_NONE, .liquid_form = MAT_LIQUID_CO2, .gas_form = MAT_CARBON_DIOXIDE,
        .transition_temp_down = 0, .transition_temp_up = 195,  // Sublimes at 195K (1 atm)
        .enthalpy_down = 0, .enthalpy_up = 25200,  // Sublimation enthalpy
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {220, 220, 220, 200}
    },
    [MAT_LIQUID_CO2] = {
        .name = "Liquid CO2", .formula = "CO2",
        .phase = PHASE_LIQUID,
        .molar_mass = 0.044, .molar_volume = 0.0000370, .molar_heat_capacity = 85.0,
        .thermal_conductivity = 0.1, .viscosity = 0.00007,
        .solid_form = MAT_DRY_ICE, .liquid_form = MAT_NONE, .gas_form = MAT_CARBON_DIOXIDE,
        .transition_temp_down = 216.55, .transition_temp_up = 304.25,  // Triple to critical
        .enthalpy_down = 9020, .enthalpy_up = 16700,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {200, 200, 200, 150}
    },
    [MAT_CARBON_DIOXIDE] = {
        .name = "Carbon Dioxide", .formula = "CO2",
        .phase = PHASE_GAS,
        .molar_mass = 0.044, .molar_volume = 0.0245, .molar_heat_capacity = 37.1,
        .thermal_conductivity = 0.015, .viscosity = 0.0000150,
        .solid_form = MAT_DRY_ICE, .liquid_form = MAT_LIQUID_CO2, .gas_form = MAT_NONE,
        .transition_temp_down = 195, .transition_temp_up = 0,  // Deposits at 195K
        .enthalpy_down = 25200, .enthalpy_up = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color = {180, 180, 180, 30}
    },
};

// ============ CELL FUNCTIONS ============

void cell_init(Cell3D *cell) {
    memset(cell, 0, sizeof(Cell3D));
}

void cell_free(Cell3D *cell) {
    cell->present = 0;
}

Cell3D cell_clone(const Cell3D *src) {
    Cell3D dst;
    memcpy(&dst, src, sizeof(Cell3D));
    return dst;
}

void cell_add_material(Cell3D *cell, MaterialType type, double moles, double energy) {
    if (type == MAT_NONE || type >= MAT_COUNT) return;
    if (moles < MOLES_EPSILON) return;

    if (CELL_HAS_MATERIAL(cell, type)) {
        // Add to existing
        cell->materials[type].moles += moles;
        cell->materials[type].thermal_energy += energy;
    } else {
        // New material
        cell->materials[type].moles = moles;
        cell->materials[type].thermal_energy = energy;
        cell->present |= (1 << type);
    }
    cell->materials[type].temp_valid = false;
}

void cell_remove_material(Cell3D *cell, MaterialType type) {
    if (type == MAT_NONE || type >= MAT_COUNT) return;
    cell->materials[type].moles = 0;
    cell->materials[type].thermal_energy = 0;
    cell->materials[type].temp_valid = false;
    cell->present &= ~(1 << type);
}

bool cells_match(const Cell3D *a, const Cell3D *b) {
    if (a->present != b->present) return false;

    CELL_FOR_EACH_MATERIAL(a, type) {
        if (fabs(a->materials[type].moles - b->materials[type].moles) > MOLES_EPSILON)
            return false;
        if (fabs(a->materials[type].thermal_energy - b->materials[type].thermal_energy) > 1.0)
            return false;
    }
    return true;
}

// Thread-local storage for legacy API
static _Thread_local MaterialEntry tls_entry;

MaterialEntry* cell_find_material(Cell3D *cell, MaterialType type) {
    if (!CELL_HAS_MATERIAL(cell, type)) return NULL;
    tls_entry.type = type;
    tls_entry.state = cell->materials[type];
    return &tls_entry;
}

const MaterialEntry* cell_find_material_const(const Cell3D *cell, MaterialType type) {
    if (!CELL_HAS_MATERIAL(cell, type)) return NULL;
    tls_entry.type = type;
    tls_entry.state = cell->materials[type];
    return &tls_entry;
}

// ============ MATERIAL FUNCTIONS ============
// Simplified for single-phase materials.
// Phase is intrinsic to each MaterialType.

double material_get_temperature(MaterialState *state, MaterialType type) {
    // Return cached value if valid
    if (state->temp_valid) {
        return state->cached_temp;
    }

    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double n = state->moles;
    double E = state->thermal_energy;
    double Cp = props->molar_heat_capacity;

    if (n < MOLES_EPSILON || Cp < 1e-10) {
        state->cached_temp = 0.0;
        state->temp_valid = true;
        return 0.0;
    }

    // Simple: T = E / (n * Cp)
    // For single-phase materials, no phase transition plateaus
    double temp = E / (n * Cp);

    state->cached_temp = temp;
    state->temp_valid = true;
    return temp;
}

double material_get_mass(const MaterialState *state, MaterialType type) {
    return state->moles * MATERIAL_PROPS[type].molar_mass;
}

// Volume is now single-valued per material (no phase parameter needed)
double material_get_volume(const MaterialState *state, MaterialType type) {
    return state->moles * MATERIAL_PROPS[type].molar_volume;
}

// Density from first principles: ρ = molar_mass / molar_volume
// Returns density in kg/m³
double material_get_density(MaterialType type) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    if (props->molar_volume <= 0) return 0;
    return props->molar_mass / props->molar_volume;  // kg/m³
}

// ============ PHASE TRANSITION FUNCTIONS ============

// Check if material should transition to another phase based on temperature
// Returns the target MaterialType, or MAT_NONE if no transition needed
MaterialType material_check_transition(MaterialType type, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];

    // Check heating transition (to lighter phase)
    if (props->transition_temp_up > 0 && temp_k > props->transition_temp_up) {
        // For solids: melt to liquid, or sublimate to gas
        if (props->phase == PHASE_SOLID) {
            return (props->liquid_form != MAT_NONE) ? props->liquid_form : props->gas_form;
        }
        // For liquids: boil to gas
        if (props->phase == PHASE_LIQUID && props->gas_form != MAT_NONE) {
            return props->gas_form;
        }
    }

    // Check cooling transition (to denser phase)
    if (props->transition_temp_down > 0 && temp_k < props->transition_temp_down) {
        // For gases: condense to liquid, or deposit to solid
        if (props->phase == PHASE_GAS) {
            return (props->liquid_form != MAT_NONE) ? props->liquid_form : props->solid_form;
        }
        // For liquids: freeze to solid
        if (props->phase == PHASE_LIQUID && props->solid_form != MAT_NONE) {
            return props->solid_form;
        }
    }

    return MAT_NONE;  // No transition
}

// Convert material from one phase to another (conserves moles and energy)
void material_convert_phase(Cell3D *cell, MaterialType from, MaterialType to) {
    if (!CELL_HAS_MATERIAL(cell, from)) return;
    if (to == MAT_NONE || to == from) return;

    double moles = cell->materials[from].moles;
    double energy = cell->materials[from].thermal_energy;

    const MaterialProperties *props_from = &MATERIAL_PROPS[from];
    const MaterialProperties *props_to = &MATERIAL_PROPS[to];

    // Adjust energy for latent heat
    // If transitioning to lighter phase (heating), subtract latent heat absorbed
    // If transitioning to denser phase (cooling), add latent heat released
    if (props_to->phase > props_from->phase) {
        // Heating: solid->liquid or liquid->gas
        energy -= moles * props_from->enthalpy_up;
    } else if (props_to->phase < props_from->phase) {
        // Cooling: gas->liquid or liquid->solid
        energy += moles * props_from->enthalpy_down;
    }

    // Remove old material
    cell_remove_material(cell, from);

    // Add new material with adjusted energy
    cell_add_material(cell, to, moles, energy);
}

// Calculate effective density for multi-material cell
// Weighted by mass and volume: ρ = total_mass / total_volume
double cell_get_density(const Cell3D *cell) {
    if (cell->present == 0) return 0;

    double total_mass = 0;    // kg
    double total_volume = 0;  // m³

    CELL_FOR_EACH_MATERIAL(cell, type) {
        const MaterialState *state = &cell->materials[type];
        const MaterialProperties *props = &MATERIAL_PROPS[type];

        if (props->molar_volume > 0) {
            total_mass += state->moles * props->molar_mass;
            total_volume += state->moles * props->molar_volume;
        }
    }

    if (total_volume <= 0) return 0;
    return total_mass / total_volume;  // kg/m³
}

double cell_get_temperature(Cell3D *cell) {
    if (cell->present == 0) return 0.0;

    double weighted_temp_sum = 0;
    double total_heat_capacity = 0;

    CELL_FOR_EACH_MATERIAL(cell, type) {
        double temp = material_get_temperature(&cell->materials[type], type);
        double Cp = MATERIAL_PROPS[type].molar_heat_capacity;
        double hc = cell->materials[type].moles * Cp;
        weighted_temp_sum += temp * hc;
        total_heat_capacity += hc;
    }

    if (total_heat_capacity < 1e-10) return 0.0;
    return weighted_temp_sum / total_heat_capacity;
}

double cell_get_total_volume(const Cell3D *cell) {
    double total = 0;
    CELL_FOR_EACH_MATERIAL(cell, type) {
        total += material_get_volume(&cell->materials[type], type);
    }
    return total;
}

double cell_get_available_volume(const Cell3D *cell) {
    return CELL_VOLUME_CAPACITY - cell_get_total_volume(cell);
}

// ============ CHUNK FUNCTIONS ============

Chunk* chunk_create(int cx, int cy, int cz) {
    Chunk *chunk = (Chunk*)calloc(1, sizeof(Chunk));
    if (!chunk) return NULL;

    chunk->cx = cx;
    chunk->cy = cy;
    chunk->cz = cz;

    // Initialize all cells
    for (int i = 0; i < CHUNK_VOLUME; i++) {
        cell_init(&chunk->cells[i]);
    }

    // No neighbors initially
    for (int i = 0; i < DIR_COUNT; i++) {
        chunk->neighbors[i] = NULL;
    }

    // Reset dirty region (empty)
    chunk->dirty_min_x = CHUNK_SIZE;
    chunk->dirty_min_y = CHUNK_SIZE;
    chunk->dirty_min_z = CHUNK_SIZE;
    chunk->dirty_max_x = 0;
    chunk->dirty_max_y = 0;
    chunk->dirty_max_z = 0;

    chunk->is_active = false;
    chunk->is_stable = false;
    chunk->stable_frames = 0;
    chunk->active_list_idx = -1;
    chunk->hash_next = NULL;

    return chunk;
}

void chunk_free(Chunk *chunk) {
    if (!chunk) return;
    // Cells are embedded, no separate free needed
    free(chunk);
}

Cell3D* chunk_get_neighbor_cell(Chunk *chunk, int lx, int ly, int lz, Direction dir) {
    int nx = lx + DIR_DX[dir];
    int ny = ly + DIR_DY[dir];
    int nz = lz + DIR_DZ[dir];

    // Same chunk - direct access O(1)
    if (nx >= 0 && nx < CHUNK_SIZE &&
        ny >= 0 && ny < CHUNK_SIZE &&
        nz >= 0 && nz < CHUNK_SIZE) {
        return chunk_get_cell(chunk, nx, ny, nz);
    }

    // Cross chunk - use cached neighbor pointer O(1)
    Chunk *neighbor_chunk = chunk->neighbors[dir];
    if (!neighbor_chunk) return NULL;

    // Wrap coordinates to neighbor chunk
    if (nx < 0) nx = CHUNK_SIZE - 1;
    else if (nx >= CHUNK_SIZE) nx = 0;

    if (ny < 0) ny = CHUNK_SIZE - 1;
    else if (ny >= CHUNK_SIZE) ny = 0;

    if (nz < 0) nz = CHUNK_SIZE - 1;
    else if (nz >= CHUNK_SIZE) nz = 0;

    return chunk_get_cell(neighbor_chunk, nx, ny, nz);
}

void chunk_mark_dirty(Chunk *chunk, int lx, int ly, int lz) {
    if (!chunk->is_active) {
        chunk->is_active = true;
        chunk->dirty_min_x = chunk->dirty_max_x = lx;
        chunk->dirty_min_y = chunk->dirty_max_y = ly;
        chunk->dirty_min_z = chunk->dirty_max_z = lz;
    } else {
        if (lx < chunk->dirty_min_x) chunk->dirty_min_x = lx;
        if (lx > chunk->dirty_max_x) chunk->dirty_max_x = lx;
        if (ly < chunk->dirty_min_y) chunk->dirty_min_y = ly;
        if (ly > chunk->dirty_max_y) chunk->dirty_max_y = ly;
        if (lz < chunk->dirty_min_z) chunk->dirty_min_z = lz;
        if (lz > chunk->dirty_max_z) chunk->dirty_max_z = lz;
    }
    chunk->stable_frames = 0;
    chunk->is_stable = false;
}

void chunk_reset_dirty(Chunk *chunk) {
    // Note: is_active is managed by world_add_to_active_list, not here.
    // We only reset the dirty region bounds.
    chunk->dirty_min_x = CHUNK_SIZE;
    chunk->dirty_min_y = CHUNK_SIZE;
    chunk->dirty_min_z = CHUNK_SIZE;
    chunk->dirty_max_x = 0;
    chunk->dirty_max_y = 0;
    chunk->dirty_max_z = 0;
}

void chunk_check_equilibrium(Chunk *chunk) {
    if (!chunk->is_active) {
        chunk->stable_frames++;
        if (chunk->stable_frames >= EQUILIBRIUM_FRAMES) {
            chunk->is_stable = true;
        }
    }
}
