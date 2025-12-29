/**
 * test_phase_transitions.c - Phase Transition Tests
 *
 * Tests that verify phase transitions work correctly:
 * - Transition points (freezing, melting, boiling, condensing)
 * - Latent heat exchange
 * - Temperature plateaus during phase change
 * - Multi-phase transitions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Include matter system directly for testing
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

static void create_flat_terrain(int terrain[MATTER_RES][MATTER_RES], int height) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            terrain[x][z] = height;
        }
    }
}

// Set up a single test cell with specific contents
static MatterCell* setup_test_cell(MatterState *state, int x, int z) {
    MatterCell *cell = &state->cells[x][z];
    // Clear all matter
    memset(cell->phase_mass, 0, sizeof(cell->phase_mass));
    cell->cellulose_solid = 0;
    cell->co2_gas = 0;
    cell->smoke_gas = 0;
    cell->ash_solid = 0;
    cell_update_cache(cell);
    return cell;
}

// Set cell temperature via energy
static void set_cell_temperature(MatterCell *cell, float temp_k) {
    cell_update_cache(cell);
    cell->energy = fixed_mul(cell->thermal_mass, FLOAT_TO_FIXED(temp_k));
    cell_update_cache(cell);
}

// ============ TRANSITION POINT TESTS ============

void test_water_freezes_below_273K(void) {
    TEST("water freezes below 273K");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 11111);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_LIQUID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature below freezing (e.g., 260K)
    set_cell_temperature(cell, 260.0f);

    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);
    fixed16_t ice_before = CELL_H2O_ICE(cell);

    // Run phase transitions
    for (int i = 0; i < 100; i++) {
        cell_process_phase_transition(cell, PHASEABLE_H2O);
        cell_update_cache(cell);
        // Maintain cold temperature
        if (FIXED_TO_FLOAT(cell->temperature) > 270.0f) {
            set_cell_temperature(cell, 260.0f);
        }
    }

    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);
    fixed16_t ice_after = CELL_H2O_ICE(cell);

    // Some water should have frozen
    ASSERT(liquid_after < liquid_before, "liquid didn't decrease");
    ASSERT(ice_after > ice_before, "ice didn't increase");
    PASS();
}

void test_water_melts_above_273K(void) {
    TEST("ice melts above 273K");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 22222);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_ICE(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature above freezing (e.g., 283K = 10C)
    set_cell_temperature(cell, 283.0f);

    fixed16_t ice_before = CELL_H2O_ICE(cell);
    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);

    // Run phase transitions
    for (int i = 0; i < 100; i++) {
        cell_process_phase_transition(cell, PHASEABLE_H2O);
        cell_update_cache(cell);
        // Maintain warm temperature
        if (FIXED_TO_FLOAT(cell->temperature) < 280.0f) {
            set_cell_temperature(cell, 283.0f);
        }
    }

    fixed16_t ice_after = CELL_H2O_ICE(cell);
    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);

    // Some ice should have melted
    ASSERT(ice_after < ice_before, "ice didn't decrease");
    ASSERT(liquid_after > liquid_before, "liquid didn't increase");
    PASS();
}

void test_water_boils_above_373K(void) {
    TEST("water boils above 373K");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 33333);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_LIQUID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature above boiling (e.g., 400K)
    set_cell_temperature(cell, 400.0f);

    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);
    fixed16_t steam_before = CELL_H2O_STEAM(cell);

    // Run phase transitions
    for (int i = 0; i < 100; i++) {
        cell_process_phase_transition(cell, PHASEABLE_H2O);
        cell_update_cache(cell);
        // Maintain hot temperature
        if (FIXED_TO_FLOAT(cell->temperature) < 390.0f) {
            set_cell_temperature(cell, 400.0f);
        }
    }

    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);
    fixed16_t steam_after = CELL_H2O_STEAM(cell);

    // Some water should have evaporated
    ASSERT(liquid_after < liquid_before, "liquid didn't decrease");
    ASSERT(steam_after > steam_before, "steam didn't increase");
    PASS();
}

void test_steam_condenses_below_373K(void) {
    TEST("steam condenses below 373K");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 44444);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_STEAM(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature below boiling (e.g., 350K)
    set_cell_temperature(cell, 350.0f);

    fixed16_t steam_before = CELL_H2O_STEAM(cell);
    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);

    // Run phase transitions
    for (int i = 0; i < 100; i++) {
        cell_process_phase_transition(cell, PHASEABLE_H2O);
        cell_update_cache(cell);
        // Maintain temperature
        if (FIXED_TO_FLOAT(cell->temperature) > 355.0f) {
            set_cell_temperature(cell, 350.0f);
        }
    }

    fixed16_t steam_after = CELL_H2O_STEAM(cell);
    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);

    // Some steam should have condensed
    ASSERT(steam_after < steam_before, "steam didn't decrease");
    ASSERT(liquid_after > liquid_before, "liquid didn't increase");
    PASS();
}

// ============ LATENT HEAT TESTS ============

void test_freezing_releases_heat(void) {
    TEST("freezing releases latent heat");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 55555);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_LIQUID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature just below freezing
    set_cell_temperature(cell, 272.0f);

    fixed16_t energy_before = cell->energy;
    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);

    // Process one phase transition
    cell_process_phase_transition(cell, PHASEABLE_H2O);
    cell_update_cache(cell);

    fixed16_t energy_after = cell->energy;
    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);

    fixed16_t mass_frozen = liquid_before - liquid_after;

    // If water froze, energy should have increased (heat released)
    if (mass_frozen > 0) {
        ASSERT(energy_after > energy_before, "freezing didn't release heat");
    }
    PASS();
}

void test_melting_absorbs_heat(void) {
    TEST("melting absorbs latent heat");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 66666);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_ICE(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature just above freezing
    set_cell_temperature(cell, 275.0f);

    fixed16_t energy_before = cell->energy;
    fixed16_t ice_before = CELL_H2O_ICE(cell);

    // Process one phase transition
    cell_process_phase_transition(cell, PHASEABLE_H2O);
    cell_update_cache(cell);

    fixed16_t energy_after = cell->energy;
    fixed16_t ice_after = CELL_H2O_ICE(cell);

    fixed16_t mass_melted = ice_before - ice_after;

    // If ice melted, energy should have decreased (heat absorbed)
    if (mass_melted > 0) {
        ASSERT(energy_after < energy_before, "melting didn't absorb heat");
    }
    PASS();
}

void test_boiling_absorbs_heat(void) {
    TEST("boiling absorbs latent heat");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 77777);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_LIQUID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature above boiling
    set_cell_temperature(cell, 400.0f);

    fixed16_t energy_before = cell->energy;
    fixed16_t liquid_before = CELL_H2O_LIQUID(cell);

    // Process one phase transition
    cell_process_phase_transition(cell, PHASEABLE_H2O);
    cell_update_cache(cell);

    fixed16_t energy_after = cell->energy;
    fixed16_t liquid_after = CELL_H2O_LIQUID(cell);

    fixed16_t mass_evaporated = liquid_before - liquid_after;

    // If water evaporated, energy should have decreased (heat absorbed)
    if (mass_evaporated > 0) {
        ASSERT(energy_after < energy_before, "boiling didn't absorb heat");
    }
    PASS();
}

void test_condensation_releases_heat(void) {
    TEST("condensation releases latent heat");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 88888);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_STEAM(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature below boiling
    set_cell_temperature(cell, 360.0f);

    fixed16_t energy_before = cell->energy;
    fixed16_t steam_before = CELL_H2O_STEAM(cell);

    // Process one phase transition
    cell_process_phase_transition(cell, PHASEABLE_H2O);
    cell_update_cache(cell);

    fixed16_t energy_after = cell->energy;
    fixed16_t steam_after = CELL_H2O_STEAM(cell);

    fixed16_t mass_condensed = steam_before - steam_after;

    // If steam condensed, energy should have increased (heat released)
    if (mass_condensed > 0) {
        ASSERT(energy_after > energy_before, "condensation didn't release heat");
    }
    PASS();
}

// ============ MASS CONSERVATION DURING PHASE CHANGE ============

void test_phase_change_conserves_h2o_mass(void) {
    TEST("phase change conserves H2O mass");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 99999);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_H2O_LIQUID(cell) = FLOAT_TO_FIXED(2.0f);
    cell_update_cache(cell);

    fixed16_t total_h2o_before = CELL_H2O_ICE(cell) + CELL_H2O_LIQUID(cell) + CELL_H2O_STEAM(cell);

    // Freeze
    set_cell_temperature(cell, 260.0f);
    for (int i = 0; i < 200; i++) {
        cell_process_phase_transition(cell, PHASEABLE_H2O);
        cell_update_cache(cell);
        set_cell_temperature(cell, 260.0f);
    }

    fixed16_t total_h2o_after_freeze = CELL_H2O_ICE(cell) + CELL_H2O_LIQUID(cell) + CELL_H2O_STEAM(cell);

    // Boil
    set_cell_temperature(cell, 500.0f);
    for (int i = 0; i < 200; i++) {
        cell_process_phase_transition(cell, PHASEABLE_H2O);
        cell_update_cache(cell);
        set_cell_temperature(cell, 500.0f);
    }

    fixed16_t total_h2o_after_boil = CELL_H2O_ICE(cell) + CELL_H2O_LIQUID(cell) + CELL_H2O_STEAM(cell);

    float before_f = FIXED_TO_FLOAT(total_h2o_before);
    float after_freeze_f = FIXED_TO_FLOAT(total_h2o_after_freeze);
    float after_boil_f = FIXED_TO_FLOAT(total_h2o_after_boil);

    ASSERT_FLOAT_EQ(after_freeze_f, before_f, 0.01f, "H2O mass changed during freezing");
    ASSERT_FLOAT_EQ(after_boil_f, before_f, 0.01f, "H2O mass changed during boiling");
    PASS();
}

// ============ SILICATE PHASE TRANSITIONS ============

void test_silicate_melts_at_high_temp(void) {
    TEST("silicate melts at high temperature");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 10101);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_SILICATE_SOLID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature above silicate melting point (2259K)
    set_cell_temperature(cell, 2500.0f);

    fixed16_t solid_before = CELL_SILICATE_SOLID(cell);
    fixed16_t lava_before = CELL_SILICATE_LIQUID(cell);

    // Run phase transitions
    for (int i = 0; i < 200; i++) {
        cell_process_phase_transition(cell, PHASEABLE_SILICATE);
        cell_update_cache(cell);
        set_cell_temperature(cell, 2500.0f);
    }

    fixed16_t solid_after = CELL_SILICATE_SOLID(cell);
    fixed16_t lava_after = CELL_SILICATE_LIQUID(cell);

    // Some silicate should have melted
    ASSERT(solid_after < solid_before, "silicate didn't decrease");
    ASSERT(lava_after > lava_before, "lava didn't increase");
    PASS();
}

void test_lava_solidifies_at_low_temp(void) {
    TEST("lava solidifies at low temperature");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 20202);

    MatterCell *cell = setup_test_cell(&state, 80, 80);
    CELL_SILICATE_LIQUID(cell) = FLOAT_TO_FIXED(1.0f);
    cell_update_cache(cell);

    // Set temperature below silicate melting point (2259K)
    set_cell_temperature(cell, 2000.0f);

    fixed16_t lava_before = CELL_SILICATE_LIQUID(cell);
    fixed16_t solid_before = CELL_SILICATE_SOLID(cell);

    // Run phase transitions
    for (int i = 0; i < 200; i++) {
        cell_process_phase_transition(cell, PHASEABLE_SILICATE);
        cell_update_cache(cell);
        set_cell_temperature(cell, 2000.0f);
    }

    fixed16_t lava_after = CELL_SILICATE_LIQUID(cell);
    fixed16_t solid_after = CELL_SILICATE_SOLID(cell);

    // Some lava should have solidified
    ASSERT(lava_after < lava_before, "lava didn't decrease");
    ASSERT(solid_after > solid_before, "solid didn't increase");
    PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n========================================\n");
    printf("Phase Transition Tests\n");
    printf("========================================\n\n");

    printf("=== WATER TRANSITION POINTS ===\n\n");
    test_water_freezes_below_273K();
    test_water_melts_above_273K();
    test_water_boils_above_373K();
    test_steam_condenses_below_373K();

    printf("\n=== LATENT HEAT ===\n\n");
    test_freezing_releases_heat();
    test_melting_absorbs_heat();
    test_boiling_absorbs_heat();
    test_condensation_releases_heat();

    printf("\n=== MASS CONSERVATION ===\n\n");
    test_phase_change_conserves_h2o_mass();

    printf("\n=== SILICATE TRANSITIONS ===\n\n");
    test_silicate_melts_at_high_temp();
    test_lava_solidifies_at_low_temp();

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
