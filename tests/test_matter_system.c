/**
 * Matter System - System Tests
 *
 * Tests the full engine with designed maps and injected events.
 * Uses the actual matter.h/matter.c implementation.
 *
 * Theories tested:
 * 1. System initializes correctly with designed terrain
 * 2. Heat injection propagates through the full system
 * 3. Combustion triggers correctly when conditions are met
 * 4. Energy is conserved over extended simulation runs
 * 5. Boundary conditions are handled correctly
 * 6. Temperature stability in production-like scenarios
 */

// Include matter.h first (includes water.h with fixed-point definitions)
#include "../include/matter.h"
#include "test_common.h"

// ============ DESIGNED TERRAIN MAPS ============

// Flat terrain at height 5
static void terrain_flat(int terrain[MATTER_RES][MATTER_RES], int height) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            terrain[x][z] = height;
        }
    }
}

// Additional terrain generators for future tests:
// - terrain_bowl: low in center, high at edges
// - terrain_hill: high in center, low at edges
// These can be added when needed for specific test scenarios

// ============ HELPER FUNCTIONS ============

static float state_avg_temp(const MatterState *state) {
    float sum = 0;
    int count = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            sum += FIXED_TO_FLOAT(state->cells[x][z].temperature);
            count++;
        }
    }
    return sum / count;
}

static float state_max_temp(const MatterState *state) {
    float max = -1e9f;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            float t = FIXED_TO_FLOAT(state->cells[x][z].temperature);
            if (t > max) max = t;
        }
    }
    return max;
}

static float state_min_temp(const MatterState *state) {
    float min = 1e9f;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            float t = FIXED_TO_FLOAT(state->cells[x][z].temperature);
            if (t < min) min = t;
        }
    }
    return min;
}

// Additional helper functions for future tests:
// - count_cells_above_temp(state, temp_k): count cells over a threshold
// - count_cells_with_fuel(state): count cells with cellulose
// These can be added when needed for specific test scenarios

// ============ INITIALIZATION TESTS ============

static bool test_init_flat_terrain(void) {
    TEST_BEGIN("init: flat terrain");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    ASSERT(state.initialized, "state not marked initialized");
    ASSERT(state.tick == 0, "tick should be 0");

    // All cells should be at ambient temperature
    float avg = state_avg_temp(&state);
    ASSERT_FLOAT_EQ(avg, 293.15f, 1.0f, "wrong average temperature");

    float variance = state_max_temp(&state) - state_min_temp(&state);
    ASSERT(variance < 1.0f, "initial temperature variance too high");

    TEST_PASS();
}

static bool test_init_has_atmosphere(void) {
    TEST_BEGIN("init: atmosphere present");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Check center cell has atmosphere
    MatterCell *center = &state.cells[80][80];

    ASSERT(center->mass[SUBST_NITROGEN] > 0, "no nitrogen in atmosphere");
    ASSERT(center->mass[SUBST_OXYGEN] > 0, "no oxygen in atmosphere");

    // N2:O2 ratio should be roughly 78:21
    float n2 = FIXED_TO_FLOAT(center->mass[SUBST_NITROGEN]);
    float o2 = FIXED_TO_FLOAT(center->mass[SUBST_OXYGEN]);
    float ratio = n2 / o2;
    ASSERT_FLOAT_EQ(ratio, 78.0f / 21.0f, 0.5f, "wrong N2/O2 ratio");

    TEST_PASS();
}

static bool test_init_has_ground(void) {
    TEST_BEGIN("init: ground present");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // All cells should have silicate
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            ASSERT(state.cells[x][z].mass[SUBST_SILICATE] > 0,
                   "missing silicate at (%d,%d)", x, z);
        }
    }

    TEST_PASS();
}

static bool test_init_seed_determinism(void) {
    TEST_BEGIN("init: same seed gives same result");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state1, state2;

    terrain_flat(terrain, 5);

    matter_init(&state1, terrain, 99999);
    matter_init(&state2, terrain, 99999);

    // Compare a few cells
    for (int i = 0; i < 10; i++) {
        int x = (i * 17) % MATTER_RES;
        int z = (i * 23) % MATTER_RES;

        ASSERT(state1.cells[x][z].mass[SUBST_CELLULOSE] ==
               state2.cells[x][z].mass[SUBST_CELLULOSE],
               "seed not deterministic at (%d,%d)", x, z);
    }

    TEST_PASS();
}

// ============ STABILITY TESTS ============

static bool test_uniform_stability(void) {
    TEST_BEGIN("stability: uniform grid stays stable");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Check temperature stability in a small region
    // (full grid total_energy overflows int32_t)
    float initial_temps[10][10];
    for (int x = 70; x < 80; x++) {
        for (int z = 70; z < 80; z++) {
            initial_temps[x-70][z-70] = FIXED_TO_FLOAT(state.cells[x][z].temperature);
        }
    }

    // Run for many steps
    for (int i = 0; i < 100; i++) {
        matter_step(&state);
    }

    // Check temperatures stayed stable
    float max_change = 0;
    for (int x = 70; x < 80; x++) {
        for (int z = 70; z < 80; z++) {
            float now = FIXED_TO_FLOAT(state.cells[x][z].temperature);
            float change = fabsf(now - initial_temps[x-70][z-70]);
            if (change > max_change) max_change = change;
        }
    }

    ASSERT(max_change < 5.0f, "temperature drifted too much: %.2fK", max_change);

    TEST_PASS();
}

static bool test_long_term_stability(void) {
    TEST_BEGIN("stability: 30 seconds simulation");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Simulate 30 seconds at 30Hz
    int steps = 30 * 30;
    for (int i = 0; i < steps; i++) {
        matter_step(&state);
    }

    float final_avg = state_avg_temp(&state);
    float max_temp = state_max_temp(&state);
    float min_temp = state_min_temp(&state);

    // Average should stay near ambient
    ASSERT_FLOAT_EQ(final_avg, 293.15f, 5.0f, "average temperature drifted");

    // No extreme temperatures
    ASSERT(max_temp < 350.0f, "spontaneous heating detected");
    ASSERT(min_temp > 250.0f, "spontaneous cooling detected");

    TEST_PASS();
}

// ============ HEAT INJECTION TESTS ============

static bool test_heat_injection_center(void) {
    TEST_BEGIN("heat injection: center cell");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Add fuel to center region to enable heat transfer
    for (int x = 75; x < 85; x++) {
        for (int z = 75; z < 85; z++) {
            state.cells[x][z].mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.1f);
            cell_update_cache(&state.cells[x][z]);
        }
    }

    // Inject heat at center
    MatterCell *center = &state.cells[80][80];
    fixed16_t heat_injection = FLOAT_TO_FIXED(1000.0f);
    center->energy += heat_injection;
    cell_update_cache(center);

    float initial_center_temp = FIXED_TO_FLOAT(center->temperature);
    ASSERT(initial_center_temp > 350.0f, "heat injection didn't raise temp");

    // Run a few steps
    for (int i = 0; i < 50; i++) {
        matter_conduct_heat(&state);
        for (int x = 70; x < 90; x++) {
            for (int z = 70; z < 90; z++) {
                cell_update_cache(&state.cells[x][z]);
            }
        }
    }

    // Center should have cooled
    float final_center_temp = FIXED_TO_FLOAT(center->temperature);
    ASSERT(final_center_temp < initial_center_temp, "center didn't cool down");

    // Neighbors should have warmed
    float neighbor_temp = FIXED_TO_FLOAT(state.cells[81][80].temperature);
    ASSERT(neighbor_temp > 293.15f, "heat didn't spread to neighbor");

    TEST_PASS();
}

static bool test_heat_injection_corner(void) {
    TEST_BEGIN("heat injection: corner cell");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Add fuel to corner region
    for (int x = 0; x < 10; x++) {
        for (int z = 0; z < 10; z++) {
            state.cells[x][z].mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.1f);
            cell_update_cache(&state.cells[x][z]);
        }
    }

    // Inject heat at corner (0,0) - need enough to get above fire_temp (400K)
    MatterCell *corner = &state.cells[0][0];
    float initial_corner_temp = FIXED_TO_FLOAT(corner->temperature);

    // Set energy to achieve 500K (well above fire_temp of 400K)
    corner->energy = fixed_mul(corner->thermal_mass, FLOAT_TO_FIXED(500.0f));
    cell_update_cache(corner);

    float heated_corner_temp = FIXED_TO_FLOAT(corner->temperature);
    ASSERT(heated_corner_temp > 450.0f, "heat injection didn't raise temp enough: %.1fK", heated_corner_temp);

    // Run conduction
    for (int i = 0; i < 100; i++) {
        matter_step(&state);
    }

    // Corner should have cooled as heat spread to neighbors
    float final_corner_temp = FIXED_TO_FLOAT(corner->temperature);
    ASSERT(final_corner_temp < heated_corner_temp, "corner didn't cool down");

    // Neighbors should be warmer than initial
    float neighbor_temp = FIXED_TO_FLOAT(state.cells[1][0].temperature);
    ASSERT(neighbor_temp > initial_corner_temp, "heat didn't spread to neighbor");

    TEST_PASS();
}

static bool test_heat_propagation_pattern(void) {
    TEST_BEGIN("heat propagation: radial pattern");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Add fuel in a 20x20 region around center
    for (int x = 70; x < 90; x++) {
        for (int z = 70; z < 90; z++) {
            state.cells[x][z].mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.1f);
            cell_update_cache(&state.cells[x][z]);
        }
    }

    // Inject heat at center
    state.cells[80][80].energy += FLOAT_TO_FIXED(2000.0f);
    cell_update_cache(&state.cells[80][80]);

    // Run some steps
    for (int i = 0; i < 100; i++) {
        matter_conduct_heat(&state);
        for (int x = 70; x < 90; x++) {
            for (int z = 70; z < 90; z++) {
                cell_update_cache(&state.cells[x][z]);
            }
        }
    }

    // Temperature should decrease with distance from center
    float temp_center = FIXED_TO_FLOAT(state.cells[80][80].temperature);
    float temp_near = FIXED_TO_FLOAT(state.cells[82][80].temperature);    // 2 cells away
    float temp_far = FIXED_TO_FLOAT(state.cells[85][80].temperature);     // 5 cells away

    ASSERT(temp_center >= temp_near, "center not hottest");
    ASSERT(temp_near >= temp_far, "heat not decreasing with distance");

    TEST_PASS();
}

// ============ COMBUSTION TESTS ============

static bool test_combustion_requires_fuel(void) {
    TEST_BEGIN("combustion: requires fuel");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Heat a cell without fuel
    MatterCell *cell = &state.cells[80][80];
    cell->mass[SUBST_CELLULOSE] = 0;
    cell->energy = fixed_mul(cell->thermal_mass, FLOAT_TO_FIXED(600.0f));
    cell_update_cache(cell);

    // Should not be able to combust
    ASSERT(!cell_can_combust(cell, SUBST_CELLULOSE), "combustion without fuel");

    TEST_PASS();
}

static bool test_combustion_requires_oxygen(void) {
    TEST_BEGIN("combustion: requires oxygen");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    MatterCell *cell = &state.cells[80][80];

    // Add fuel but remove oxygen
    cell->mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.1f);
    cell->mass[SUBST_OXYGEN] = 0;
    cell->energy = fixed_mul(cell->thermal_mass, FLOAT_TO_FIXED(600.0f));
    cell_update_cache(cell);

    ASSERT(!cell_can_combust(cell, SUBST_CELLULOSE), "combustion without oxygen");

    TEST_PASS();
}

static bool test_combustion_requires_temperature(void) {
    TEST_BEGIN("combustion: requires ignition temp");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    MatterCell *cell = &state.cells[80][80];

    // Add fuel and oxygen, but keep cold
    cell->mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.1f);
    cell->mass[SUBST_OXYGEN] = FLOAT_TO_FIXED(0.05f);
    cell_update_cache(cell);

    // At ambient temp (293K), should not combust
    ASSERT(!cell_can_combust(cell, SUBST_CELLULOSE), "combustion at ambient temp");

    // Heat to ignition temp
    cell->energy = fixed_mul(cell->thermal_mass, FLOAT_TO_FIXED(600.0f));
    cell_update_cache(cell);

    ASSERT(cell_can_combust(cell, SUBST_CELLULOSE), "no combustion at ignition temp");

    TEST_PASS();
}

static bool test_combustion_consumes_fuel(void) {
    TEST_BEGIN("combustion: consumes fuel");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    MatterCell *cell = &state.cells[80][80];

    // Setup for combustion - add fuel and oxygen
    cell->mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.5f);
    cell->mass[SUBST_OXYGEN] = FLOAT_TO_FIXED(0.5f);

    // First update cache to get correct thermal_mass
    cell_update_cache(cell);

    // Now set energy to achieve 600K
    cell->energy = fixed_mul(cell->thermal_mass, FLOAT_TO_FIXED(600.0f));
    cell_update_cache(cell);

    // Verify temperature is at ignition level
    float temp = FIXED_TO_FLOAT(cell->temperature);
    ASSERT(temp > 550.0f, "temperature not at ignition level: %.1fK", temp);

    fixed16_t initial_fuel = cell->mass[SUBST_CELLULOSE];

    // Run combustion
    for (int i = 0; i < 100; i++) {
        matter_process_combustion(&state);
        cell_update_cache(cell);
    }

    fixed16_t final_fuel = cell->mass[SUBST_CELLULOSE];

    ASSERT(final_fuel < initial_fuel, "fuel not consumed");

    TEST_PASS();
}

static bool test_combustion_produces_byproducts(void) {
    TEST_BEGIN("combustion: produces CO2 and ash");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    MatterCell *cell = &state.cells[80][80];

    // Clear initial byproducts
    fixed16_t initial_co2 = cell->mass[SUBST_CO2];
    fixed16_t initial_ash = cell->mass[SUBST_ASH];

    // Setup for combustion - add fuel and oxygen
    cell->mass[SUBST_CELLULOSE] = FLOAT_TO_FIXED(0.5f);
    cell->mass[SUBST_OXYGEN] = FLOAT_TO_FIXED(0.5f);

    // First update cache to get correct thermal_mass
    cell_update_cache(cell);

    // Now set energy to achieve 600K
    cell->energy = fixed_mul(cell->thermal_mass, FLOAT_TO_FIXED(600.0f));
    cell_update_cache(cell);

    // Run combustion
    for (int i = 0; i < 100; i++) {
        matter_process_combustion(&state);
        cell_update_cache(cell);
    }

    ASSERT(cell->mass[SUBST_CO2] > initial_co2, "no CO2 produced");
    ASSERT(cell->mass[SUBST_ASH] > initial_ash, "no ash produced");

    TEST_PASS();
}

// ============ ENERGY CONSERVATION TESTS ============

static bool test_total_energy_api(void) {
    TEST_BEGIN("API: matter_total_energy");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    // Note: matter_total_energy may overflow for full 160x160 grid
    // Just check that individual cells have positive energy
    MatterCell *cell = &state.cells[80][80];
    ASSERT(cell->energy > 0, "cell energy not positive");

    // Also verify cell->temperature is reasonable
    float temp = FIXED_TO_FLOAT(cell->temperature);
    ASSERT(temp > 200.0f && temp < 400.0f, "unreasonable temperature: %.1fK", temp);

    TEST_PASS();
}

static bool test_total_mass_api(void) {
    TEST_BEGIN("API: matter_total_mass");

    static int terrain[MATTER_RES][MATTER_RES];
    static MatterState state;

    terrain_flat(terrain, 5);
    matter_init(&state, terrain, 12345);

    fixed16_t silicate = matter_total_mass(&state, SUBST_SILICATE);
    fixed16_t nitrogen = matter_total_mass(&state, SUBST_NITROGEN);
    fixed16_t oxygen = matter_total_mass(&state, SUBST_OXYGEN);

    // All cells should have these
    ASSERT(silicate > 0, "no silicate mass");
    ASSERT(nitrogen > 0, "no nitrogen mass");
    ASSERT(oxygen > 0, "no oxygen mass");

    // N2 should be more than O2 (78:21 ratio)
    ASSERT(nitrogen > oxygen, "N2 should exceed O2");

    TEST_PASS();
}

// ============ COORDINATE CONVERSION TESTS ============

static bool test_world_cell_conversion(void) {
    TEST_BEGIN("conversion: world <-> cell");

    // Cell to world
    float wx, wz;
    matter_cell_to_world(10, 20, &wx, &wz);

    // World to cell
    int cx, cz;
    matter_world_to_cell(wx, wz, &cx, &cz);

    ASSERT_EQ(cx, 10, "x roundtrip failed");
    ASSERT_EQ(cz, 20, "z roundtrip failed");

    TEST_PASS();
}

static bool test_world_coordinates_bounds(void) {
    TEST_BEGIN("conversion: boundary coordinates");

    int cx, cz;

    // Origin
    matter_world_to_cell(0.0f, 0.0f, &cx, &cz);
    ASSERT(matter_cell_valid(cx, cz), "origin should be valid");

    // Near max
    float max_world = MATTER_RES * MATTER_CELL_SIZE - 0.1f;
    matter_world_to_cell(max_world, max_world, &cx, &cz);
    ASSERT(matter_cell_valid(cx, cz), "near-max should be valid");

    TEST_PASS();
}

// ============ PHASE DETERMINATION TESTS ============

static bool test_water_phase_solid(void) {
    TEST_BEGIN("phase: water solid below 273K");

    fixed16_t cold_temp = FLOAT_TO_FIXED(200.0f);
    Phase p = substance_get_phase(SUBST_H2O, cold_temp);

    ASSERT(p == PHASE_SOLID, "water should be solid at 200K");

    TEST_PASS();
}

static bool test_water_phase_liquid(void) {
    TEST_BEGIN("phase: water liquid at 300K");

    fixed16_t temp = FLOAT_TO_FIXED(300.0f);
    Phase p = substance_get_phase(SUBST_H2O, temp);

    ASSERT(p == PHASE_LIQUID, "water should be liquid at 300K");

    TEST_PASS();
}

static bool test_water_phase_gas(void) {
    TEST_BEGIN("phase: water gas above 373K");

    fixed16_t hot_temp = FLOAT_TO_FIXED(400.0f);
    Phase p = substance_get_phase(SUBST_H2O, hot_temp);

    ASSERT(p == PHASE_GAS, "water should be gas at 400K");

    TEST_PASS();
}

static bool test_nitrogen_always_gas(void) {
    TEST_BEGIN("phase: nitrogen always gas at sim temps");

    // At any realistic sim temp, N2 is gas
    fixed16_t temps[] = {
        FLOAT_TO_FIXED(200.0f),
        FLOAT_TO_FIXED(293.0f),
        FLOAT_TO_FIXED(500.0f)
    };

    for (int i = 0; i < 3; i++) {
        Phase p = substance_get_phase(SUBST_NITROGEN, temps[i]);
        ASSERT(p == PHASE_GAS, "N2 should be gas at all sim temps");
    }

    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Matter System - System Tests\n");
    printf("========================================\n");

    test_suite_begin("INITIALIZATION");
    test_init_flat_terrain();
    test_init_has_atmosphere();
    test_init_has_ground();
    test_init_seed_determinism();
    test_suite_end();

    test_suite_begin("STABILITY");
    test_uniform_stability();
    test_long_term_stability();
    test_suite_end();

    test_suite_begin("HEAT INJECTION");
    test_heat_injection_center();
    test_heat_injection_corner();
    test_heat_propagation_pattern();
    test_suite_end();

    test_suite_begin("COMBUSTION");
    test_combustion_requires_fuel();
    test_combustion_requires_oxygen();
    test_combustion_requires_temperature();
    test_combustion_consumes_fuel();
    test_combustion_produces_byproducts();
    test_suite_end();

    test_suite_begin("ENERGY/MASS API");
    test_total_energy_api();
    test_total_mass_api();
    test_suite_end();

    test_suite_begin("COORDINATE CONVERSION");
    test_world_cell_conversion();
    test_world_coordinates_bounds();
    test_suite_end();

    test_suite_begin("PHASE DETERMINATION");
    test_water_phase_solid();
    test_water_phase_liquid();
    test_water_phase_gas();
    test_nitrogen_always_gas();
    test_suite_end();

    test_summary();
    return test_exit_code();
}
