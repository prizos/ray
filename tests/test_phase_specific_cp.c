/**
 * test_phase_specific_cp.c - Phase-Specific Heat Capacity Tests
 *
 * Theory: Heat capacity varies significantly between phases for most substances.
 * Using a single Cp for all phases introduces thermodynamic errors.
 *
 * Scientific basis:
 * - Ice (solid H2O): Cp_s = 38.0 J/(mol·K)
 * - Water (liquid H2O): Cp_l = 75.3 J/(mol·K)
 * - Steam (gas H2O): Cp_g = 33.6 J/(mol·K)
 *
 * The energy required to heat a substance depends on which phase it's in.
 * Energy thresholds and temperature calculations must use the correct Cp.
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

// ============ REFERENCE VALUES (from CLAUDE.md) ============

// Water (H2O) - significant variation between phases
#define REF_H2O_CP_S  38.0    // J/(mol·K) solid (ice)
#define REF_H2O_CP_L  75.3    // J/(mol·K) liquid (water)
#define REF_H2O_CP_G  33.6    // J/(mol·K) gas (steam)
#define REF_H2O_TM    273.15  // K melting point
#define REF_H2O_TB    373.15  // K boiling point
#define REF_H2O_HF    6010.0  // J/mol enthalpy of fusion
#define REF_H2O_HV    40660.0 // J/mol enthalpy of vaporization

// Rock (SiO2)
#define REF_SIO2_CP_S 44.4
#define REF_SIO2_CP_L 82.6
#define REF_SIO2_CP_G 47.4
#define REF_SIO2_TM   1986.0
#define REF_SIO2_TB   2503.0
#define REF_SIO2_HF   9600.0
#define REF_SIO2_HV   600000.0

// ============ HELPER: Calculate energy using phase-specific Cp ============
// This is what the implementation SHOULD do

static double calculate_correct_energy(double moles, double temp_k,
                                       double Cp_s, double Cp_l, double Cp_g,
                                       double Tm, double Tb, double Hf, double Hv) {
    if (temp_k <= Tm) {
        // Solid phase: E = n * Cp_s * T
        return moles * Cp_s * temp_k;
    } else if (temp_k <= Tb) {
        // Liquid phase: E = (solid to Tm) + Hf + (liquid from Tm to T)
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (temp_k - Tm);
    } else {
        // Gas phase: E = (solid to Tm) + Hf + (liquid to Tb) + Hv + (gas from Tb to T)
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (Tb - Tm)
             + moles * Hv + moles * Cp_g * (temp_k - Tb);
    }
}

// ============ TEST: MaterialProperties has phase-specific Cp fields ============

static void test_material_properties_has_phase_specific_cp(void) {
    TEST_BEGIN("MaterialProperties has phase-specific Cp fields");

    const MaterialProperties *water = &MATERIAL_PROPS[MAT_WATER];

    // These fields must exist and be accessible
    // If compilation fails here, the struct needs updating
    double cp_s = water->molar_heat_capacity_solid;
    double cp_l = water->molar_heat_capacity_liquid;
    double cp_g = water->molar_heat_capacity_gas;

    // They should be different for water (ice vs water vs steam)
    ASSERT(cp_s != cp_l, "Cp_s should differ from Cp_l for water");
    ASSERT(cp_l != cp_g, "Cp_l should differ from Cp_g for water");

    TEST_PASS();
}

// ============ TEST: Water Cp values match reference ============

static void test_water_cp_values(void) {
    TEST_BEGIN("water Cp values match reference");

    const MaterialProperties *water = &MATERIAL_PROPS[MAT_WATER];

    ASSERT_FLOAT_EQ(water->molar_heat_capacity_solid, REF_H2O_CP_S, 0.1,
                    "water Cp_s");
    ASSERT_FLOAT_EQ(water->molar_heat_capacity_liquid, REF_H2O_CP_L, 0.1,
                    "water Cp_l");
    ASSERT_FLOAT_EQ(water->molar_heat_capacity_gas, REF_H2O_CP_G, 0.1,
                    "water Cp_g");

    TEST_PASS();
}

// ============ TEST: Rock Cp values match reference ============

static void test_rock_cp_values(void) {
    TEST_BEGIN("rock Cp values match reference");

    const MaterialProperties *rock = &MATERIAL_PROPS[MAT_ROCK];

    ASSERT_FLOAT_EQ(rock->molar_heat_capacity_solid, REF_SIO2_CP_S, 0.1,
                    "rock Cp_s");
    ASSERT_FLOAT_EQ(rock->molar_heat_capacity_liquid, REF_SIO2_CP_L, 0.1,
                    "rock Cp_l");
    ASSERT_FLOAT_EQ(rock->molar_heat_capacity_gas, REF_SIO2_CP_G, 0.1,
                    "rock Cp_g");

    TEST_PASS();
}

// ============ TEST: Temperature in solid phase uses Cp_s ============

static void test_temperature_solid_phase_uses_cp_s(void) {
    TEST_BEGIN("temperature in solid phase uses Cp_s");

    // Set up water at 200K (solid ice, below melting point 273K)
    double moles = 1.0;
    double target_temp = 200.0;  // K

    // Calculate energy using correct formula: E = n * Cp_s * T
    double energy = moles * REF_H2O_CP_S * target_temp;

    MaterialState state = {
        .moles = moles,
        .thermal_energy = energy
    };

    double calculated_temp = material_get_temperature(&state, MAT_WATER);

    ASSERT_FLOAT_EQ(calculated_temp, target_temp, 0.1,
                    "solid phase temperature");

    TEST_PASS();
}

// ============ TEST: Temperature in liquid phase uses Cp_l ============

static void test_temperature_liquid_phase_uses_cp_l(void) {
    TEST_BEGIN("temperature in liquid phase uses Cp_l");

    // Set up water at 300K (liquid, between 273K and 373K)
    double moles = 1.0;
    double target_temp = 300.0;  // K

    // Calculate energy for liquid at 300K:
    // E = (solid to Tm) + Hf + (liquid from Tm to T)
    // E = n*Cp_s*Tm + n*Hf + n*Cp_l*(T - Tm)
    double energy = calculate_correct_energy(moles, target_temp,
                                             REF_H2O_CP_S, REF_H2O_CP_L, REF_H2O_CP_G,
                                             REF_H2O_TM, REF_H2O_TB,
                                             REF_H2O_HF, REF_H2O_HV);

    MaterialState state = {
        .moles = moles,
        .thermal_energy = energy
    };

    double calculated_temp = material_get_temperature(&state, MAT_WATER);

    ASSERT_FLOAT_EQ(calculated_temp, target_temp, 0.1,
                    "liquid phase temperature");

    TEST_PASS();
}

// ============ TEST: Temperature in gas phase uses Cp_g ============

static void test_temperature_gas_phase_uses_cp_g(void) {
    TEST_BEGIN("temperature in gas phase uses Cp_g");

    // Set up water at 400K (gas/steam, above boiling point 373K)
    double moles = 1.0;
    double target_temp = 400.0;  // K

    // Calculate energy for gas at 400K
    double energy = calculate_correct_energy(moles, target_temp,
                                             REF_H2O_CP_S, REF_H2O_CP_L, REF_H2O_CP_G,
                                             REF_H2O_TM, REF_H2O_TB,
                                             REF_H2O_HF, REF_H2O_HV);

    MaterialState state = {
        .moles = moles,
        .thermal_energy = energy
    };

    double calculated_temp = material_get_temperature(&state, MAT_WATER);

    ASSERT_FLOAT_EQ(calculated_temp, target_temp, 0.1,
                    "gas phase temperature");

    TEST_PASS();
}

// ============ TEST: Melting plateau temperature is correct ============

static void test_melting_plateau_temperature(void) {
    TEST_BEGIN("melting plateau temperature equals Tm");

    double moles = 1.0;

    // Energy at start of melting: E = n * Cp_s * Tm
    double E_melt_start = moles * REF_H2O_CP_S * REF_H2O_TM;

    // Energy midway through melting (50% melted)
    double E_mid_melt = E_melt_start + 0.5 * moles * REF_H2O_HF;

    MaterialState state = {
        .moles = moles,
        .thermal_energy = E_mid_melt
    };

    double temp = material_get_temperature(&state, MAT_WATER);

    // During melting, temperature should be exactly Tm
    ASSERT_FLOAT_EQ(temp, REF_H2O_TM, 0.01,
                    "melting plateau temperature");

    TEST_PASS();
}

// ============ TEST: Boiling plateau temperature is correct ============

static void test_boiling_plateau_temperature(void) {
    TEST_BEGIN("boiling plateau temperature equals Tb");

    double moles = 1.0;

    // Energy at start of boiling
    double E_melt_start = moles * REF_H2O_CP_S * REF_H2O_TM;
    double E_melt_end = E_melt_start + moles * REF_H2O_HF;
    double E_boil_start = E_melt_end + moles * REF_H2O_CP_L * (REF_H2O_TB - REF_H2O_TM);

    // Energy midway through boiling (50% vaporized)
    double E_mid_boil = E_boil_start + 0.5 * moles * REF_H2O_HV;

    MaterialState state = {
        .moles = moles,
        .thermal_energy = E_mid_boil
    };

    double temp = material_get_temperature(&state, MAT_WATER);

    // During boiling, temperature should be exactly Tb
    ASSERT_FLOAT_EQ(temp, REF_H2O_TB, 0.01,
                    "boiling plateau temperature");

    TEST_PASS();
}

// ============ TEST: Energy thresholds use correct Cp values ============

static void test_energy_thresholds_use_correct_cp(void) {
    TEST_BEGIN("energy thresholds use correct phase Cp");

    double moles = 1.0;

    // Calculate expected thresholds using phase-specific Cp
    double E_melt_start = moles * REF_H2O_CP_S * REF_H2O_TM;
    double E_melt_end = E_melt_start + moles * REF_H2O_HF;
    double E_boil_start = E_melt_end + moles * REF_H2O_CP_L * (REF_H2O_TB - REF_H2O_TM);
    double E_boil_end = E_boil_start + moles * REF_H2O_HV;

    // Test that material at each threshold gives correct temperature

    // Just below melting: should be just below Tm
    MaterialState state1 = { .moles = moles, .thermal_energy = E_melt_start - 100 };
    double temp1 = material_get_temperature(&state1, MAT_WATER);
    ASSERT(temp1 < REF_H2O_TM, "temp below E_melt_start should be < Tm");

    // At melting plateau: should be Tm
    MaterialState state2 = { .moles = moles, .thermal_energy = (E_melt_start + E_melt_end) / 2 };
    double temp2 = material_get_temperature(&state2, MAT_WATER);
    ASSERT_FLOAT_EQ(temp2, REF_H2O_TM, 0.01, "temp at melting plateau");

    // Just after melting: should be just above Tm
    MaterialState state3 = { .moles = moles, .thermal_energy = E_melt_end + 100 };
    double temp3 = material_get_temperature(&state3, MAT_WATER);
    ASSERT(temp3 > REF_H2O_TM && temp3 < REF_H2O_TB, "temp after E_melt_end should be in liquid range");

    // At boiling plateau: should be Tb
    MaterialState state4 = { .moles = moles, .thermal_energy = (E_boil_start + E_boil_end) / 2 };
    double temp4 = material_get_temperature(&state4, MAT_WATER);
    ASSERT_FLOAT_EQ(temp4, REF_H2O_TB, 0.01, "temp at boiling plateau");

    // After boiling: should be > Tb
    MaterialState state5 = { .moles = moles, .thermal_energy = E_boil_end + 1000 };
    double temp5 = material_get_temperature(&state5, MAT_WATER);
    ASSERT(temp5 > REF_H2O_TB, "temp after E_boil_end should be > Tb");

    TEST_PASS();
}

// ============ TEST: Phase determination uses correct energy thresholds ============

static void test_phase_from_energy_uses_correct_thresholds(void) {
    TEST_BEGIN("phase determination uses correct energy thresholds");

    double moles = 1.0;

    // Calculate thresholds
    double E_melt_start = moles * REF_H2O_CP_S * REF_H2O_TM;
    double E_melt_end = E_melt_start + moles * REF_H2O_HF;
    double E_boil_start = E_melt_end + moles * REF_H2O_CP_L * (REF_H2O_TB - REF_H2O_TM);
    double E_boil_end = E_boil_start + moles * REF_H2O_HV;

    // Solid: E < E_melt_end
    MaterialState solid = { .moles = moles, .thermal_energy = E_melt_start / 2 };
    ASSERT(material_get_phase_from_energy(&solid, MAT_WATER) == PHASE_SOLID,
           "should be solid below E_melt_end");

    // Still solid during melting
    MaterialState melting = { .moles = moles, .thermal_energy = (E_melt_start + E_melt_end) / 2 };
    ASSERT(material_get_phase_from_energy(&melting, MAT_WATER) == PHASE_SOLID,
           "should be solid during melting");

    // Liquid: E_melt_end <= E < E_boil_end
    MaterialState liquid = { .moles = moles, .thermal_energy = (E_melt_end + E_boil_start) / 2 };
    ASSERT(material_get_phase_from_energy(&liquid, MAT_WATER) == PHASE_LIQUID,
           "should be liquid between E_melt_end and E_boil_end");

    // Still liquid during boiling
    MaterialState boiling = { .moles = moles, .thermal_energy = (E_boil_start + E_boil_end) / 2 };
    ASSERT(material_get_phase_from_energy(&boiling, MAT_WATER) == PHASE_LIQUID,
           "should be liquid during boiling");

    // Gas: E >= E_boil_end
    MaterialState gas = { .moles = moles, .thermal_energy = E_boil_end + 1000 };
    ASSERT(material_get_phase_from_energy(&gas, MAT_WATER) == PHASE_GAS,
           "should be gas above E_boil_end");

    TEST_PASS();
}

// ============ TEST: Roundtrip energy -> temperature -> energy ============

static void test_energy_temperature_roundtrip(void) {
    TEST_BEGIN("energy -> temperature -> energy roundtrip");

    // Test various temperatures across all phases
    double test_temps[] = {100.0, 200.0, 273.15, 300.0, 350.0, 373.15, 400.0, 500.0};
    int num_tests = sizeof(test_temps) / sizeof(test_temps[0]);

    for (int i = 0; i < num_tests; i++) {
        double target_temp = test_temps[i];
        double moles = 1.0;

        // Calculate energy for this temperature
        double energy = calculate_correct_energy(moles, target_temp,
                                                 REF_H2O_CP_S, REF_H2O_CP_L, REF_H2O_CP_G,
                                                 REF_H2O_TM, REF_H2O_TB,
                                                 REF_H2O_HF, REF_H2O_HV);

        // Skip plateau temperatures (they're ambiguous)
        if (fabs(target_temp - REF_H2O_TM) < 0.1 || fabs(target_temp - REF_H2O_TB) < 0.1) {
            continue;
        }

        MaterialState state = { .moles = moles, .thermal_energy = energy };
        double calculated_temp = material_get_temperature(&state, MAT_WATER);

        if (fabs(calculated_temp - target_temp) > 0.5) {
            TEST_FAIL("roundtrip failed for T=%.1fK (got %.1fK)", target_temp, calculated_temp);
            return;
        }
    }

    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Phase-Specific Heat Capacity Tests\n");
    printf("========================================\n");

    printf("\n=== STRUCT FIELDS ===\n\n");
    test_material_properties_has_phase_specific_cp();

    printf("\n=== REFERENCE VALUES ===\n\n");
    test_water_cp_values();
    test_rock_cp_values();

    printf("\n=== TEMPERATURE CALCULATIONS ===\n\n");
    test_temperature_solid_phase_uses_cp_s();
    test_temperature_liquid_phase_uses_cp_l();
    test_temperature_gas_phase_uses_cp_g();
    test_melting_plateau_temperature();
    test_boiling_plateau_temperature();

    printf("\n=== ENERGY THRESHOLDS ===\n\n");
    test_energy_thresholds_use_correct_cp();
    test_phase_from_energy_uses_correct_thresholds();

    printf("\n=== ROUNDTRIP ===\n\n");
    test_energy_temperature_roundtrip();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(" (%d FAILED)\n", tests_failed);
    } else {
        printf(" (ALL PASSED)\n");
    }
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
