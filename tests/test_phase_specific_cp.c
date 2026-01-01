/**
 * test_phase_specific_cp.c - Single-Phase Material Heat Capacity Tests
 *
 * New model: Each MaterialType has exactly ONE phase and ONE heat capacity.
 * Phase transitions are handled by converting between material types.
 *
 * Scientific basis:
 * - MAT_ICE (solid H2O): Cp = 38.0 J/(mol·K)
 * - MAT_WATER (liquid H2O): Cp = 75.3 J/(mol·K)
 * - MAT_STEAM (gas H2O): Cp = 33.6 J/(mol·K)
 *
 * Temperature calculation: T = E / (n * Cp)
 * Energy calculation: E = n * Cp * T
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "chunk.h"
#include "terrain.h"

// ============ TEST INFRASTRUCTURE ============

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_BEGIN(name) \
    printf("  %s... ", name); \
    fflush(stdout);

#define TEST_PASS() do { \
    tests_run++; \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define TEST_FAIL(msg, ...) do { \
    tests_run++; \
    tests_failed++; \
    printf("FAIL: " msg "\n", ##__VA_ARGS__); \
} while(0)

#define ASSERT(cond, msg, ...) do { \
    if (!(cond)) { \
        TEST_FAIL(msg, ##__VA_ARGS__); \
        return; \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b, tol, msg) do { \
    double _a = (a), _b = (b), _tol = (tol); \
    if (fabs(_a - _b) > _tol) { \
        TEST_FAIL("%s (expected %.4f, got %.4f, diff %.6f)", msg, _b, _a, fabs(_a - _b)); \
        return; \
    } \
} while(0)

// ============ REFERENCE VALUES ============

// Water phases as separate materials
#define REF_ICE_CP     38.0    // J/(mol·K) MAT_ICE
#define REF_WATER_CP   75.3    // J/(mol·K) MAT_WATER
#define REF_STEAM_CP   33.6    // J/(mol·K) MAT_STEAM

// Rock phases as separate materials
#define REF_ROCK_CP    44.4    // J/(mol·K) MAT_ROCK
#define REF_MAGMA_CP   82.6    // J/(mol·K) MAT_MAGMA

// ============ TEST: Each material has single Cp ============

static void test_material_has_single_cp(void) {
    TEST_BEGIN("each material has single molar_heat_capacity");

    // All materials (except MAT_NONE) should have positive Cp
    for (int i = 1; i < MAT_COUNT; i++) {
        const MaterialProperties *props = &MATERIAL_PROPS[i];
        ASSERT(props->molar_heat_capacity > 0,
               "%s should have positive Cp", props->name);
    }

    TEST_PASS();
}

// ============ TEST: Water phase Cp values match reference ============

static void test_water_phase_cp_values(void) {
    TEST_BEGIN("water phase Cp values match reference");

    // Each water phase is its own material with distinct Cp
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_ICE].molar_heat_capacity, REF_ICE_CP, 0.1,
                    "MAT_ICE Cp");
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_WATER].molar_heat_capacity, REF_WATER_CP, 0.1,
                    "MAT_WATER Cp");
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_STEAM].molar_heat_capacity, REF_STEAM_CP, 0.1,
                    "MAT_STEAM Cp");

    TEST_PASS();
}

// ============ TEST: Rock phase Cp values match reference ============

static void test_rock_phase_cp_values(void) {
    TEST_BEGIN("rock phase Cp values match reference");

    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_ROCK].molar_heat_capacity, REF_ROCK_CP, 0.1,
                    "MAT_ROCK Cp");
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_MAGMA].molar_heat_capacity, REF_MAGMA_CP, 0.1,
                    "MAT_MAGMA Cp");

    TEST_PASS();
}

// ============ TEST: Temperature uses material's Cp ============

static void test_temperature_uses_material_cp(void) {
    TEST_BEGIN("temperature uses material's molar_heat_capacity");

    // Test with MAT_ICE
    double moles = 1.0;
    double target_temp = 200.0;  // K
    double Cp = MATERIAL_PROPS[MAT_ICE].molar_heat_capacity;

    // E = n * Cp * T
    double energy = moles * Cp * target_temp;

    MaterialState state = {
        .moles = moles,
        .thermal_energy = energy,
        .cached_temp = 0.0
    };

    double calculated_temp = material_get_temperature(&state, MAT_ICE);

    ASSERT_FLOAT_EQ(calculated_temp, target_temp, 0.1,
                    "ice temperature from energy");

    TEST_PASS();
}

// ============ TEST: Different materials give different temps for same energy ============

static void test_different_cp_gives_different_temp(void) {
    TEST_BEGIN("different Cp gives different temperature for same energy");

    double moles = 1.0;
    double energy = 10000.0;  // J

    // Same energy in ice vs water gives different temperature
    MaterialState ice_state = { .moles = moles, .thermal_energy = energy, .cached_temp = 0.0 };
    MaterialState water_state = { .moles = moles, .thermal_energy = energy, .cached_temp = 0.0 };

    double ice_temp = material_get_temperature(&ice_state, MAT_ICE);
    double water_temp = material_get_temperature(&water_state, MAT_WATER);

    // Ice has lower Cp (38) than water (75.3), so same energy = higher temp
    ASSERT(ice_temp > water_temp,
           "ice should be hotter than water with same energy");

    // Verify quantitatively: T = E/(n*Cp)
    double expected_ice_temp = energy / (moles * REF_ICE_CP);
    double expected_water_temp = energy / (moles * REF_WATER_CP);

    ASSERT_FLOAT_EQ(ice_temp, expected_ice_temp, 0.1, "ice temp calculation");
    ASSERT_FLOAT_EQ(water_temp, expected_water_temp, 0.1, "water temp calculation");

    TEST_PASS();
}

// ============ TEST: Phase is intrinsic to material type ============

static void test_phase_is_intrinsic(void) {
    TEST_BEGIN("phase is intrinsic to material type");

    // Each material has exactly one phase
    ASSERT(MATERIAL_PROPS[MAT_ICE].phase == PHASE_SOLID, "MAT_ICE should be solid");
    ASSERT(MATERIAL_PROPS[MAT_WATER].phase == PHASE_LIQUID, "MAT_WATER should be liquid");
    ASSERT(MATERIAL_PROPS[MAT_STEAM].phase == PHASE_GAS, "MAT_STEAM should be gas");

    ASSERT(MATERIAL_PROPS[MAT_ROCK].phase == PHASE_SOLID, "MAT_ROCK should be solid");
    ASSERT(MATERIAL_PROPS[MAT_MAGMA].phase == PHASE_LIQUID, "MAT_MAGMA should be liquid");

    TEST_PASS();
}

// ============ TEST: Phase transition links exist ============

static void test_phase_transition_links(void) {
    TEST_BEGIN("phase transition links connect related materials");

    // Water cycle: ice <-> water <-> steam
    ASSERT(MATERIAL_PROPS[MAT_ICE].liquid_form == MAT_WATER, "ice melts to water");
    ASSERT(MATERIAL_PROPS[MAT_WATER].solid_form == MAT_ICE, "water freezes to ice");
    ASSERT(MATERIAL_PROPS[MAT_WATER].gas_form == MAT_STEAM, "water boils to steam");
    ASSERT(MATERIAL_PROPS[MAT_STEAM].liquid_form == MAT_WATER, "steam condenses to water");

    // Rock cycle: rock <-> magma
    ASSERT(MATERIAL_PROPS[MAT_ROCK].liquid_form == MAT_MAGMA, "rock melts to magma");
    ASSERT(MATERIAL_PROPS[MAT_MAGMA].solid_form == MAT_ROCK, "magma solidifies to rock");

    TEST_PASS();
}

// ============ TEST: Transition temperatures are set ============

static void test_transition_temperatures(void) {
    TEST_BEGIN("transition temperatures are defined");

    // Water transitions
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_WATER].transition_temp_down, 273.15, 0.1,
                    "water freezing point");
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_WATER].transition_temp_up, 373.15, 0.1,
                    "water boiling point");

    // Ice melting point
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_ICE].transition_temp_up, 273.15, 0.1,
                    "ice melting point");

    // Steam condensation point
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_STEAM].transition_temp_down, 373.15, 0.1,
                    "steam condensation point");

    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("    SINGLE-PHASE Cp TESTS\n");
    printf("========================================\n\n");

    test_material_has_single_cp();
    test_water_phase_cp_values();
    test_rock_phase_cp_values();
    test_temperature_uses_material_cp();
    test_different_cp_gives_different_temp();
    test_phase_is_intrinsic();
    test_phase_transition_links();
    test_transition_temperatures();

    printf("\n========================================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)\n", tests_failed);
    } else {
        printf(" (ALL PASSED)\n");
    }
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
