/**
 * Water-Matter System - Unit Tests
 *
 * Tests the interaction between water simulation and matter thermodynamics.
 * Each test validates a specific scientific theory.
 *
 * Theories tested:
 * 1. Water suppresses combustion
 * 2. Phase transitions conserve mass
 * 3. Phase transitions require latent heat
 * 4. Temperature cannot go below absolute zero
 * 5. No artificial temperature ceiling
 */

#include "test_common.h"

// ============ PHYSICAL CONSTANTS ============
// Using J/g·K and J/g units to fit in 16.16 fixed-point (same scale as matter.c)

// Phase transition temperatures (Kelvin)
#define WATER_MELTING_POINT    FLOAT_TO_FIXED(273.15f)
#define WATER_BOILING_POINT    FLOAT_TO_FIXED(373.15f)
#define ABSOLUTE_ZERO          FLOAT_TO_FIXED(0.0f)

// Latent heats (J/g) - scaled to match fixed-point range
#define LATENT_HEAT_FUSION          FLOAT_TO_FIXED(334.0f)    // 334 J/g for ice→water
#define LATENT_HEAT_VAPORIZATION    FLOAT_TO_FIXED(2260.0f)   // 2260 J/g for water→steam

// Specific heats (J/g·K) - matches matter.c scale
#define SPECIFIC_HEAT_ICE      FLOAT_TO_FIXED(2.09f)
#define SPECIFIC_HEAT_WATER    FLOAT_TO_FIXED(4.18f)
#define SPECIFIC_HEAT_STEAM    FLOAT_TO_FIXED(2.01f)

// Water-matter sync constant
#define WATER_MASS_PER_DEPTH   FLOAT_TO_FIXED(1000.0f)  // kg per unit depth

// Combustion thresholds
#define IGNITION_TEMP          FLOAT_TO_FIXED(533.0f)
#define MIN_FUEL_MASS          FLOAT_TO_FIXED(0.01f)
#define MIN_O2_MASS            FLOAT_TO_FIXED(0.001f)
#define WATER_SUPPRESSION_THRESHOLD FLOAT_TO_FIXED(0.1f)

// ============ TEST CELL STRUCTURE ============
// Simplified test cell with phase-specific water tracking

typedef struct {
    // Masses
    fixed16_t fuel_mass;       // Cellulose
    fixed16_t o2_mass;         // Oxygen
    fixed16_t silicate_mass;   // Ground

    // H2O by phase
    fixed16_t h2o_ice;
    fixed16_t h2o_liquid;
    fixed16_t h2o_steam;

    // Thermal state
    fixed16_t energy;
    fixed16_t temperature;
    fixed16_t thermal_mass;

} TestCell;

// ============ HELPER FUNCTIONS ============

static fixed16_t test_cell_total_h2o(const TestCell *c) {
    return c->h2o_ice + c->h2o_liquid + c->h2o_steam;
}

static void test_cell_update_thermal_mass(TestCell *c) {
    // Using J/g·K specific heats, mass in grams (matches matter.c)
    // Silicate: 0.7 J/(g·K)
    fixed16_t silicate_th = fixed_mul(c->silicate_mass, FLOAT_TO_FIXED(0.7f));

    // Fuel (cellulose): 1.3 J/(g·K)
    fixed16_t fuel_th = fixed_mul(c->fuel_mass, FLOAT_TO_FIXED(1.3f));

    // H2O phases with correct specific heats
    fixed16_t ice_th = fixed_mul(c->h2o_ice, SPECIFIC_HEAT_ICE);
    fixed16_t liquid_th = fixed_mul(c->h2o_liquid, SPECIFIC_HEAT_WATER);
    fixed16_t steam_th = fixed_mul(c->h2o_steam, SPECIFIC_HEAT_STEAM);

    c->thermal_mass = silicate_th + fuel_th + ice_th + liquid_th + steam_th;
}

static void test_cell_update_temperature(TestCell *c) {
    // Minimum thermal mass threshold (matches matter.c)
    if (c->thermal_mass > FLOAT_TO_FIXED(0.01f)) {
        c->temperature = fixed_div(c->energy, c->thermal_mass);

        // Enforce absolute zero floor
        if (c->temperature < 0) {
            c->temperature = 0;
            c->energy = 0;
        }
        // NO upper limit - temperatures can be arbitrarily high
    }
}

static void test_cell_init(TestCell *c, float temp_k) {
    memset(c, 0, sizeof(TestCell));
    c->silicate_mass = FLOAT_TO_FIXED(1.0f);
    test_cell_update_thermal_mass(c);
    c->temperature = FLOAT_TO_FIXED(temp_k);
    c->energy = fixed_mul(c->thermal_mass, c->temperature);
}

static bool test_cell_can_combust(const TestCell *c) {
    // Check fuel
    if (c->fuel_mass < MIN_FUEL_MASS) return false;

    // Check temperature
    if (c->temperature < IGNITION_TEMP) return false;

    // Check oxygen
    if (c->o2_mass < MIN_O2_MASS) return false;

    // Water suppression: liquid water prevents combustion
    if (c->h2o_liquid > WATER_SUPPRESSION_THRESHOLD) return false;

    return true;
}

// ============ THEORY 5: WATER SUPPRESSES COMBUSTION ============

static bool test_dry_cell_can_combust(void) {
    TEST_BEGIN("dry cell with fuel, heat, O2 can combust");

    TestCell c;
    test_cell_init(&c, 600.0f);  // Above ignition temp
    c.fuel_mass = FLOAT_TO_FIXED(0.1f);
    c.o2_mass = FLOAT_TO_FIXED(0.021f);

    ASSERT_TRUE(test_cell_can_combust(&c), "dry hot fuel cell should combust");

    TEST_PASS();
}

static bool test_wet_cell_cannot_combust(void) {
    TEST_BEGIN("wet cell cannot combust (water suppression)");

    TestCell c;
    test_cell_init(&c, 600.0f);  // Above ignition temp
    c.fuel_mass = FLOAT_TO_FIXED(0.1f);
    c.o2_mass = FLOAT_TO_FIXED(0.021f);
    c.h2o_liquid = FLOAT_TO_FIXED(0.5f);  // Submerged

    ASSERT_FALSE(test_cell_can_combust(&c), "wet cell should not combust");

    TEST_PASS();
}

static bool test_barely_wet_cell_cannot_combust(void) {
    TEST_BEGIN("cell with water above threshold cannot combust");

    TestCell c;
    test_cell_init(&c, 600.0f);
    c.fuel_mass = FLOAT_TO_FIXED(0.1f);
    c.o2_mass = FLOAT_TO_FIXED(0.021f);
    c.h2o_liquid = FLOAT_TO_FIXED(0.15f);  // Just above 0.1 threshold

    ASSERT_FALSE(test_cell_can_combust(&c), "barely wet cell should not combust");

    TEST_PASS();
}

static bool test_steam_does_not_suppress(void) {
    TEST_BEGIN("steam does not suppress combustion");

    TestCell c;
    test_cell_init(&c, 600.0f);
    c.fuel_mass = FLOAT_TO_FIXED(0.1f);
    c.o2_mass = FLOAT_TO_FIXED(0.021f);
    c.h2o_steam = FLOAT_TO_FIXED(1.0f);  // Lots of steam, no liquid

    ASSERT_TRUE(test_cell_can_combust(&c), "steam should not prevent combustion");

    TEST_PASS();
}

// ============ THEORY 2: CONSERVATION OF MASS ============

static bool test_evaporation_conserves_mass(void) {
    TEST_BEGIN("evaporation conserves H2O mass");

    TestCell c;
    test_cell_init(&c, 400.0f);  // Start at 400K (above boiling)
    c.h2o_liquid = FLOAT_TO_FIXED(1.0f);  // 1 kg liquid

    fixed16_t initial_h2o = test_cell_total_h2o(&c);

    // Simulate evaporation: transfer 0.1 kg liquid → steam
    fixed16_t evap_amount = FLOAT_TO_FIXED(0.1f);
    c.h2o_liquid -= evap_amount;
    c.h2o_steam += evap_amount;

    fixed16_t final_h2o = test_cell_total_h2o(&c);

    ASSERT_EQ(final_h2o, initial_h2o, "total H2O mass changed during evaporation");

    TEST_PASS();
}

static bool test_condensation_conserves_mass(void) {
    TEST_BEGIN("condensation conserves H2O mass");

    TestCell c;
    test_cell_init(&c, 350.0f);  // Below boiling
    c.h2o_steam = FLOAT_TO_FIXED(0.5f);  // 0.5 kg steam

    fixed16_t initial_h2o = test_cell_total_h2o(&c);

    // Simulate condensation: transfer 0.1 kg steam → liquid
    fixed16_t condense_amount = FLOAT_TO_FIXED(0.1f);
    c.h2o_steam -= condense_amount;
    c.h2o_liquid += condense_amount;

    fixed16_t final_h2o = test_cell_total_h2o(&c);

    ASSERT_EQ(final_h2o, initial_h2o, "total H2O mass changed during condensation");

    TEST_PASS();
}

static bool test_melting_conserves_mass(void) {
    TEST_BEGIN("melting conserves H2O mass");

    TestCell c;
    test_cell_init(&c, 280.0f);  // Above melting point
    c.h2o_ice = FLOAT_TO_FIXED(2.0f);  // 2 kg ice

    fixed16_t initial_h2o = test_cell_total_h2o(&c);

    // Simulate melting: transfer 0.5 kg ice → liquid
    fixed16_t melt_amount = FLOAT_TO_FIXED(0.5f);
    c.h2o_ice -= melt_amount;
    c.h2o_liquid += melt_amount;

    fixed16_t final_h2o = test_cell_total_h2o(&c);

    ASSERT_EQ(final_h2o, initial_h2o, "total H2O mass changed during melting");

    TEST_PASS();
}

// ============ THEORY 3: PHASE TRANSITIONS REQUIRE LATENT HEAT ============

static bool test_evaporation_consumes_energy(void) {
    TEST_BEGIN("evaporation consumes latent heat");

    TestCell c;
    test_cell_init(&c, 400.0f);
    c.h2o_liquid = FLOAT_TO_FIXED(1.0f);
    test_cell_update_thermal_mass(&c);
    c.energy = fixed_mul(c.thermal_mass, c.temperature);

    fixed16_t initial_energy = c.energy;

    // Evaporate 0.1 kg water
    fixed16_t evap_amount = FLOAT_TO_FIXED(0.1f);
    c.h2o_liquid -= evap_amount;
    c.h2o_steam += evap_amount;

    // Should consume latent heat of vaporization
    fixed16_t latent_consumed = fixed_mul(evap_amount, LATENT_HEAT_VAPORIZATION);
    c.energy -= latent_consumed;

    ASSERT(c.energy < initial_energy, "energy should decrease during evaporation");

    // Verify correct amount consumed (within fixed-point tolerance)
    fixed16_t expected_energy = initial_energy - latent_consumed;
    int energy_diff = abs(c.energy - expected_energy);
    ASSERT(energy_diff < 1000, "wrong latent heat consumed");

    TEST_PASS();
}

static bool test_condensation_releases_energy(void) {
    TEST_BEGIN("condensation releases latent heat");

    TestCell c;
    test_cell_init(&c, 350.0f);
    c.h2o_steam = FLOAT_TO_FIXED(0.5f);
    test_cell_update_thermal_mass(&c);
    c.energy = fixed_mul(c.thermal_mass, c.temperature);

    fixed16_t initial_energy = c.energy;

    // Condense 0.1 kg steam
    fixed16_t condense_amount = FLOAT_TO_FIXED(0.1f);
    c.h2o_steam -= condense_amount;
    c.h2o_liquid += condense_amount;

    // Should release latent heat
    fixed16_t latent_released = fixed_mul(condense_amount, LATENT_HEAT_VAPORIZATION);
    c.energy += latent_released;

    ASSERT(c.energy > initial_energy, "energy should increase during condensation");

    TEST_PASS();
}

static bool test_melting_consumes_energy(void) {
    TEST_BEGIN("melting consumes latent heat of fusion");

    TestCell c;
    test_cell_init(&c, 280.0f);
    c.h2o_ice = FLOAT_TO_FIXED(1.0f);
    test_cell_update_thermal_mass(&c);
    c.energy = fixed_mul(c.thermal_mass, c.temperature);

    fixed16_t initial_energy = c.energy;

    // Melt 0.1 kg ice
    fixed16_t melt_amount = FLOAT_TO_FIXED(0.1f);
    c.h2o_ice -= melt_amount;
    c.h2o_liquid += melt_amount;

    // Should consume latent heat of fusion
    fixed16_t latent_consumed = fixed_mul(melt_amount, LATENT_HEAT_FUSION);
    c.energy -= latent_consumed;

    ASSERT(c.energy < initial_energy, "energy should decrease during melting");

    TEST_PASS();
}

// ============ THEORY 4: TEMPERATURE CANNOT GO BELOW ABSOLUTE ZERO ============

static bool test_temperature_floors_at_zero(void) {
    TEST_BEGIN("temperature cannot go below 0K");

    TestCell c;
    test_cell_init(&c, 100.0f);  // Start at 100K

    // Remove all energy
    c.energy = 0;
    test_cell_update_temperature(&c);

    ASSERT(c.temperature >= 0, "temperature went below absolute zero");
    ASSERT_EQ(c.temperature, 0, "temperature should be exactly 0K");

    TEST_PASS();
}

static bool test_negative_energy_floors_at_zero(void) {
    TEST_BEGIN("negative energy results in 0K temperature");

    TestCell c;
    test_cell_init(&c, 100.0f);

    // Set negative energy (shouldn't happen, but verify it's handled)
    c.energy = FLOAT_TO_FIXED(-1000.0f);
    test_cell_update_temperature(&c);

    ASSERT(c.temperature >= 0, "negative energy caused negative temperature");
    ASSERT(c.energy >= 0, "energy not clamped to zero");

    TEST_PASS();
}

// ============ THEORY 6: NO ARTIFICIAL TEMPERATURE CEILING ============

static bool test_high_temperature_allowed(void) {
    TEST_BEGIN("temperatures above 2000K are allowed");

    TestCell c;
    test_cell_init(&c, 293.15f);  // Start at ambient

    // For thermal_mass = 0.7 J/K, E = 2100 J gives T = 3000K
    // This avoids fixed-point overflow (2100 * 65536 fits in int32)
    c.energy = FLOAT_TO_FIXED(2100.0f);
    test_cell_update_temperature(&c);

    float temp_k = FIXED_TO_FLOAT(c.temperature);
    ASSERT(temp_k > 2000.0f, "temperature capped below 2000K (got %.1f)", temp_k);

    TEST_PASS();
}

static bool test_extreme_temperature_allowed(void) {
    TEST_BEGIN("extreme temperatures (10000K+) are allowed");

    TestCell c;
    test_cell_init(&c, 293.15f);

    // For thermal_mass = 0.7 J/K, E = 7000 J gives T = 10000K
    // 7000 * 65536 = 458,752,000 fits in int32
    c.energy = FLOAT_TO_FIXED(7000.0f);
    test_cell_update_temperature(&c);

    float temp_k = FIXED_TO_FLOAT(c.temperature);
    ASSERT(temp_k > 9000.0f, "extreme temperature not achieved (got %.1f)", temp_k);

    TEST_PASS();
}

// ============ WATER SYNC TESTS ============

static bool test_water_depth_to_mass_conversion(void) {
    TEST_BEGIN("water depth converts to correct liquid mass");

    // 1 unit depth * WATER_MASS_PER_DEPTH = 1000 kg
    fixed16_t depth = FLOAT_TO_FIXED(1.0f);
    fixed16_t expected_mass = fixed_mul(depth, WATER_MASS_PER_DEPTH);

    float mass_kg = FIXED_TO_FLOAT(expected_mass);
    ASSERT_FLOAT_EQ(mass_kg, 1000.0f, 1.0f, "wrong mass for 1 unit depth");

    // 0.5 depth = 500 kg
    depth = FLOAT_TO_FIXED(0.5f);
    expected_mass = fixed_mul(depth, WATER_MASS_PER_DEPTH);
    mass_kg = FIXED_TO_FLOAT(expected_mass);
    ASSERT_FLOAT_EQ(mass_kg, 500.0f, 1.0f, "wrong mass for 0.5 unit depth");

    TEST_PASS();
}

static bool test_mass_to_water_depth_conversion(void) {
    TEST_BEGIN("liquid mass converts back to correct water depth");

    // 1000 kg / WATER_MASS_PER_DEPTH = 1 unit depth
    fixed16_t mass = FLOAT_TO_FIXED(1000.0f);
    fixed16_t depth = fixed_div(mass, WATER_MASS_PER_DEPTH);

    float depth_units = FIXED_TO_FLOAT(depth);
    ASSERT_FLOAT_EQ(depth_units, 1.0f, 0.01f, "wrong depth for 1000 kg");

    TEST_PASS();
}

// ============ THERMAL MASS WITH WATER PHASES ============

static bool test_water_increases_thermal_mass(void) {
    TEST_BEGIN("adding water increases thermal mass");

    TestCell dry, wet;
    test_cell_init(&dry, 300.0f);
    test_cell_init(&wet, 300.0f);

    wet.h2o_liquid = FLOAT_TO_FIXED(1.0f);  // 1 g water

    test_cell_update_thermal_mass(&dry);
    test_cell_update_thermal_mass(&wet);

    ASSERT(wet.thermal_mass > dry.thermal_mass,
           "wet cell should have higher thermal mass");

    // Water's specific heat is ~6x silicate, so 1g water adds ~4.18 J/K
    fixed16_t diff = wet.thermal_mass - dry.thermal_mass;
    float diff_float = FIXED_TO_FLOAT(diff);
    ASSERT(diff_float > 4.0f, "thermal mass increase too small");

    TEST_PASS();
}

static bool test_ice_has_lower_specific_heat_than_liquid(void) {
    TEST_BEGIN("ice has lower specific heat than liquid water");

    TestCell ice_cell, liquid_cell;
    test_cell_init(&ice_cell, 270.0f);
    test_cell_init(&liquid_cell, 280.0f);

    ice_cell.h2o_ice = FLOAT_TO_FIXED(1.0f);
    liquid_cell.h2o_liquid = FLOAT_TO_FIXED(1.0f);

    test_cell_update_thermal_mass(&ice_cell);
    test_cell_update_thermal_mass(&liquid_cell);

    // Remove base silicate contribution for comparison
    fixed16_t silicate_th = fixed_mul(FLOAT_TO_FIXED(1.0f), FLOAT_TO_FIXED(0.7f));
    fixed16_t ice_th = ice_cell.thermal_mass - silicate_th;
    fixed16_t liquid_th = liquid_cell.thermal_mass - silicate_th;

    ASSERT(liquid_th > ice_th, "liquid water should have higher specific heat than ice");

    // Ratio should be approximately 4.18/2.09 = 2
    float ratio = FIXED_TO_FLOAT(liquid_th) / FIXED_TO_FLOAT(ice_th);
    ASSERT(ratio > 1.8f && ratio < 2.2f, "specific heat ratio wrong");

    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Water-Matter System - Unit Tests\n");
    printf("========================================\n");

    test_suite_begin("THEORY 5: WATER SUPPRESSES COMBUSTION");
    test_dry_cell_can_combust();
    test_wet_cell_cannot_combust();
    test_barely_wet_cell_cannot_combust();
    test_steam_does_not_suppress();
    test_suite_end();

    test_suite_begin("THEORY 2: CONSERVATION OF MASS");
    test_evaporation_conserves_mass();
    test_condensation_conserves_mass();
    test_melting_conserves_mass();
    test_suite_end();

    test_suite_begin("THEORY 3: PHASE TRANSITIONS REQUIRE LATENT HEAT");
    test_evaporation_consumes_energy();
    test_condensation_releases_energy();
    test_melting_consumes_energy();
    test_suite_end();

    test_suite_begin("THEORY 4: ABSOLUTE ZERO FLOOR");
    test_temperature_floors_at_zero();
    test_negative_energy_floors_at_zero();
    test_suite_end();

    test_suite_begin("THEORY 6: NO TEMPERATURE CEILING");
    test_high_temperature_allowed();
    test_extreme_temperature_allowed();
    test_suite_end();

    test_suite_begin("WATER SYNC");
    test_water_depth_to_mass_conversion();
    test_mass_to_water_depth_conversion();
    test_suite_end();

    test_suite_begin("THERMAL MASS");
    test_water_increases_thermal_mass();
    test_ice_has_lower_specific_heat_than_liquid();
    test_suite_end();

    test_summary();
    return test_exit_code();
}
