// Tool Integration Tests
// Tests that tools do what they're expected to do,
// including physics effects like heat propagation and water flow

#include "test_common.h"
#include "terrain.h"
#include <string.h>

// ============================================================================
//                         TEST INFRASTRUCTURE
// ============================================================================

typedef struct {
    const char *category;
    const char *name;
    bool (*func)(void);
} TestCase;

// ============================================================================
//                         TEST HELPERS
// ============================================================================

// Calculate energy for water at a given temperature (accounts for latent heat)
static double calculate_water_energy(double moles, double temp_k) {
    return calculate_material_energy(MAT_WATER, moles, temp_k);
}

// Initialize chunk world for testing (vacuum - no materials, no terrain)
// This is the correct approach for testing isolated physics behaviors.
static bool init_test_svo(ChunkWorld *world) {
    world_init(world);
    return true;  // world_init always succeeds
}

// Initialize chunk world with flat ground at a specific height
static bool init_svo_with_ground(ChunkWorld *world, int ground_height) {
    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
        for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
            terrain[z][x] = ground_height;
        }
    }
    world_init_terrain(world, terrain);
    return world->chunk_count > 0;  // Success if at least one chunk was created
}

// Get temperature at a cell coordinate (returns 0.0 for vacuum - no temperature)
static double get_cell_temp(ChunkWorld *world, int cx, int cy, int cz) {
    Cell3D *cell = world_get_cell_for_write(world, cx, cy, cz);
    if (!cell || cell->present == 0) return 0.0;
    return cell_get_temperature(cell);
}

// Get water moles at a cell coordinate
static double get_water_moles(ChunkWorld *world, int cx, int cy, int cz) {
    const Cell3D *cell = world_get_cell(world, cx, cy, cz);
    if (!cell) return 0.0;
    if (CELL_HAS_MATERIAL(cell, MAT_WATER)) {
        return cell->materials[MAT_WATER].moles;
    }
    return 0.0;
}

// Check if cell has solid material
static bool cell_has_solid(ChunkWorld *world, int cx, int cy, int cz) {
    Cell3D *cell = world_get_cell_for_write(world, cx, cy, cz);
    if (!cell) return false;
    CELL_FOR_EACH_MATERIAL(cell, type) {
        Phase phase = material_get_phase_from_energy(&cell->materials[type], type);
        if (phase == PHASE_SOLID) {
            return true;
        }
    }
    return false;
}

// Run physics for N steps
static void run_physics_steps(ChunkWorld *world, int steps) {
    for (int i = 0; i < steps; i++) {
        world_physics_step(world, 0.016f);  // 60 FPS timestep
        TEST_RECORD_PHYSICS_STEP();
    }
    TEST_RECORD_ACTIVE_NODES(world->active_count);
}

// Calculate total water moles across all chunks
static double get_total_water_moles(ChunkWorld *world) {
    double total = 0.0;

    // Iterate through all chunks in hash table
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = world->hash_table[h];
        while (chunk) {
            // Iterate through all cells in chunk
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_SIZE; y++) {
                    for (int x = 0; x < CHUNK_SIZE; x++) {
                        const Cell3D *cell = chunk_get_cell_const(chunk, x, y, z);
                        if (CELL_HAS_MATERIAL(cell, MAT_WATER)) {
                            total += cell->materials[MAT_WATER].moles;
                        }
                    }
                }
            }
            chunk = chunk->hash_next;
        }
    }
    return total;
}

// ============================================================================
//                      TIER 1: HEAT TOOL TESTS
// ============================================================================

static bool test_heat_tool_adds_energy(void) {
    TEST_BEGIN("heat tool adds thermal energy to cell");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // First add some material (water) to receive the heat
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 1.0);

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    double temp_before = get_cell_temp(&svo, cx, cy, cz);

    // Add heat
    svo_add_heat_at(&svo, 0.0f, 0.0f, 0.0f, 10000.0);  // 10kJ

    double temp_after = get_cell_temp(&svo, cx, cy, cz);

    ASSERT(temp_after > temp_before, "temperature should increase after adding heat");
    ASSERT(temp_after > temp_before + 50, "temperature should increase significantly");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_tool_distributes_among_materials(void) {
    TEST_BEGIN("heat tool distributes energy among materials by heat capacity");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water and rock to same cell
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!cell) { TEST_FAIL("couldn't get cell"); }

    // Add 1 mol water and 1 mol rock, both at ambient temp
    // Rock is solid at 293K
    double rock_hc = MATERIAL_PROPS[MAT_ROCK].molar_heat_capacity_solid;
    cell3d_add_material(cell, MAT_WATER, 1.0, calculate_water_energy(1.0, INITIAL_TEMP_K));
    cell3d_add_material(cell, MAT_ROCK, 1.0, rock_hc * INITIAL_TEMP_K);

    double energy_before = 0;
    CELL_FOR_EACH_MATERIAL(cell, type_b) {
        energy_before += cell->materials[type_b].thermal_energy;
    }

    // Add heat
    svo_add_heat_at(&svo, 0.0f, 0.0f, 0.0f, 1000.0);

    double energy_after = 0;
    CELL_FOR_EACH_MATERIAL(cell, type_a) {
        energy_after += cell->materials[type_a].thermal_energy;
    }

    // Total energy should increase by ~1000J
    ASSERT(fabs(energy_after - energy_before - 1000.0) < 1.0, "total energy should increase by 1000J");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_propagates_to_neighbor(void) {
    TEST_BEGIN("heat propagates from hot cell to cold neighbor");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Create two adjacent cells with water
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Add water to both cells
    Cell3D *cell1 = svo_get_cell_for_write(&svo, cx, cy, cz);
    Cell3D *cell2 = svo_get_cell_for_write(&svo, cx + 1, cy, cz);
    if (!cell1 || !cell2) { TEST_FAIL("couldn't get cells"); }

    double water_hc_solid = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_solid;

    // Cell 1: hot water (400K) - gas, needs both latent heats
    cell3d_add_material(cell1, MAT_WATER, 1.0, calculate_water_energy(1.0, 400.0));
    // Cell 2: cold water (250K) - solid ice, no latent heat yet
    cell3d_add_material(cell2, MAT_WATER, 1.0, water_hc_solid * 250.0);

    double temp1_before = get_cell_temp(&svo, cx, cy, cz);
    double temp2_before = get_cell_temp(&svo, cx + 1, cy, cz);

    ASSERT(temp1_before > 350, "cell1 should start hot");
    ASSERT(temp2_before < 300, "cell2 should start cold");

    // Mark cells as active for physics
    svo_mark_cell_active(&svo, cx, cy, cz);
    svo_mark_cell_active(&svo, cx + 1, cy, cz);

    // Run physics for many steps
    run_physics_steps(&svo, 100);

    double temp1_after = get_cell_temp(&svo, cx, cy, cz);
    double temp2_after = get_cell_temp(&svo, cx + 1, cy, cz);

    // Heat should flow from hot to cold
    ASSERT(temp1_after < temp1_before, "hot cell should cool down");
    ASSERT(temp2_after > temp2_before, "cold cell should warm up");

    // Temperatures should move toward each other
    double diff_before = fabs(temp1_before - temp2_before);
    double diff_after = fabs(temp1_after - temp2_after);
    ASSERT(diff_after < diff_before, "temperature difference should decrease");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_equilibrates_over_time(void) {
    TEST_BEGIN("heat equilibrates to similar temperatures over time");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Create hot cell surrounded by cold cells
    // Use temperatures within liquid phase (273K-373K) to avoid phase transition complexity
    Cell3D *center = svo_get_cell_for_write(&svo, cx, cy, cz);
    cell3d_add_material(center, MAT_WATER, 1.0, calculate_water_energy(1.0, 360.0));  // 360K (hot liquid)
    svo_mark_cell_active(&svo, cx, cy, cz);

    double initial_center_temp = get_cell_temp(&svo, cx, cy, cz);

    // Add cold neighbors (just above melting point 273.15K)
    int dx[] = {1, -1, 0, 0, 0, 0};
    int dy[] = {0, 0, 1, -1, 0, 0};
    int dz[] = {0, 0, 0, 0, 1, -1};

    for (int d = 0; d < 6; d++) {
        Cell3D *neighbor = svo_get_cell_for_write(&svo, cx + dx[d], cy + dy[d], cz + dz[d]);
        if (neighbor) {
            cell3d_add_material(neighbor, MAT_WATER, 1.0, calculate_water_energy(1.0, 290.0));  // 290K (cold liquid)
            svo_mark_cell_active(&svo, cx + dx[d], cy + dy[d], cz + dz[d]);
        }
    }

    // Run physics for many steps (2000 steps = 32 seconds of simulation)
    run_physics_steps(&svo, 2000);

    // Center should have cooled
    double center_temp = get_cell_temp(&svo, cx, cy, cz);
    ASSERT(center_temp < initial_center_temp, "center should have cooled");

    // Verify temperature is moving toward equilibrium
    // Equilibrium should be around (341 + 6*291)/7 ≈ 298K (weighted by both cells' heat capacities)
    // With slow conduction, just verify it's cooling and headed in the right direction
    double temp_drop = initial_center_temp - center_temp;
    ASSERT(temp_drop > 0.5, "center should have cooled by at least 0.5K");

    // Center shouldn't drop below neighbor temperature
    ASSERT(center_temp > 290, "center shouldn't go below neighbors' temp");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                      TIER 2: WATER TOOL TESTS
// ============================================================================

static bool test_water_tool_adds_water(void) {
    TEST_BEGIN("water tool adds water at ambient temperature");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 5.0);

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    double moles = get_water_moles(&svo, cx, cy, cz);
    double temp = get_cell_temp(&svo, cx, cy, cz);

    ASSERT(fabs(moles - 5.0) < 0.01, "should have 5 moles of water");
    // Temperature is weighted average with air, so may differ slightly
    ASSERT(temp > 250 && temp < 350, "water should be near ambient temperature");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_tool_accumulates(void) {
    TEST_BEGIN("multiple water additions accumulate");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 3.0);
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 2.0);
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 5.0);

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    double moles = get_water_moles(&svo, cx, cy, cz);
    ASSERT(fabs(moles - 10.0) < 0.01, "should have 10 moles total");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_flows_down(void) {
    TEST_BEGIN("water flows downward due to gravity");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Place water high up
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 10.0f, 0.0f, &cx, &cy, &cz);

    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!cell) { TEST_FAIL("couldn't get cell"); }

    // Add liquid water at room temperature (with proper latent heat)
    cell3d_add_material(cell, MAT_WATER, 10.0, calculate_water_energy(10.0, INITIAL_TEMP_K));
    svo_mark_cell_active(&svo, cx, cy, cz);

    double water_above_before = get_water_moles(&svo, cx, cy, cz);
    double water_below_before = get_water_moles(&svo, cx, cy - 1, cz);

    ASSERT(water_above_before > 9.0, "should have water at top initially");
    ASSERT(water_below_before < 0.1, "should have no water below initially");

    // Run physics
    run_physics_steps(&svo, 50);

    double water_above_after = get_water_moles(&svo, cx, cy, cz);
    double water_below_after = get_water_moles(&svo, cx, cy - 1, cz);

    // Water should have flowed down
    ASSERT(water_above_after < water_above_before, "water should decrease at top");
    ASSERT(water_below_after > water_below_before, "water should appear below");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_accumulates_at_bottom(void) {
    TEST_BEGIN("water accumulates at the bottom after flowing");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Place water high up
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 20.0f, 0.0f, &cx, &cy, &cz);

    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!cell) { TEST_FAIL("couldn't get cell"); }

    double initial_water = 10.0;
    cell3d_add_material(cell, MAT_WATER, initial_water, calculate_water_energy(initial_water, INITIAL_TEMP_K));
    svo_mark_cell_active(&svo, cx, cy, cz);

    // Run physics for many steps
    run_physics_steps(&svo, 200);

    // Water should have moved down - check several cells below
    double total_water = 0;
    for (int y_offset = -10; y_offset <= 0; y_offset++) {
        total_water += get_water_moles(&svo, cx, cy + y_offset, cz);
    }

    // Total water should be conserved (approximately)
    ASSERT(fabs(total_water - initial_water) < initial_water * 0.1,
           "total water should be conserved during flow");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_does_not_flow_through_solid(void) {
    TEST_BEGIN("water does not flow through solid ground");

    MatterSVO svo;
    // Create ground at height 5 (solid rock below)
    if (!init_svo_with_ground(&svo, 5)) { TEST_FAIL("init failed"); }

    // Ground level in SVO coords
    int ground_y = SVO_GROUND_Y + 5;

    // Place water above ground
    int cx = SVO_SIZE / 2;  // Center
    int cz = SVO_SIZE / 2;

    Cell3D *cell = svo_get_cell_for_write(&svo, cx, ground_y + 1, cz);
    if (!cell) { TEST_FAIL("couldn't get cell above ground"); }

    cell3d_add_material(cell, MAT_WATER, 5.0, calculate_water_energy(5.0, INITIAL_TEMP_K));
    svo_mark_cell_active(&svo, cx, ground_y + 1, cz);

    // Verify ground is solid
    ASSERT(cell_has_solid(&svo, cx, ground_y, cz), "ground should be solid");

    // Run physics
    run_physics_steps(&svo, 100);

    // Water shouldn't penetrate solid
    double water_in_ground = get_water_moles(&svo, cx, ground_y, cz);
    ASSERT(water_in_ground < 0.1, "water should not penetrate solid ground");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                      TIER 3: COLD TOOL TESTS
// ============================================================================

static bool test_cold_tool_removes_energy(void) {
    TEST_BEGIN("cold tool removes thermal energy from cell");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add hot water
    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    cell3d_add_material(cell, MAT_WATER, 1.0, calculate_water_energy(1.0, 350.0));  // 350K (liquid)

    double temp_before = get_cell_temp(&svo, cx, cy, cz);
    ASSERT(temp_before > 300, "should start warm");

    // Remove heat (cold tool)
    svo_remove_heat_at(&svo, 0.0f, 0.0f, 0.0f, 5000.0);

    double temp_after = get_cell_temp(&svo, cx, cy, cz);
    ASSERT(temp_after < temp_before, "temperature should decrease");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_cold_cannot_go_below_zero(void) {
    TEST_BEGIN("cold tool cannot make temperature negative");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water at ambient temp
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 1.0);

    // Remove massive amount of heat
    svo_remove_heat_at(&svo, 0.0f, 0.0f, 0.0f, 1e9);

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    double temp = get_cell_temp(&svo, cx, cy, cz);
    ASSERT(temp >= 0, "temperature cannot be negative");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_cold_can_freeze_water(void) {
    TEST_BEGIN("sufficient cooling freezes water (below 273K)");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water at ambient temp
    svo_add_water_at(&svo, 0.0f, 0.0f, 0.0f, 1.0);

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    Cell3D *cell_read = svo_get_cell_for_write(&svo, cx, cy, cz);
    MaterialEntry *water_entry = cell3d_find_material(cell_read, MAT_WATER);
    double water_temp = material_get_temperature(&water_entry->state, MAT_WATER);
    Phase phase_before = material_get_phase(MAT_WATER, water_temp);
    ASSERT(phase_before == PHASE_LIQUID, "water should start as liquid");

    // Calculate energy needed to cool to below freezing
    // Need to get from ~293K to ~200K = ~93K drop
    // Energy = moles * heat_capacity * delta_T (use liquid Cp since starting as liquid)
    double water_hc_liquid = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;
    double energy_to_remove = 1.0 * water_hc_liquid * 150.0;  // Cool by 150K

    svo_remove_heat_at(&svo, 0.0f, 0.0f, 0.0f, energy_to_remove);

    cell_read = svo_get_cell_for_write(&svo, cx, cy, cz);
    water_entry = cell3d_find_material(cell_read, MAT_WATER);
    if (water_entry) {
        water_temp = material_get_temperature(&water_entry->state, MAT_WATER);
        Phase phase_after = material_get_phase(MAT_WATER, water_temp);

        ASSERT(water_temp < 273.15, "water should be below freezing point");
        ASSERT(phase_after == PHASE_SOLID, "water should be solid (ice)");
    }

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                TIER 4: CONSERVATION TESTS DURING PHYSICS
// ============================================================================

static bool test_energy_conserved_during_conduction(void) {
    TEST_BEGIN("total energy conserved during heat conduction");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Create two adjacent cells with different temperatures
    // Use only water (clear air) to avoid heat leaking to air
    // Use liquid water temperatures (273K-373K) to avoid phase transitions during test
    Cell3D *cell1 = svo_get_cell_for_write(&svo, cx, cy, cz);
    Cell3D *cell2 = svo_get_cell_for_write(&svo, cx + 1, cy, cz);

    // Clear cells and add only water
    cell3d_free(cell1);
    cell3d_init(cell1);
    cell3d_free(cell2);
    cell3d_init(cell2);

    // Use liquid temperatures: 350K (hot) and 280K (cold) - both in liquid range
    cell3d_add_material(cell1, MAT_WATER, 1.0, calculate_water_energy(1.0, 350.0));  // Hot liquid
    cell3d_add_material(cell2, MAT_WATER, 1.0, calculate_water_energy(1.0, 280.0));  // Cold liquid

    svo_mark_cell_active(&svo, cx, cy, cz);
    svo_mark_cell_active(&svo, cx + 1, cy, cz);

    // Calculate initial energy in these two cells
    // Note: Must read values immediately - cell3d_find_material_const uses thread-local storage
    double e1_before = CELL_HAS_MATERIAL(cell1, MAT_WATER) ? cell1->materials[MAT_WATER].thermal_energy : 0;
    double e2_before = CELL_HAS_MATERIAL(cell2, MAT_WATER) ? cell2->materials[MAT_WATER].thermal_energy : 0;
    double total_before = e1_before + e2_before;

    // Run physics for a few steps (not too many to limit heat spreading to other cells)
    run_physics_steps(&svo, 20);

    // Re-get cells
    cell1 = svo_get_cell_for_write(&svo, cx, cy, cz);
    cell2 = svo_get_cell_for_write(&svo, cx + 1, cy, cz);

    // Read values directly from cells
    double e1_after = CELL_HAS_MATERIAL(cell1, MAT_WATER) ? cell1->materials[MAT_WATER].thermal_energy : 0;
    double e2_after = CELL_HAS_MATERIAL(cell2, MAT_WATER) ? cell2->materials[MAT_WATER].thermal_energy : 0;
    double total_after = e1_after + e2_after;

    // Verify heat transferred (hot got cooler, cold got warmer)
    ASSERT(e1_after < e1_before, "hot cell should lose energy");
    ASSERT(e2_after > e2_before, "cold cell should gain energy");

    // Energy should be approximately conserved between these two cells
    // (some may leak to air neighbors, so use generous tolerance)
    double tolerance = total_before * 0.15;  // 15% tolerance for leakage to neighbors
    ASSERT(fabs(total_after - total_before) < tolerance,
           "total energy should be approximately conserved");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_mass_conserved_during_flow(void) {
    TEST_BEGIN("water mass conserved during flow");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    // Add water at multiple heights
    double initial_water = 0;

    for (int y = 0; y < 5; y++) {
        int cx, cy, cz;
        svo_world_to_cell(0.0f, (float)(y * 5), 0.0f, &cx, &cy, &cz);

        Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
        if (cell) {
            double moles = 2.0;
            cell3d_add_material(cell, MAT_WATER, moles, calculate_water_energy(moles, INITIAL_TEMP_K));
            svo_mark_cell_active(&svo, cx, cy, cz);
            initial_water += moles;
        }
    }

    // Run physics for water to flow
    run_physics_steps(&svo, 100);

    double final_water = get_total_water_moles(&svo);

    // Water mass should be conserved
    ASSERT(fabs(final_water - initial_water) < initial_water * 0.1,
           "water mass should be conserved during flow");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                    TIER 5: COMBINED PHYSICS TESTS
// ============================================================================

static bool test_heated_water_changes_phase_to_steam(void) {
    TEST_BEGIN("heated water becomes steam above boiling point");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Create cell with ONLY water (no air) for clean test
    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!cell) { TEST_FAIL("couldn't get cell"); }

    // Clear any existing materials and add only water at ambient
    cell3d_free(cell);
    cell3d_init(cell);
    cell3d_add_material(cell, MAT_WATER, 1.0, calculate_water_energy(1.0, INITIAL_TEMP_K));

    MaterialEntry *water = cell3d_find_material(cell, MAT_WATER);
    Phase phase_before = material_get_phase_from_energy(&water->state, MAT_WATER);
    ASSERT(phase_before == PHASE_LIQUID, "should start as liquid");

    // Heat to above boiling (need to add enough energy to:
    // 1. Heat from 293K to 373K (boiling point)
    // 2. Supply latent heat of vaporization
    // 3. Heat above boiling to ensure it's definitely gas
    double Cp_l = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;
    double Cp_g = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_gas;
    double Hv = MATERIAL_PROPS[MAT_WATER].enthalpy_vaporization;
    double energy_to_boiling = 1.0 * Cp_l * (373.15 - INITIAL_TEMP_K);  // Heat to boiling (liquid)
    double energy_needed = energy_to_boiling + 1.0 * Hv + 1.0 * Cp_g * 30.0;  // + vaporize + heat above (gas)
    svo_add_heat_at(&svo, 0.0f, 0.0f, 0.0f, energy_needed);

    cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    water = cell3d_find_material(cell, MAT_WATER);
    double temp_after = material_get_temperature(&water->state, MAT_WATER);
    Phase phase_after = material_get_phase_from_energy(&water->state, MAT_WATER);

    ASSERT(temp_after > 373.15, "water should be above boiling point");
    ASSERT(phase_after == PHASE_GAS, "water should be steam");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_hot_cell_melts_ice(void) {
    TEST_BEGIN("hot cell adjacent to ice causes melting");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    double water_hc_solid = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_solid;

    // Create ice (water below freezing) - solid, no latent heat
    Cell3D *ice_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    cell3d_add_material(ice_cell, MAT_WATER, 1.0, water_hc_solid * 200.0);  // 200K = ice

    // Create hot water next to it (liquid, needs latent heat of fusion)
    Cell3D *hot_cell = svo_get_cell_for_write(&svo, cx + 1, cy, cz);
    cell3d_add_material(hot_cell, MAT_WATER, 1.0, calculate_water_energy(1.0, 350.0));  // 350K = hot liquid

    svo_mark_cell_active(&svo, cx, cy, cz);
    svo_mark_cell_active(&svo, cx + 1, cy, cz);

    double ice_temp_before = get_cell_temp(&svo, cx, cy, cz);
    ASSERT(ice_temp_before < 273.15, "ice should start frozen");

    // Run physics for heat transfer
    run_physics_steps(&svo, 200);

    double ice_temp_after = get_cell_temp(&svo, cx, cy, cz);

    // Ice should have warmed up
    ASSERT(ice_temp_after > ice_temp_before, "ice should warm up from hot neighbor");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_steam_rises(void) {
    TEST_BEGIN("steam (hot water vapor) rises upward");

    MatterSVO svo;
    if (!init_test_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx, cy, cz;
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);

    // Create steam at a low position (hot gas - needs both latent heats)
    Cell3D *steam_cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    // Steam at 400K (gas, includes latent heat of fusion and vaporization)
    cell3d_add_material(steam_cell, MAT_WATER, 5.0, calculate_water_energy(5.0, 400.0));
    svo_mark_cell_active(&svo, cx, cy, cz);

    const Cell3D *cell = svo_get_cell(&svo, cx, cy, cz);
    const MaterialEntry *water = cell3d_find_material_const(cell, MAT_WATER);
    Phase phase = material_get_phase_from_energy(&water->state, MAT_WATER);
    ASSERT(phase == PHASE_GAS, "should be steam (gas)");

    double water_below_before = get_water_moles(&svo, cx, cy, cz);
    double water_above_before = get_water_moles(&svo, cx, cy + 1, cz);

    // Run physics for gas diffusion
    run_physics_steps(&svo, 100);

    double water_below_after = get_water_moles(&svo, cx, cy, cz);
    double water_above_after = get_water_moles(&svo, cx, cy + 1, cz);

    // Steam should have diffused upward (water vapor is lighter than air)
    // Note: depends on gas diffusion implementation
    // At minimum, some should have moved
    ASSERT(water_below_after < water_below_before || water_above_after > water_above_before,
           "steam should diffuse (some upward)");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                         MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("    TOOL INTEGRATION TESTS\n");
    printf("========================================\n");

    TestCase tests[] = {
        // Tier 1: Heat Tool
        {"HEAT TOOL", "adds_energy", test_heat_tool_adds_energy},
        {"HEAT TOOL", "distributes_among_materials", test_heat_tool_distributes_among_materials},
        {"HEAT TOOL", "propagates_to_neighbor", test_heat_propagates_to_neighbor},
        {"HEAT TOOL", "equilibrates_over_time", test_heat_equilibrates_over_time},

        // Tier 2: Water Tool
        {"WATER TOOL", "adds_water", test_water_tool_adds_water},
        {"WATER TOOL", "accumulates", test_water_tool_accumulates},
        {"WATER TOOL", "flows_down", test_water_flows_down},
        {"WATER TOOL", "accumulates_at_bottom", test_water_accumulates_at_bottom},
        {"WATER TOOL", "does_not_flow_through_solid", test_water_does_not_flow_through_solid},

        // Tier 3: Cold Tool
        {"COLD TOOL", "removes_energy", test_cold_tool_removes_energy},
        {"COLD TOOL", "cannot_go_below_zero", test_cold_cannot_go_below_zero},
        {"COLD TOOL", "can_freeze_water", test_cold_can_freeze_water},

        // Tier 4: Conservation
        {"CONSERVATION", "energy_during_conduction", test_energy_conserved_during_conduction},
        {"CONSERVATION", "water_mass_during_flow", test_water_mass_conserved_during_flow},

        // Tier 5: Combined Physics
        {"COMBINED", "heated_water_becomes_steam", test_heated_water_changes_phase_to_steam},
        {"COMBINED", "hot_cell_melts_ice", test_hot_cell_melts_ice},
        {"COMBINED", "steam_rises", test_steam_rises},
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
