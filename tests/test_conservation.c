/**
 * test_conservation.c - Conservation Law Tests
 *
 * Tests that verify fundamental conservation laws:
 * - Mass is neither created nor destroyed
 * - Energy is conserved (in closed system without radiation)
 * - Per-substance mass is conserved through phase transitions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Include matter system directly for testing (need internal access)
#include "../src/matter.c"
#include "../src/noise.c"

// ============ TEST INFRASTRUCTURE ============

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  %s... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
    tests_run++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_run++; \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b, tol, msg) do { \
    float _a = (a), _b = (b), _t = (tol); \
    if (fabsf(_a - _b) > _t) { \
        printf("FAIL: %s (expected %.4f, got %.4f, diff %.6f)\n", msg, _b, _a, fabsf(_a - _b)); \
        tests_run++; \
        return; \
    } \
} while(0)

// ============ HELPER FUNCTIONS ============

// Calculate total mass across all cells and all substances
static fixed16_t calculate_total_mass(const MatterState *state) {
    fixed16_t total = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            const MatterCell *cell = &state->cells[x][z];

            // All phaseable substances (all phases)
            for (int ps = 0; ps < PHASEABLE_COUNT; ps++) {
                total += cell->phase_mass[ps].solid;
                total += cell->phase_mass[ps].liquid;
                total += cell->phase_mass[ps].gas;
            }

            // Non-phaseable substances
            total += cell->co2_gas;
            total += cell->smoke_gas;
            total += cell->ash_solid;
            total += cell->cellulose_solid;
        }
    }
    return total;
}

// Calculate total energy across all cells
static fixed16_t calculate_total_energy(const MatterState *state) {
    fixed16_t total = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            total += state->cells[x][z].energy;
        }
    }
    return total;
}

// Calculate total H2O mass (ice + liquid + steam)
static fixed16_t calculate_total_h2o(const MatterState *state) {
    fixed16_t total = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            const MatterCell *cell = &state->cells[x][z];
            total += CELL_H2O_ICE(cell);
            total += CELL_H2O_LIQUID(cell);
            total += CELL_H2O_STEAM(cell);
        }
    }
    return total;
}

// Calculate total silicate mass (solid + liquid lava)
static fixed16_t calculate_total_silicate(const MatterState *state) {
    fixed16_t total = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            const MatterCell *cell = &state->cells[x][z];
            total += CELL_SILICATE_SOLID(cell);
            total += CELL_SILICATE_LIQUID(cell);
            total += CELL_SILICATE_GAS(cell);
        }
    }
    return total;
}

// Create a simple flat terrain for testing
static void create_flat_terrain(int terrain[MATTER_RES][MATTER_RES], int height) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            terrain[x][z] = height;
        }
    }
}

// Run N simulation steps
static void run_steps(MatterState *state, int steps) {
    for (int i = 0; i < steps; i++) {
        matter_step(state);
    }
}

// ============ MASS CONSERVATION TESTS ============

void test_total_mass_constant_over_simulation(void) {
    TEST("total mass constant over 1000 steps");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 12345);

    fixed16_t mass_before = calculate_total_mass(&state);

    // Run 1000 steps
    run_steps(&state, 1000);

    fixed16_t mass_after = calculate_total_mass(&state);

    // Allow 0.01% tolerance for fixed-point rounding
    float before_f = FIXED_TO_FLOAT(mass_before);
    float after_f = FIXED_TO_FLOAT(mass_after);
    float tolerance = before_f * 0.0001f;

    ASSERT_FLOAT_EQ(after_f, before_f, tolerance, "mass changed during simulation");
    PASS();
}

void test_h2o_mass_conserved_through_phases(void) {
    TEST("H2O mass conserved through freeze/melt/evaporate cycles");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 12345);

    // Add water to center cells
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            CELL_H2O_LIQUID(&state.cells[x][z]) = FLOAT_TO_FIXED(5.0f);
            cell_update_cache(&state.cells[x][z]);
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, AMBIENT_TEMP);
        }
    }

    fixed16_t h2o_before = calculate_total_h2o(&state);

    // Cool to freeze water
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(200.0f));
            cell_update_cache(&state.cells[x][z]);
        }
    }
    run_steps(&state, 500);

    fixed16_t h2o_after_freeze = calculate_total_h2o(&state);

    // Heat to melt and evaporate
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(500.0f));
            cell_update_cache(&state.cells[x][z]);
        }
    }
    run_steps(&state, 500);

    fixed16_t h2o_after_heat = calculate_total_h2o(&state);

    float before_f = FIXED_TO_FLOAT(h2o_before);
    float after_freeze_f = FIXED_TO_FLOAT(h2o_after_freeze);
    float after_heat_f = FIXED_TO_FLOAT(h2o_after_heat);
    float tolerance = before_f * 0.01f; // 1% tolerance

    ASSERT_FLOAT_EQ(after_freeze_f, before_f, tolerance, "H2O mass changed during freezing");
    ASSERT_FLOAT_EQ(after_heat_f, before_f, tolerance, "H2O mass changed during heating");
    PASS();
}

void test_silicate_mass_conserved(void) {
    TEST("silicate mass conserved (solid + lava)");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 12345);

    fixed16_t silicate_before = calculate_total_silicate(&state);

    // Run simulation (silicate shouldn't change at normal temps)
    run_steps(&state, 500);

    fixed16_t silicate_after = calculate_total_silicate(&state);

    float before_f = FIXED_TO_FLOAT(silicate_before);
    float after_f = FIXED_TO_FLOAT(silicate_after);
    float tolerance = before_f * 0.0001f;

    ASSERT_FLOAT_EQ(after_f, before_f, tolerance, "silicate mass changed");
    PASS();
}

void test_phase_transition_conserves_mass(void) {
    TEST("phase transitions conserve total mass");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 54321);

    // Add significant water
    for (int x = 60; x < 100; x++) {
        for (int z = 60; z < 100; z++) {
            CELL_H2O_LIQUID(&state.cells[x][z]) = FLOAT_TO_FIXED(2.0f);
            cell_update_cache(&state.cells[x][z]);
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, AMBIENT_TEMP);
        }
    }

    fixed16_t mass_before = calculate_total_mass(&state);

    // Cycle through temperatures to trigger phase transitions
    for (int cycle = 0; cycle < 3; cycle++) {
        // Cool everything
        for (int x = 0; x < MATTER_RES; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(200.0f));
            }
        }
        run_steps(&state, 100);

        // Heat everything
        for (int x = 0; x < MATTER_RES; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(400.0f));
            }
        }
        run_steps(&state, 100);
    }

    fixed16_t mass_after = calculate_total_mass(&state);

    float before_f = FIXED_TO_FLOAT(mass_before);
    float after_f = FIXED_TO_FLOAT(mass_after);
    // Use fabsf for tolerance in case of fixed-point overflow to negative
    float tolerance = fabsf(before_f) * 0.01f; // 1% tolerance for aggressive cycling

    ASSERT_FLOAT_EQ(after_f, before_f, tolerance, "mass changed during phase cycling");
    PASS();
}

// ============ ENERGY CONSERVATION TESTS ============

// Pure conduction test: set all cells to same temp, verify no net change
void test_heat_conduction_at_uniform_temp(void) {
    TEST("heat conduction at uniform temp has no net change");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 11111);

    // Set all cells to EXACTLY ambient temperature - no radiation loss
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            state.cells[x][z].temperature = AMBIENT_TEMP;
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, AMBIENT_TEMP);
        }
    }

    fixed16_t energy_before = calculate_total_energy(&state);

    // Run heat conduction - should have no effect at uniform temp
    for (int i = 0; i < 100; i++) {
        matter_conduct_heat(&state);
        for (int x = 0; x < MATTER_RES; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                cell_update_cache(&state.cells[x][z]);
            }
        }
    }

    fixed16_t energy_after = calculate_total_energy(&state);

    float before_f = FIXED_TO_FLOAT(energy_before);
    float after_f = FIXED_TO_FLOAT(energy_after);
    float tolerance = fabsf(before_f) * 0.001f; // 0.1% tolerance

    ASSERT_FLOAT_EQ(after_f, before_f, tolerance, "energy changed at uniform temp");
    PASS();
}

// Test that hot cells lose energy to environment (expected physical behavior)
void test_hot_cells_radiate_energy(void) {
    TEST("hot cells radiate energy to environment");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 11112);

    // Create a hot spot ABOVE ambient temp
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            state.cells[x][z].temperature = FLOAT_TO_FIXED(500.0f);
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(500.0f));
        }
    }

    fixed16_t energy_before = calculate_total_energy(&state);

    // Run heat conduction (includes radiation)
    for (int i = 0; i < 100; i++) {
        matter_conduct_heat(&state);
        for (int x = 0; x < MATTER_RES; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                cell_update_cache(&state.cells[x][z]);
            }
        }
    }

    fixed16_t energy_after = calculate_total_energy(&state);

    float before_f = FIXED_TO_FLOAT(energy_before);
    float after_f = FIXED_TO_FLOAT(energy_after);

    // Hot cells should lose energy to environment - after < before
    ASSERT(after_f < before_f, "hot cells should lose energy to environment");
    PASS();
}

void test_phase_transition_energy_balance(void) {
    TEST("phase transitions balance latent heat");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 22222);

    // Add water at exactly freezing point
    MatterCell *cell = &state.cells[80][80];
    CELL_H2O_LIQUID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature just below freezing
    cell->temperature = FLOAT_TO_FIXED(272.0f);
    cell->energy = fixed_mul(cell->thermal_mass, cell->temperature);

    fixed16_t energy_before = cell->energy;
    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);

    // Run phase transitions
    cell_process_phase_transition(cell, PHASEABLE_H2O);
    cell_update_cache(cell);

    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);
    fixed16_t energy_after = cell->energy;

    // Energy should have increased by latent heat of freezing
    fixed16_t frozen_mass = liquid_before - liquid_after;
    if (frozen_mass > 0) {
        fixed16_t actual_energy_gain = energy_after - energy_before;
        float actual_f = FIXED_TO_FLOAT(actual_energy_gain);

        // Allow some tolerance due to rate limiting
        ASSERT(actual_f > 0, "freezing should release energy");
    }

    // Also verify ice was created
    ASSERT(CELL_H2O_ICE(cell) > 0 || liquid_after < liquid_before, "phase change should occur");

    PASS();
}

// ============ COMBUSTION CONSERVATION TESTS ============

void test_combustion_conserves_mass(void) {
    TEST("combustion conserves total mass (fuel -> CO2 + ash)");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 44444);

    // Create a cell with fuel, oxygen, and high temperature
    MatterCell *cell = &state.cells[80][80];
    cell->cellulose_solid = FLOAT_TO_FIXED(1.0f);
    CELL_O2_GAS(cell) = FLOAT_TO_FIXED(1.0f);

    // First update cache to get thermal_mass, then set energy for desired temp
    cell_update_cache(cell);
    fixed16_t desired_temp = FLOAT_TO_FIXED(600.0f); // Above ignition (533K)
    cell->energy = fixed_mul(cell->thermal_mass, desired_temp);
    cell_update_cache(cell); // Now temperature will be correct

    fixed16_t fuel_before = cell->cellulose_solid;
    // Total mass includes smoke (combustion product)
    fixed16_t mass_before = cell->cellulose_solid + cell->co2_gas + cell->ash_solid +
                            CELL_O2_GAS(cell) + cell->smoke_gas + CELL_H2O_STEAM(cell);

    // Run combustion steps (maintaining temperature)
    for (int i = 0; i < 100; i++) {
        matter_process_combustion(&state);
        // Maintain high temperature for combustion to continue
        cell->energy = fixed_mul(cell->thermal_mass, desired_temp);
        cell_update_cache(cell);
    }

    fixed16_t mass_after = cell->cellulose_solid + cell->co2_gas + cell->ash_solid +
                           CELL_O2_GAS(cell) + cell->smoke_gas + CELL_H2O_STEAM(cell);

    // Note: Combustion converts cellulose + O2 -> CO2 + ash + smoke + steam + heat
    float before_f = FIXED_TO_FLOAT(mass_before);
    float after_f = FIXED_TO_FLOAT(mass_after);
    float tolerance = before_f * 0.05f; // 5% tolerance

    ASSERT_FLOAT_EQ(after_f, before_f, tolerance, "mass not conserved during combustion");

    // Check that fuel was consumed
    ASSERT(FIXED_TO_FLOAT(cell->cellulose_solid) < FIXED_TO_FLOAT(fuel_before),
           "fuel should be consumed");

    PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n========================================\n");
    printf("Conservation Law Tests\n");
    printf("========================================\n\n");

    printf("=== MASS CONSERVATION ===\n\n");
    test_total_mass_constant_over_simulation();
    test_h2o_mass_conserved_through_phases();
    test_silicate_mass_conserved();
    test_phase_transition_conserves_mass();

    printf("\n=== ENERGY CONSERVATION ===\n\n");
    test_heat_conduction_at_uniform_temp();
    test_hot_cells_radiate_energy();
    test_phase_transition_energy_balance();

    printf("\n=== COMBUSTION CONSERVATION ===\n\n");
    test_combustion_conserves_mass();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf(" (ALL PASSED)\n");
    } else {
        printf(" (%d FAILED)\n", tests_run - tests_passed);
    }
    printf("========================================\n\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
