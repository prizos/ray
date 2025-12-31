#include "chunk.h"
#include <stdio.h>
#include <string.h>

// ============ MATERIAL PROPERTIES TABLE ============

const MaterialProperties MATERIAL_PROPS[MAT_COUNT] = {
    [MAT_NONE] = {
        .name = "None", .formula = "-",
        .molar_mass = 0, .molar_volume_solid = 0, .molar_volume_liquid = 0, .molar_volume_gas = 0,
        .molar_heat_capacity_solid = 1, .molar_heat_capacity_liquid = 1, .molar_heat_capacity_gas = 1,
        .melting_point = 0, .boiling_point = 0, .enthalpy_fusion = 0, .enthalpy_vaporization = 0,
        .thermal_conductivity = 0, .viscosity = 0,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {0, 0, 0, 0}, .color_liquid = {0, 0, 0, 0}, .color_gas = {0, 0, 0, 0}
    },
    [MAT_AIR] = {
        .name = "Air", .formula = "N2/O2",
        .molar_mass = 0.029, .molar_volume_solid = 0, .molar_volume_liquid = 0, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 29, .molar_heat_capacity_liquid = 29, .molar_heat_capacity_gas = 29,
        .melting_point = 60, .boiling_point = 80, .enthalpy_fusion = 720, .enthalpy_vaporization = 5600,
        .thermal_conductivity = 0.026, .viscosity = 0.000018,
        .is_oxidizer = true, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {200, 220, 255, 50}, .color_liquid = {180, 200, 240, 100}, .color_gas = {135, 206, 235, 30}
    },
    [MAT_WATER] = {
        .name = "Water", .formula = "H2O",
        .molar_mass = 0.018, .molar_volume_solid = 0.0000196, .molar_volume_liquid = 0.000018, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 38, .molar_heat_capacity_liquid = 75.3, .molar_heat_capacity_gas = 33.6,
        .melting_point = 273.15, .boiling_point = 373.15, .enthalpy_fusion = 6010, .enthalpy_vaporization = 40660,
        .thermal_conductivity = 0.6, .viscosity = 0.001,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {200, 220, 255, 200}, .color_liquid = {64, 164, 223, 180}, .color_gas = {220, 220, 220, 100}
    },
    [MAT_ROCK] = {
        .name = "Rock", .formula = "SiO2",
        .molar_mass = 0.060, .molar_volume_solid = 0.0000227, .molar_volume_liquid = 0.0000273, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 44.4, .molar_heat_capacity_liquid = 82.6, .molar_heat_capacity_gas = 47.4,
        .melting_point = 1986, .boiling_point = 2503, .enthalpy_fusion = 9600, .enthalpy_vaporization = 520000,
        .thermal_conductivity = 1.4, .viscosity = 10000000,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {128, 128, 128, 255}, .color_liquid = {255, 100, 50, 255}, .color_gas = {200, 150, 100, 100}
    },
    [MAT_DIRT] = {
        .name = "Dirt", .formula = "soil",
        .molar_mass = 0.050, .molar_volume_solid = 0.00002, .molar_volume_liquid = 0.000025, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 40, .molar_heat_capacity_liquid = 60, .molar_heat_capacity_gas = 40,
        .melting_point = 1500, .boiling_point = 2500, .enthalpy_fusion = 8000, .enthalpy_vaporization = 400000,
        .thermal_conductivity = 0.5, .viscosity = 5000000,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {139, 90, 43, 255}, .color_liquid = {180, 100, 50, 255}, .color_gas = {150, 120, 80, 100}
    },
    [MAT_NITROGEN] = {
        .name = "Nitrogen", .formula = "N2",
        .molar_mass = 0.028, .molar_volume_solid = 0.0000159, .molar_volume_liquid = 0.0000347, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 25.7, .molar_heat_capacity_liquid = 56.0, .molar_heat_capacity_gas = 29.1,
        .melting_point = 63.15, .boiling_point = 77.36, .enthalpy_fusion = 720, .enthalpy_vaporization = 5560,
        .thermal_conductivity = 0.026, .viscosity = 0.0000178,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {200, 200, 255, 200}, .color_liquid = {180, 180, 240, 150}, .color_gas = {220, 220, 255, 20}
    },
    [MAT_OXYGEN] = {
        .name = "Oxygen", .formula = "O2",
        .molar_mass = 0.032, .molar_volume_solid = 0.0000139, .molar_volume_liquid = 0.0000280, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 23.0, .molar_heat_capacity_liquid = 53.0, .molar_heat_capacity_gas = 29.4,
        .melting_point = 54.36, .boiling_point = 90.19, .enthalpy_fusion = 444, .enthalpy_vaporization = 6820,
        .thermal_conductivity = 0.027, .viscosity = 0.0000207,
        .is_oxidizer = true, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {180, 200, 255, 200}, .color_liquid = {150, 180, 255, 150}, .color_gas = {200, 220, 255, 20}
    },
    [MAT_CARBON_DIOXIDE] = {
        .name = "Carbon Dioxide", .formula = "CO2",
        .molar_mass = 0.044, .molar_volume_solid = 0.0000286, .molar_volume_liquid = 0.0000370, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 47.0, .molar_heat_capacity_liquid = 85.0, .molar_heat_capacity_gas = 37.1,
        .melting_point = 216.55, .boiling_point = 194.65, .enthalpy_fusion = 9020, .enthalpy_vaporization = 16700,
        .thermal_conductivity = 0.015, .viscosity = 0.0000150,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {220, 220, 220, 200}, .color_liquid = {200, 200, 200, 150}, .color_gas = {180, 180, 180, 30}
    },
    [MAT_STEAM] = {
        .name = "Steam", .formula = "H2O(g)",
        .molar_mass = 0.018, .molar_volume_solid = 0.0000196, .molar_volume_liquid = 0.000018, .molar_volume_gas = 0.0224,
        .molar_heat_capacity_solid = 38, .molar_heat_capacity_liquid = 75.3, .molar_heat_capacity_gas = 33.6,
        .melting_point = 273.15, .boiling_point = 373.15, .enthalpy_fusion = 6010, .enthalpy_vaporization = 40660,
        .thermal_conductivity = 0.025, .viscosity = 0.000013,
        .is_oxidizer = false, .is_fuel = false, .ignition_temp = 0, .enthalpy_combustion = 0,
        .color_solid = {200, 220, 255, 200}, .color_liquid = {64, 164, 223, 180}, .color_gas = {240, 240, 240, 80}
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

// ============ ENERGY THRESHOLD CALCULATION ============

static void calculate_energy_thresholds(double n, MaterialType type,
                                         double *E_melt_start, double *E_melt_end,
                                         double *E_boil_start, double *E_boil_end) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double Cp_s = props->molar_heat_capacity_solid;
    double Cp_l = props->molar_heat_capacity_liquid;
    double Tm = props->melting_point;
    double Tb = props->boiling_point;
    double Hf = props->enthalpy_fusion;
    double Hv = props->enthalpy_vaporization;

    *E_melt_start = n * Cp_s * Tm;
    *E_melt_end = *E_melt_start + n * Hf;
    *E_boil_start = *E_melt_end + n * Cp_l * (Tb - Tm);
    *E_boil_end = *E_boil_start + n * Hv;
}

// ============ MATERIAL FUNCTIONS ============

double material_get_temperature(MaterialState *state, MaterialType type) {
    // Return cached value if valid
    if (state->temp_valid) {
        return state->cached_temp;
    }

    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double n = state->moles;
    double E = state->thermal_energy;
    double Cp_s = props->molar_heat_capacity_solid;
    double Cp_l = props->molar_heat_capacity_liquid;
    double Cp_g = props->molar_heat_capacity_gas;

    if (n < MOLES_EPSILON || Cp_s < 1e-10) {
        state->cached_temp = 0.0;
        state->temp_valid = true;
        return 0.0;
    }

    if (E < 0) {
        state->cached_temp = E / (n * Cp_s);
        state->temp_valid = true;
        return state->cached_temp;
    }

    double E_melt_start, E_melt_end, E_boil_start, E_boil_end;
    calculate_energy_thresholds(n, type, &E_melt_start, &E_melt_end, &E_boil_start, &E_boil_end);

    double Tm = props->melting_point;
    double Tb = props->boiling_point;
    double temp;

    if (E < E_melt_start) {
        temp = E / (n * Cp_s);
    } else if (E < E_melt_end) {
        temp = Tm;
    } else if (E < E_boil_start) {
        temp = Tm + (E - E_melt_end) / (n * Cp_l);
    } else if (E < E_boil_end) {
        temp = Tb;
    } else {
        temp = Tb + (E - E_boil_end) / (n * Cp_g);
    }

    state->cached_temp = temp;
    state->temp_valid = true;
    return temp;
}

Phase material_get_phase(MaterialType type, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    if (temp_k < props->melting_point) return PHASE_SOLID;
    if (temp_k < props->boiling_point) return PHASE_LIQUID;
    return PHASE_GAS;
}

Phase material_get_phase_from_energy(const MaterialState *state, MaterialType type) {
    double n = state->moles;
    double E = state->thermal_energy;

    if (n < MOLES_EPSILON) return PHASE_GAS;

    double E_melt_start, E_melt_end, E_boil_start, E_boil_end;
    calculate_energy_thresholds(n, type, &E_melt_start, &E_melt_end, &E_boil_start, &E_boil_end);

    if (E < E_melt_end) return PHASE_SOLID;
    if (E < E_boil_end) return PHASE_LIQUID;
    return PHASE_GAS;
}

double get_effective_heat_capacity(const MaterialState *state, MaterialType type) {
    Phase phase = material_get_phase_from_energy(state, type);
    const MaterialProperties *props = &MATERIAL_PROPS[type];

    switch (phase) {
        case PHASE_SOLID: return props->molar_heat_capacity_solid;
        case PHASE_LIQUID: return props->molar_heat_capacity_liquid;
        case PHASE_GAS: return props->molar_heat_capacity_gas;
    }
    return props->molar_heat_capacity_solid;
}

double material_get_mass(const MaterialState *state, MaterialType type) {
    return state->moles * MATERIAL_PROPS[type].molar_mass;
}

double material_get_volume(const MaterialState *state, MaterialType type, Phase phase) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    switch (phase) {
        case PHASE_SOLID: return state->moles * props->molar_volume_solid;
        case PHASE_LIQUID: return state->moles * props->molar_volume_liquid;
        case PHASE_GAS: return state->moles * props->molar_volume_gas;
    }
    return 0;
}

double cell_get_temperature(Cell3D *cell) {
    if (cell->present == 0) return 0.0;

    double weighted_temp_sum = 0;
    double total_heat_capacity = 0;

    CELL_FOR_EACH_MATERIAL(cell, type) {
        double temp = material_get_temperature(&cell->materials[type], type);
        double Cp = get_effective_heat_capacity(&cell->materials[type], type);
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
        Phase phase = material_get_phase_from_energy(&cell->materials[type], type);
        total += material_get_volume(&cell->materials[type], type, phase);
    }
    return total;
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
    chunk->is_active = false;
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
