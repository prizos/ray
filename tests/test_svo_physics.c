/**
 * test_svo_physics.c - SVO Physics Unit and Integration Tests
 *
 * Tests the physics simulation for the 3D SVO matter system.
 * Organized in tiers:
 *   1. Unit tests - Individual function correctness
 *   2. Conservation tests - Mass and energy conservation
 *   3. Flow behavior tests - Liquid/gas movement
 *   4. Phase transition tests - State changes
 */

#include "test_common.h"
#include "terrain.h"

// Extended assertions for this file
#define ASSERT_GT(a, b, msg) \
    if (!((a) > (b))) { \
        TEST_FAIL(msg " (%.6f not > %.6f)", (double)(a), (double)(b)); \
    }

#define ASSERT_LT(a, b, msg) \
    if (!((a) < (b))) { \
        TEST_FAIL(msg " (%.6f not < %.6f)", (double)(a), (double)(b)); \
    }

#define ASSERT_GTE(a, b, msg) \
    if (!((a) >= (b))) { \
        TEST_FAIL(msg " (%.6f not >= %.6f)", (double)(a), (double)(b)); \
    }

// ============ HELPER FUNCTIONS ============

// Initialize a minimal ChunkWorld for testing
static bool init_test_svo(ChunkWorld *world) {
    world_init(world);
    return true;  // world_init always succeeds
}

// Calculate total moles of a specific material across the entire world
static double calculate_total_moles(ChunkWorld *world, MaterialType type) {
    double total = 0.0;
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = world->hash_table[h];
        while (chunk) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_SIZE; y++) {
                    for (int x = 0; x < CHUNK_SIZE; x++) {
                        const Cell3D *cell = chunk_get_cell_const(chunk, x, y, z);
                        if (CELL_HAS_MATERIAL(cell, type)) {
                            total += cell->materials[type].moles;
                        }
                    }
                }
            }
            chunk = chunk->hash_next;
        }
    }
    return total;
}

// Calculate total thermal energy across the entire world
static double calculate_total_energy(ChunkWorld *world) {
    double total = 0.0;
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = world->hash_table[h];
        while (chunk) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_SIZE; y++) {
                    for (int x = 0; x < CHUNK_SIZE; x++) {
                        const Cell3D *cell = chunk_get_cell_const(chunk, x, y, z);
                        CELL_FOR_EACH_MATERIAL(cell, type) {
                            total += cell->materials[type].thermal_energy;
                        }
                    }
                }
            }
            chunk = chunk->hash_next;
        }
    }
    return total;
}

// Run physics for N steps with metric recording
static void run_physics_steps(ChunkWorld *world, int steps) {
    for (int i = 0; i < steps; i++) {
        world_physics_step(world, 0.016f);
        TEST_RECORD_PHYSICS_STEP();
    }
    TEST_RECORD_ACTIVE_NODES(world->active_count);
}

// ============================================================================
//                      TIER 1: UNIT TESTS
// ============================================================================

// --- Temperature Calculation Tests ---

static bool test_temperature_from_energy(void) {
    TEST_BEGIN("temperature calculation with latent heat");

    MaterialState state;
    state.moles = 2.0;  // 2 moles of water
    double target_temp = 300.0;  // Target: 300K (liquid water)
    double Cp_s = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_solid;
    double Cp_l = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;
    double Tm = MATERIAL_PROPS[MAT_WATER].melting_point;  // 273.15K
    double Hf = MATERIAL_PROPS[MAT_WATER].enthalpy_fusion; // 6010 J/mol

    // For liquid water at 300K:
    // E = n * Cp_s * Tm + n * Hf + n * Cp_l * (T - Tm)
    state.thermal_energy = state.moles * Cp_s * Tm +     // Energy to reach melting point (solid)
                           state.moles * Hf +             // Latent heat of fusion
                           state.moles * Cp_l * (target_temp - Tm);  // Energy to heat liquid

    double calculated_temp = material_get_temperature(&state, MAT_WATER);

    ASSERT_FLOAT_EQ(calculated_temp, target_temp, 0.01, "temperature calculation incorrect");
    TEST_PASS();
}

static bool test_temperature_zero_moles_returns_zero(void) {
    TEST_BEGIN("zero moles returns 0.0 (vacuum has no temperature)");

    MaterialState state = {0};  // Zero-initialize all fields including cache
    state.moles = 0.0;
    state.thermal_energy = 0.0;
    state.temp_valid = false;  // Ensure cache is invalid

    double temp = material_get_temperature(&state, MAT_WATER);
    // Vacuum (no matter) has no temperature - 0.0 is the sentinel value
    ASSERT_FLOAT_EQ(temp, 0.0, 0.01, "should return 0.0 for zero moles (vacuum)");
    TEST_PASS();
}

static bool test_temperature_negative_energy_gives_low_temp(void) {
    TEST_BEGIN("negative energy gives temperature below ambient");

    // This tests that we don't crash on edge cases
    // In reality, negative thermal energy shouldn't occur
    MaterialState state = {0};  // Zero-initialize all fields including cache
    state.moles = 1.0;
    state.thermal_energy = -1000.0;  // Negative (invalid but shouldn't crash)
    state.temp_valid = false;  // Ensure cache is invalid

    double temp = material_get_temperature(&state, MAT_WATER);
    // Just verify it returns something reasonable (negative temp is physically impossible)
    // The system should handle this gracefully
    ASSERT(temp < INITIAL_TEMP_K, "negative energy should give low temp");
    TEST_PASS();
}

static bool test_cell_temperature_weighted_average(void) {
    TEST_BEGIN("cell temperature is weighted by heat capacity");

    Cell3D cell;
    cell3d_init(&cell);

    // Add 1 mol water at 400K (gas phase - needs latent heat)
    double water_moles = 1.0;
    double water_hc = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_gas;  // Gas at 400K
    double water_temp = 400.0;
    double water_energy = calculate_material_energy(MAT_WATER, water_moles, water_temp);
    cell3d_add_material(&cell, MAT_WATER, water_moles, water_energy);

    // Add 1 mol air at 200K (gas phase - N2 boils at 77K, so at 200K it's gas)
    double air_moles = 1.0;
    double air_hc = MATERIAL_PROPS[MAT_AIR].molar_heat_capacity_gas;  // Air is always gas
    double air_temp = 200.0;
    double air_energy = calculate_material_energy(MAT_AIR, air_moles, air_temp);
    cell3d_add_material(&cell, MAT_AIR, air_moles, air_energy);

    double cell_temp = cell_get_temperature(&cell);

    // Expected: weighted average of temperatures by heat capacity
    double expected_temp = (water_temp * water_hc + air_temp * air_hc)
                          / (water_hc + air_hc);

    ASSERT_FLOAT_EQ(cell_temp, expected_temp, 0.1, "weighted temperature incorrect");

    cell3d_free(&cell);
    TEST_PASS();
}

// --- Phase Determination Tests ---

static bool test_water_phase_solid_below_273(void) {
    TEST_BEGIN("water is solid below 273K");

    Phase phase = material_get_phase(MAT_WATER, 260.0);
    ASSERT(phase == PHASE_SOLID, "should be solid at 260K");
    TEST_PASS();
}

static bool test_water_phase_liquid_273_to_373(void) {
    TEST_BEGIN("water is liquid between 273K and 373K");

    Phase phase_low = material_get_phase(MAT_WATER, 280.0);
    Phase phase_mid = material_get_phase(MAT_WATER, 320.0);
    Phase phase_high = material_get_phase(MAT_WATER, 370.0);

    ASSERT(phase_low == PHASE_LIQUID, "should be liquid at 280K");
    ASSERT(phase_mid == PHASE_LIQUID, "should be liquid at 320K");
    ASSERT(phase_high == PHASE_LIQUID, "should be liquid at 370K");
    TEST_PASS();
}

static bool test_water_phase_gas_above_373(void) {
    TEST_BEGIN("water is gas above 373K");

    Phase phase = material_get_phase(MAT_WATER, 400.0);
    ASSERT(phase == PHASE_GAS, "should be gas at 400K");
    TEST_PASS();
}

static bool test_rock_phase_solid_at_room_temp(void) {
    TEST_BEGIN("rock is solid at room temperature (293K)");

    Phase phase = material_get_phase(MAT_ROCK, 293.0);  // Room temperature
    ASSERT(phase == PHASE_SOLID, "rock should be solid at 293K");
    TEST_PASS();
}

// --- Material Property Tests ---

static bool test_material_properties_defined(void) {
    TEST_BEGIN("material properties are defined for all types");

    for (int i = 0; i < MAT_COUNT; i++) {
        const MaterialProperties *props = &MATERIAL_PROPS[i];
        ASSERT(props->name != NULL, "name should not be NULL");
        // Molar heat capacities should be positive for real materials (except NONE)
        if (i != MAT_NONE) {
            ASSERT(props->molar_heat_capacity_solid > 0, "solid heat capacity should be positive");
            ASSERT(props->molar_heat_capacity_liquid > 0, "liquid heat capacity should be positive");
            ASSERT(props->molar_heat_capacity_gas > 0, "gas heat capacity should be positive");
        }
    }
    TEST_PASS();
}

static bool test_water_properties_correct(void) {
    TEST_BEGIN("water has correct physical properties");

    const MaterialProperties *water = &MATERIAL_PROPS[MAT_WATER];

    ASSERT_FLOAT_EQ(water->molar_mass, 0.018, 0.001, "water molar mass should be 18g/mol");
    ASSERT_FLOAT_EQ(water->melting_point, 273.15, 0.1, "water melting point should be 273.15K");
    ASSERT_FLOAT_EQ(water->boiling_point, 373.15, 0.1, "water boiling point should be 373.15K");
    TEST_PASS();
}

// --- Tool API Tests ---

static bool test_add_water_creates_water_material(void) {
    TEST_BEGIN("svo_add_water_at creates water material");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 5.0);

    CellInfo info = svo_get_cell_info(&svo, 0.0f, 0.0f, 0.0f);
    ASSERT(info.valid, "cell should be valid");
    ASSERT(info.material_count >= 1, "should have materials");

    // Check for water
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);
    const Cell3D *cell = svo_get_cell(&svo, cx, cy, cz);
    ASSERT(cell != NULL, "cell should exist");

    const MaterialEntry *water = cell3d_find_material_const(cell, MAT_WATER);
    ASSERT(water != NULL, "water should exist in cell");
    ASSERT_FLOAT_EQ(water->state.moles, 5.0, 0.01, "should have 5 moles");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_add_heat_increases_temperature(void) {
    TEST_BEGIN("svo_add_heat_at increases temperature");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // First add water so we have something to heat
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 1.0);

    CellInfo before = svo_get_cell_info(&svo, 0.0f, 0.0f, 0.0f);
    double temp_before = before.temperature;

    svo_add_heat_at(&svo, 0.0f, 0.0f, 0.0f, 10000.0);

    CellInfo after = svo_get_cell_info(&svo, 0.0f, 0.0f, 0.0f);
    double temp_after = after.temperature;

    ASSERT_GT(temp_after, temp_before, "temperature should increase after adding heat");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_remove_heat_decreases_temperature(void) {
    TEST_BEGIN("svo_remove_heat_at decreases temperature");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // First add water so we have something to heat/cool
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 1.0);

    // First add heat to get above ambient
    svo_add_heat_at(&svo, 0.0f, 0.0f, 0.0f, 10000.0);
    CellInfo before = svo_get_cell_info(&svo, 0.0f, 0.0f, 0.0f);

    // Now remove heat
    svo_remove_heat_at(&svo, 0.0f, 0.0f, 0.0f, 5000.0);
    CellInfo after = svo_get_cell_info(&svo, 0.0f, 0.0f, 0.0f);

    ASSERT_LT(after.temperature, before.temperature, "temperature should decrease");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_temperature_cannot_go_below_zero(void) {
    TEST_BEGIN("temperature cannot go below absolute zero");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // First add water so we have something to cool
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 1.0);

    // Try to remove massive amount of heat
    for (int i = 0; i < 100; i++) {
        svo_remove_heat_at(&svo, 0.0f, 0.0f, 0.0f, 1000000.0);
    }

    CellInfo info = svo_get_cell_info(&svo, 0.0f, 0.0f, 0.0f);
    ASSERT(info.valid, "cell should be valid");
    ASSERT_GTE(info.temperature, 0.0, "temperature should not be negative");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                      TIER 2: CONSERVATION TESTS
// ============================================================================

static bool test_mass_conserved_no_simulation(void) {
    TEST_BEGIN("mass conserved without simulation");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water at several locations
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 10.0);
    svo_add_water_at(&svo, 5.0f, 0.0f, 5.0f, 10.0);
    svo_add_water_at(&svo, -5.0f, 0.0f, -5.0f, 10.0);

    double water_before = calculate_total_moles(&svo, MAT_WATER);

    // No simulation, just check mass is as expected
    ASSERT_FLOAT_EQ(water_before, 30.0, 1.0, "should have 30 moles of water");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_energy_conserved_uniform_temperature(void) {
    TEST_BEGIN("energy conserved at uniform temperature (no physics)");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add materials at same temperature
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 5.0);

    double energy_before = calculate_total_energy(&svo);

    // Don't run physics - just verify initial state
    ASSERT(energy_before > 0, "should have positive energy");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_moles_conserved_after_physics(void) {
    TEST_BEGIN("water moles conserved after physics step");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 10.0);

    // Sample water moles in the cell we added to
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    const Cell3D *cell_before = svo_get_cell(&svo, cx, cy, cz);
    const MaterialEntry *water_before = cell3d_find_material_const(cell_before, MAT_WATER);
    double moles_before = water_before ? water_before->state.moles : 0;

    // Run a few physics steps
    run_physics_steps(&svo, 10);

    // Check water - it may have flowed but total should be similar
    // (in a closed system without terrain, water shouldn't disappear)
    double total_water_after = calculate_total_moles(&svo, MAT_WATER);

    // Allow some tolerance for numerical errors
    ASSERT_FLOAT_EQ(total_water_after, moles_before, moles_before * 0.1, "water mass changed");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                      TIER 3: FLOW BEHAVIOR TESTS
// ============================================================================

static bool test_liquid_flows_down(void) {
    TEST_BEGIN("liquid water flows downward");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water high up (y > 0)
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 10.0f, 0.0f, &cx, &cy, &cz);

    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (cell) {
        double water_moles = 5.0;
        double Cp_s = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_solid;
        double Cp_l = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;
        double Tm = MATERIAL_PROPS[MAT_WATER].melting_point;
        double Hf = MATERIAL_PROPS[MAT_WATER].enthalpy_fusion;

        // For liquid water at 293K, must include latent heat:
        // E = n * Cp_s * Tm + n * Hf + n * Cp_l * (T - Tm)
        double energy = water_moles * Cp_s * Tm +
                        water_moles * Hf +
                        water_moles * Cp_l * (INITIAL_TEMP_K - Tm);
        cell3d_add_material(cell, MAT_WATER, water_moles, energy);

        // Mark cell active so physics will process it
        svo_mark_cell_active(&svo, cx, cy, cz);
    }

    // Get initial water at upper cell
    const Cell3D *upper_before = svo_get_cell(&svo, cx, cy, cz);
    const MaterialEntry *water_upper_before = upper_before ?
        cell3d_find_material_const(upper_before, MAT_WATER) : NULL;
    double upper_moles_before = water_upper_before ? water_upper_before->state.moles : 0;

    // Run physics
    run_physics_steps(&svo, 100);

    // Check if water has decreased at upper cell (flowed down)
    const Cell3D *upper_after = svo_get_cell(&svo, cx, cy, cz);
    const MaterialEntry *water_upper_after = upper_after ?
        cell3d_find_material_const(upper_after, MAT_WATER) : NULL;
    double upper_moles_after = water_upper_after ? water_upper_after->state.moles : 0;

    // Water should have flowed down
    ASSERT_LT(upper_moles_after, upper_moles_before, "water should flow down from upper cell");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_flows_into_air_occupied_cell(void) {
    TEST_BEGIN("water flows into cell occupied by air (not vacuum)");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Set up: water cell above, air cell below
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 10.0f, 0.0f, &cx, &cy, &cz);

    // Add water at upper cell
    Cell3D *water_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!water_cell) { TEST_FAIL("couldn't get water cell"); }
    double water_moles = 5.0;
    double water_energy = calculate_material_energy(MAT_WATER, water_moles, INITIAL_TEMP_K);
    cell3d_add_material(water_cell, MAT_WATER, water_moles, water_energy);
    svo_mark_cell_active(&svo, cx, cy, cz);

    // Add air at lower cell (not vacuum - has material)
    Cell3D *air_cell = svo_get_cell_for_write(&svo, cx, cy - 1, cz);
    if (!air_cell) { TEST_FAIL("couldn't get air cell"); }
    double air_moles = 1.0;
    double air_energy = calculate_material_energy(MAT_AIR, air_moles, INITIAL_TEMP_K);
    cell3d_add_material(air_cell, MAT_AIR, air_moles, air_energy);
    svo_mark_cell_active(&svo, cx, cy - 1, cz);

    // Verify initial state
    double water_before = water_cell->materials[MAT_WATER].moles;
    ASSERT_FLOAT_EQ(water_before, 5.0, 0.01, "should start with 5 moles water");

    // Run physics (liquid flow only)
    for (int i = 0; i < 50; i++) {
        world_physics_step_flags(&svo, 0.016f, PHYSICS_LIQUID_FLOW);
    }

    // Check: water should have flowed into the air cell
    const Cell3D *lower_after = svo_get_cell(&svo, cx, cy - 1, cz);
    ASSERT(lower_after != NULL, "lower cell should exist");
    ASSERT(CELL_HAS_MATERIAL(lower_after, MAT_WATER), "water should have flowed into air cell");

    double water_in_lower = lower_after->materials[MAT_WATER].moles;
    ASSERT_GT(water_in_lower, 0.0, "lower cell should have water");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_does_not_flow_into_vacuum(void) {
    TEST_BEGIN("heat does NOT flow into vacuum (conduction requires matter)");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Set up: hot water cell, vacuum cell adjacent
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Add hot water
    Cell3D *hot_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!hot_cell) { TEST_FAIL("couldn't get hot cell"); }
    double water_moles = 5.0;
    double hot_temp = 400.0;  // Hot water (steam)
    double water_energy = calculate_material_energy(MAT_WATER, water_moles, hot_temp);
    cell3d_add_material(hot_cell, MAT_WATER, water_moles, water_energy);
    svo_mark_cell_active(&svo, cx, cy, cz);

    // Neighbor cell is vacuum (empty) - don't add anything
    // Just verify it's empty
    const Cell3D *vacuum_cell = svo_get_cell(&svo, cx + 1, cy, cz);
    bool neighbor_is_vacuum = (vacuum_cell == NULL || vacuum_cell->present == 0);
    ASSERT(neighbor_is_vacuum, "neighbor should be vacuum");

    // Record energy before
    double energy_before = hot_cell->materials[MAT_WATER].thermal_energy;

    // Run heat conduction only
    for (int i = 0; i < 100; i++) {
        world_physics_step_flags(&svo, 0.016f, PHYSICS_HEAT_CONDUCT);
    }

    // Get cell again (pointer may have changed)
    hot_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    double energy_after = hot_cell->materials[MAT_WATER].thermal_energy;

    // Energy should NOT have changed (no conduction to vacuum)
    // Allow tiny tolerance for floating point
    ASSERT_FLOAT_EQ(energy_after, energy_before, 0.1, "energy should not leak to vacuum");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_flows_between_matter_cells(void) {
    TEST_BEGIN("heat DOES flow between adjacent matter cells");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Set up: hot cell and cold cell adjacent
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Add hot water at (cx, cy, cz)
    Cell3D *hot_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!hot_cell) { TEST_FAIL("couldn't get hot cell"); }
    double water_moles = 5.0;
    double hot_temp = 400.0;
    double hot_energy = calculate_material_energy(MAT_WATER, water_moles, hot_temp);
    cell3d_add_material(hot_cell, MAT_WATER, water_moles, hot_energy);
    svo_mark_cell_active(&svo, cx, cy, cz);

    // Add cold water at (cx+1, cy, cz)
    Cell3D *cold_cell = svo_get_cell_for_write(&svo, cx + 1, cy, cz);
    if (!cold_cell) { TEST_FAIL("couldn't get cold cell"); }
    double cold_temp = 280.0;  // Just above freezing (liquid)
    double cold_energy = calculate_material_energy(MAT_WATER, water_moles, cold_temp);
    cell3d_add_material(cold_cell, MAT_WATER, water_moles, cold_energy);
    svo_mark_cell_active(&svo, cx + 1, cy, cz);

    // Record temperatures before
    double temp_hot_before = cell_get_temperature(hot_cell);
    double temp_cold_before = cell_get_temperature(cold_cell);
    ASSERT_GT(temp_hot_before, temp_cold_before, "hot should be hotter than cold");

    // Run heat conduction only
    for (int i = 0; i < 100; i++) {
        world_physics_step_flags(&svo, 0.016f, PHYSICS_HEAT_CONDUCT);
    }

    // Get cells again
    hot_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    cold_cell = svo_get_cell_for_write(&svo, cx + 1, cy, cz);

    double temp_hot_after = cell_get_temperature(hot_cell);
    double temp_cold_after = cell_get_temperature(cold_cell);

    // Heat should have flowed: hot cooled, cold warmed
    ASSERT_LT(temp_hot_after, temp_hot_before, "hot cell should cool down");
    ASSERT_GT(temp_cold_after, temp_cold_before, "cold cell should warm up");

    // Temperature difference should have decreased
    double diff_before = temp_hot_before - temp_cold_before;
    double diff_after = temp_hot_after - temp_cold_after;
    ASSERT_LT(diff_after, diff_before, "temperature difference should decrease");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                      TIER 4: PHASE TRANSITION TESTS
// ============================================================================

static bool test_water_phase_determined_by_temperature(void) {
    TEST_BEGIN("water phase determined by temperature");

    // Ice (T < 273.15K)
    Phase ice_phase = material_get_phase(MAT_WATER, 260.0);
    ASSERT(ice_phase == PHASE_SOLID, "260K should be solid");

    // Liquid (273.15K < T < 373.15K)
    Phase liquid_phase = material_get_phase(MAT_WATER, 300.0);
    ASSERT(liquid_phase == PHASE_LIQUID, "300K should be liquid");

    // Gas (T > 373.15K)
    Phase gas_phase = material_get_phase(MAT_WATER, 400.0);
    ASSERT(gas_phase == PHASE_GAS, "400K should be gas");

    TEST_PASS();
}

// ============================================================================
//                      RUN ALL TESTS
// ============================================================================

typedef bool (*TestFunc)(void);

typedef struct {
    const char *category;
    const char *name;
    TestFunc func;
} TestCase;

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("    SVO PHYSICS TESTS\n");
    printf("========================================\n");

    TestCase tests[] = {
        // Tier 1: Unit Tests
        {"UNIT: Temperature", "temperature_from_energy", test_temperature_from_energy},
        {"UNIT: Temperature", "zero_moles_returns_zero", test_temperature_zero_moles_returns_zero},
        {"UNIT: Temperature", "negative_energy_gives_low_temp", test_temperature_negative_energy_gives_low_temp},
        {"UNIT: Temperature", "cell_temperature_weighted_average", test_cell_temperature_weighted_average},

        {"UNIT: Phase", "water_phase_solid_below_273", test_water_phase_solid_below_273},
        {"UNIT: Phase", "water_phase_liquid_273_to_373", test_water_phase_liquid_273_to_373},
        {"UNIT: Phase", "water_phase_gas_above_373", test_water_phase_gas_above_373},
        {"UNIT: Phase", "rock_phase_solid_at_room_temp", test_rock_phase_solid_at_room_temp},

        {"UNIT: Properties", "material_properties_defined", test_material_properties_defined},
        {"UNIT: Properties", "water_properties_correct", test_water_properties_correct},

        {"UNIT: Tool APIs", "add_water_creates_material", test_add_water_creates_water_material},
        {"UNIT: Tool APIs", "add_heat_increases_temp", test_add_heat_increases_temperature},
        {"UNIT: Tool APIs", "remove_heat_decreases_temp", test_remove_heat_decreases_temperature},
        {"UNIT: Tool APIs", "temp_cannot_go_below_zero", test_temperature_cannot_go_below_zero},

        // Tier 2: Conservation Tests
        {"CONSERVATION", "mass_conserved_no_simulation", test_mass_conserved_no_simulation},
        {"CONSERVATION", "energy_conserved_uniform_temp", test_energy_conserved_uniform_temperature},
        {"CONSERVATION", "water_moles_conserved_physics", test_water_moles_conserved_after_physics},

        // Tier 3: Flow Tests
        {"FLOW", "liquid_flows_down", test_liquid_flows_down},
        {"FLOW", "water_flows_into_air", test_water_flows_into_air_occupied_cell},

        // Tier 3b: Heat Conduction Tests
        {"HEAT", "no_conduction_to_vacuum", test_heat_does_not_flow_into_vacuum},
        {"HEAT", "conduction_between_matter", test_heat_flows_between_matter_cells},

        // Tier 4: Phase Tests
        {"PHASE", "water_phase_by_temperature", test_water_phase_determined_by_temperature},
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    const char *current_category = "";

    for (int i = 0; i < num_tests; i++) {
        if (strcmp(current_category, tests[i].category) != 0) {
            if (current_category[0] != '\0') {
                test_suite_end();
            }
            test_suite_begin(tests[i].category);
            current_category = tests[i].category;
        }
        tests[i].func();
    }

    if (current_category[0] != '\0') {
        test_suite_end();
    }

    test_summary();
    return test_exit_code();
}
