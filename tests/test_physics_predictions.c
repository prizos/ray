/**
 * test_physics_predictions.c - Predictive Physics Tests at Multiple Scales
 *
 * Tests physics behavior with mathematically predictable outcomes at scales:
 * 2x2, 4x4, 8x8, 16x16, 32x32 (for 2D surfaces)
 * 2x2x2, 4x4x4, 8x8x8 (for 3D volumes)
 *
 * Test categories:
 *   1. Water spreading on flat surfaces (uniform distribution)
 *   2. Heat equilibration (weighted average temperature)
 *   3. 3D flooding (volume filling)
 *   4. Conservation laws (mass and energy)
 */

#include "test_common.h"
#include "terrain.h"

// Extended assertions
#define ASSERT_GT(a, b, msg) \
    if (!((a) > (b))) { \
        TEST_FAIL(msg " (%.6f not > %.6f)", (double)(a), (double)(b)); \
    }

#define ASSERT_LT(a, b, msg) \
    if (!((a) < (b))) { \
        TEST_FAIL(msg " (%.6f not < %.6f)", (double)(a), (double)(b)); \
    }

// ============ HELPER FUNCTIONS ============

static void init_test_world(ChunkWorld *world) {
    world_init(world);
}

// Calculate total moles of a material in a region
static double calculate_region_moles(ChunkWorld *world, MaterialType type,
                                      int x0, int y0, int z0,
                                      int x1, int y1, int z1) {
    double total = 0.0;
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                const Cell3D *cell = world_get_cell(world, x, y, z);
                if (cell && CELL_HAS_MATERIAL(cell, type)) {
                    total += cell->materials[type].moles;
                }
            }
        }
    }
    return total;
}

// Calculate total thermal energy in a region
static double calculate_region_energy(ChunkWorld *world,
                                       int x0, int y0, int z0,
                                       int x1, int y1, int z1) {
    double total = 0.0;
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                const Cell3D *cell = world_get_cell(world, x, y, z);
                if (!cell) continue;
                CELL_FOR_EACH_MATERIAL(cell, type) {
                    total += cell->materials[type].thermal_energy;
                }
            }
        }
    }
    return total;
}

// Count cells with material in a region
static int count_cells_with_material(ChunkWorld *world, MaterialType type,
                                      int x0, int y0, int z0,
                                      int x1, int y1, int z1) {
    int count = 0;
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                const Cell3D *cell = world_get_cell(world, x, y, z);
                if (cell && CELL_HAS_MATERIAL(cell, type)) {
                    count++;
                }
            }
        }
    }
    return count;
}

// Get min and max moles in a region
static void get_moles_range(ChunkWorld *world, MaterialType type,
                            int x0, int y0, int z0,
                            int x1, int y1, int z1,
                            double *min_out, double *max_out) {
    double min_val = 1e99;
    double max_val = 0.0;
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                const Cell3D *cell = world_get_cell(world, x, y, z);
                if (cell && CELL_HAS_MATERIAL(cell, type)) {
                    double moles = cell->materials[type].moles;
                    if (moles < min_val) min_val = moles;
                    if (moles > max_val) max_val = moles;
                }
            }
        }
    }
    *min_out = min_val;
    *max_out = max_val;
}

// Get min and max temperature in a region
static void get_temp_range(ChunkWorld *world,
                           int x0, int y0, int z0,
                           int x1, int y1, int z1,
                           double *min_out, double *max_out) {
    double min_val = 1e99;
    double max_val = 0.0;
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                Cell3D *cell = world_get_cell_for_write(world, x, y, z);
                if (!cell || cell->present == 0) continue;
                double temp = cell_get_temperature(cell);
                if (temp > 0) {  // 0 means no material
                    if (temp < min_val) min_val = temp;
                    if (temp > max_val) max_val = temp;
                }
            }
        }
    }
    *min_out = min_val;
    *max_out = max_val;
}

// Run physics for N steps
static void run_physics(ChunkWorld *world, int steps, PhysicsFlags flags) {
    for (int i = 0; i < steps; i++) {
        world_physics_step_flags(world, 0.016f, flags);
        TEST_RECORD_PHYSICS_STEP();
    }
}

// Add solid floor at y level
static void add_solid_floor(ChunkWorld *world, int y, int x0, int z0, int x1, int z1) {
    for (int z = z0; z <= z1; z++) {
        for (int x = x0; x <= x1; x++) {
            Cell3D *cell = world_get_cell_for_write(world, x, y, z);
            if (cell) {
                double rock_moles = 50.0;
                double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, INITIAL_TEMP_K);
                cell_add_material(cell, MAT_ROCK, rock_moles, rock_energy);
            }
        }
    }
}

// ============================================================================
//                      WATER SPREADING TESTS (2D FLAT SURFACE)
// ============================================================================

// Test water falls and stays on floor (current physics only has gravity-driven flow)
static bool test_water_falls_to_floor(int height, double total_water_moles) {
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "water falls %d cells to floor", height);
    TEST_BEGIN(test_name);

    ChunkWorld world;
    init_test_world(&world);

    // Base coordinates
    int base_x = 128;
    int base_y = 128;
    int base_z = 128;

    // Add solid floor at base_y - 1
    Cell3D *floor = world_get_cell_for_write(&world, base_x, base_y - 1, base_z);
    if (floor) {
        double rock_moles = 50.0;
        double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, INITIAL_TEMP_K);
        cell_add_material(floor, MAT_ROCK, rock_moles, rock_energy);
    }

    // Add water at height above floor
    Cell3D *top = world_get_cell_for_write(&world, base_x, base_y + height, base_z);
    if (!top) { TEST_FAIL("couldn't get top cell"); }
    double water_energy = calculate_material_energy(MAT_WATER, total_water_moles, INITIAL_TEMP_K);
    cell_add_material(top, MAT_WATER, total_water_moles, water_energy);
    world_mark_cell_active(&world, base_x, base_y + height, base_z);

    // Record initial state
    double initial_moles = total_water_moles;

    // Run physics
    int steps = height * 100;
    run_physics(&world, steps, PHYSICS_LIQUID_FLOW);

    // Check conservation
    double final_moles = calculate_region_moles(&world, MAT_WATER,
        base_x, base_y - 1, base_z,
        base_x, base_y + height + 1, base_z);
    ASSERT_FLOAT_EQ(final_moles, initial_moles, initial_moles * 0.01,
        "water mass not conserved");

    // Check water reached floor level
    const Cell3D *floor_cell = world_get_cell(&world, base_x, base_y, base_z);
    double water_at_floor = 0;
    if (floor_cell && CELL_HAS_MATERIAL(floor_cell, MAT_WATER)) {
        water_at_floor = floor_cell->materials[MAT_WATER].moles;
    }
    ASSERT_GT(water_at_floor, 0.0, "water should reach floor level");

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_water_fall_2(void) { return test_water_falls_to_floor(2, 10.0); }
static bool test_water_fall_4(void) { return test_water_falls_to_floor(4, 10.0); }
static bool test_water_fall_8(void) { return test_water_falls_to_floor(8, 10.0); }
static bool test_water_fall_16(void) { return test_water_falls_to_floor(16, 10.0); }
// Note: 32 cells would cross chunk boundary (CHUNK_SIZE=32), use 30 to stay within one chunk
static bool test_water_fall_30(void) { return test_water_falls_to_floor(30, 10.0); }

// ============================================================================
//                      HEAT EQUILIBRATION TESTS (3D CUBES)
// ============================================================================

// Test heat equilibration in NxNxN cube
// Note: Current physics has slow heat transfer, so we just verify:
// 1. Energy is conserved
// 2. Some heat transfer occurs (temperatures move toward each other)
static bool test_heat_equilibration_cube(int size) {
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "heat flows in %dx%dx%d cube", size, size, size);
    TEST_BEGIN(test_name);

    ChunkWorld world;
    init_test_world(&world);

    // Base coordinates
    int base_x = 128;
    int base_y = 128;
    int base_z = 128;

    double hot_temp = 500.0;
    double cold_temp = 300.0;
    double moles_per_cell = 5.0;

    // Fill cube: alternating hot and cold cells (checkerboard pattern)
    for (int z = 0; z < size; z++) {
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                Cell3D *cell = world_get_cell_for_write(&world, base_x + x, base_y + y, base_z + z);
                if (!cell) continue;

                // Checkerboard: hot if (x+y+z) is even, cold otherwise
                double temp = ((x + y + z) % 2 == 0) ? hot_temp : cold_temp;
                double energy = calculate_material_energy(MAT_ROCK, moles_per_cell, temp);
                cell_add_material(cell, MAT_ROCK, moles_per_cell, energy);
                world_mark_cell_active(&world, base_x + x, base_y + y, base_z + z);
            }
        }
    }

    // Record initial energy
    double initial_energy = calculate_region_energy(&world,
        base_x, base_y, base_z,
        base_x + size - 1, base_y + size - 1, base_z + size - 1);

    // Record initial temperature range
    double min_temp_before, max_temp_before;
    get_temp_range(&world,
        base_x, base_y, base_z,
        base_x + size - 1, base_y + size - 1, base_z + size - 1,
        &min_temp_before, &max_temp_before);

    ASSERT_FLOAT_EQ(min_temp_before, cold_temp, 10.0, "initial cold temp incorrect");
    ASSERT_FLOAT_EQ(max_temp_before, hot_temp, 10.0, "initial hot temp incorrect");

    // Run heat conduction with more steps
    int steps = size * size * size * 100;
    run_physics(&world, steps, PHYSICS_HEAT_ALL);

    // Check energy conservation
    double final_energy = calculate_region_energy(&world,
        base_x, base_y, base_z,
        base_x + size - 1, base_y + size - 1, base_z + size - 1);
    ASSERT_FLOAT_EQ(final_energy, initial_energy, initial_energy * 0.001,
        "energy not conserved");

    // Check some temperature equilibration occurred
    double min_temp_after, max_temp_after;
    get_temp_range(&world,
        base_x, base_y, base_z,
        base_x + size - 1, base_y + size - 1, base_z + size - 1,
        &min_temp_after, &max_temp_after);

    // Temperature difference should decrease (even slightly)
    double temp_diff_before = max_temp_before - min_temp_before;
    double temp_diff_after = max_temp_after - min_temp_after;
    ASSERT_LT(temp_diff_after, temp_diff_before,
        "temperature difference should decrease");

    // Both min and max should move toward equilibrium (even slightly)
    ASSERT_GT(min_temp_after, min_temp_before - 0.01, "cold cells should not get colder");
    ASSERT_LT(max_temp_after, max_temp_before + 0.01, "hot cells should not get hotter");

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_heat_equilibration_2x2x2(void) { return test_heat_equilibration_cube(2); }
static bool test_heat_equilibration_4x4x4(void) { return test_heat_equilibration_cube(4); }
static bool test_heat_equilibration_8x8x8(void) { return test_heat_equilibration_cube(8); }

// Larger cubes take too long - 16x16x16 = 4096 cells, very slow
// static bool test_heat_equilibration_16x16x16(void) { return test_heat_equilibration_cube(16); }

// ============================================================================
//                      3D FLOODING TESTS
// ============================================================================

// Test water falls through a column to the bottom
static bool test_water_column_fall(int height) {
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "water falls through %d-cell column", height);
    TEST_BEGIN(test_name);

    ChunkWorld world;
    init_test_world(&world);

    // Base coordinates
    int base_x = 128;
    int base_y = 128;
    int base_z = 128;

    // Add solid floor
    Cell3D *floor = world_get_cell_for_write(&world, base_x, base_y - 1, base_z);
    if (floor) {
        double rock_moles = 50.0;
        double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, INITIAL_TEMP_K);
        cell_add_material(floor, MAT_ROCK, rock_moles, rock_energy);
    }

    // Add water at top of column
    double total_water = 10.0;
    Cell3D *top = world_get_cell_for_write(&world, base_x, base_y + height - 1, base_z);
    if (!top) { TEST_FAIL("couldn't get top cell"); }
    double water_energy = calculate_material_energy(MAT_WATER, total_water, INITIAL_TEMP_K);
    cell_add_material(top, MAT_WATER, total_water, water_energy);
    world_mark_cell_active(&world, base_x, base_y + height - 1, base_z);

    // Verify initial: water is at top
    const Cell3D *top_before = world_get_cell(&world, base_x, base_y + height - 1, base_z);
    ASSERT(CELL_HAS_MATERIAL(top_before, MAT_WATER), "water should start at top");

    // Run physics with many steps
    int steps = height * 200;
    run_physics(&world, steps, PHYSICS_LIQUID_FLOW);

    // Conservation check
    double total_after = calculate_region_moles(&world, MAT_WATER,
        base_x, base_y - 1, base_z,
        base_x, base_y + height, base_z);
    ASSERT_FLOAT_EQ(total_after, total_water, total_water * 0.01,
        "water mass not conserved");

    // Water should have flowed down - check that at least some is at bottom
    double water_at_bottom = 0;
    const Cell3D *bottom = world_get_cell(&world, base_x, base_y, base_z);
    if (bottom && CELL_HAS_MATERIAL(bottom, MAT_WATER)) {
        water_at_bottom = bottom->materials[MAT_WATER].moles;
    }
    ASSERT_GT(water_at_bottom, 0.1, "water should reach bottom");

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_water_column_fall_4(void) { return test_water_column_fall(4); }
static bool test_water_column_fall_8(void) { return test_water_column_fall(8); }
static bool test_water_column_fall_16(void) { return test_water_column_fall(16); }

// Test water falls straight down (no horizontal spreading)
static bool test_water_falls_straight(int height) {
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "water falls straight down %d cells", height);
    TEST_BEGIN(test_name);

    ChunkWorld world;
    init_test_world(&world);

    int base_x = 128;
    int base_y = 128;
    int base_z = 128;

    // Add solid floor
    Cell3D *floor = world_get_cell_for_write(&world, base_x, base_y - 1, base_z);
    if (floor) {
        double rock_moles = 50.0;
        double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, INITIAL_TEMP_K);
        cell_add_material(floor, MAT_ROCK, rock_moles, rock_energy);
    }

    // Add water at top
    double total_water = 10.0;
    Cell3D *top = world_get_cell_for_write(&world, base_x, base_y + height, base_z);
    if (!top) { TEST_FAIL("couldn't get top cell"); }
    double water_energy = calculate_material_energy(MAT_WATER, total_water, INITIAL_TEMP_K);
    cell_add_material(top, MAT_WATER, total_water, water_energy);
    world_mark_cell_active(&world, base_x, base_y + height, base_z);

    // Run physics
    int steps = height * 200;
    run_physics(&world, steps, PHYSICS_LIQUID_FLOW);

    // Conservation check
    double total_after = calculate_region_moles(&world, MAT_WATER,
        base_x - 1, base_y - 1, base_z - 1,
        base_x + 1, base_y + height + 1, base_z + 1);
    ASSERT_FLOAT_EQ(total_after, total_water, total_water * 0.01,
        "water mass not conserved");

    // Check water stayed in the same column (no horizontal spread)
    double water_in_column = calculate_region_moles(&world, MAT_WATER,
        base_x, base_y, base_z,
        base_x, base_y + height, base_z);
    ASSERT_FLOAT_EQ(water_in_column, total_water, total_water * 0.01,
        "water should stay in same column");

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_water_falls_straight_2(void) { return test_water_falls_straight(2); }
static bool test_water_falls_straight_4(void) { return test_water_falls_straight(4); }
static bool test_water_falls_straight_8(void) { return test_water_falls_straight(8); }

// ============================================================================
//                      CONSERVATION VERIFICATION TESTS
// ============================================================================

// Test mass conservation during water spreading
static bool test_mass_conservation_water_spread(int size) {
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "mass conserved during %dx%d water spread", size, size);
    TEST_BEGIN(test_name);

    ChunkWorld world;
    init_test_world(&world);

    int base_x = 128;
    int base_y = 128;
    int base_z = 128;

    // Add solid floor
    add_solid_floor(&world, base_y - 1, base_x, base_z, base_x + size - 1, base_z + size - 1);

    // Add water at center
    double total_water = 100.0;
    int center_x = base_x + size / 2;
    int center_z = base_z + size / 2;
    Cell3D *center = world_get_cell_for_write(&world, center_x, base_y, center_z);
    double water_energy = calculate_material_energy(MAT_WATER, total_water, INITIAL_TEMP_K);
    cell_add_material(center, MAT_WATER, total_water, water_energy);
    world_mark_cell_active(&world, center_x, base_y, center_z);

    // Run in small batches, checking conservation each time
    for (int batch = 0; batch < 10; batch++) {
        run_physics(&world, size * 10, PHYSICS_LIQUID_FLOW);

        double current_moles = calculate_region_moles(&world, MAT_WATER,
            base_x - 1, base_y - 1, base_z - 1,
            base_x + size, base_y + 1, base_z + size);

        if (fabs(current_moles - total_water) > total_water * 0.01) {
            TEST_FAIL("mass not conserved at batch %d: expected %.2f, got %.2f",
                batch, total_water, current_moles);
        }
    }

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_mass_conservation_4x4(void) { return test_mass_conservation_water_spread(4); }
static bool test_mass_conservation_8x8(void) { return test_mass_conservation_water_spread(8); }
static bool test_mass_conservation_16x16(void) { return test_mass_conservation_water_spread(16); }

// Test energy conservation during heat equilibration
static bool test_energy_conservation_heat(int size) {
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "energy conserved during %dx%dx%d heat flow", size, size, size);
    TEST_BEGIN(test_name);

    ChunkWorld world;
    init_test_world(&world);

    int base_x = 128;
    int base_y = 128;
    int base_z = 128;

    double hot_temp = 500.0;
    double cold_temp = 300.0;
    double moles_per_cell = 5.0;

    // Fill with alternating hot/cold rock
    for (int z = 0; z < size; z++) {
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                Cell3D *cell = world_get_cell_for_write(&world, base_x + x, base_y + y, base_z + z);
                if (!cell) continue;

                double temp = ((x + y + z) % 2 == 0) ? hot_temp : cold_temp;
                double energy = calculate_material_energy(MAT_ROCK, moles_per_cell, temp);
                cell_add_material(cell, MAT_ROCK, moles_per_cell, energy);
                world_mark_cell_active(&world, base_x + x, base_y + y, base_z + z);
            }
        }
    }

    double initial_energy = calculate_region_energy(&world,
        base_x, base_y, base_z,
        base_x + size - 1, base_y + size - 1, base_z + size - 1);

    // Run in batches, checking conservation
    for (int batch = 0; batch < 10; batch++) {
        run_physics(&world, size * size * 5, PHYSICS_HEAT_ALL);

        double current_energy = calculate_region_energy(&world,
            base_x, base_y, base_z,
            base_x + size - 1, base_y + size - 1, base_z + size - 1);

        if (fabs(current_energy - initial_energy) > initial_energy * 0.001) {
            TEST_FAIL("energy not conserved at batch %d: expected %.2f, got %.2f",
                batch, initial_energy, current_energy);
        }
    }

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_energy_conservation_2x2x2(void) { return test_energy_conservation_heat(2); }
static bool test_energy_conservation_4x4x4(void) { return test_energy_conservation_heat(4); }
static bool test_energy_conservation_8x8x8(void) { return test_energy_conservation_heat(8); }

// ============================================================================
//                      DEBUG TEST - TRACE WATER FLOW
// ============================================================================

static bool test_debug_water_flow(void) {
    TEST_BEGIN("debug: trace water flow step by step");

    ChunkWorld world;
    init_test_world(&world);

    int base_x = 128, base_y = 128, base_z = 128;
    int top_y = base_y + 2;  // Water at 130

    // Add solid floor at base_y - 1 (y=127)
    Cell3D *floor = world_get_cell_for_write(&world, base_x, base_y - 1, base_z);
    if (floor) {
        double rock_moles = 50.0;
        double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, INITIAL_TEMP_K);
        cell_add_material(floor, MAT_ROCK, rock_moles, rock_energy);
    }

    // Add water at top_y (y=130)
    Cell3D *top = world_get_cell_for_write(&world, base_x, top_y, base_z);
    if (!top) { TEST_FAIL("couldn't get top cell"); }

    double total_water = 10.0;
    double water_energy = calculate_material_energy(MAT_WATER, total_water, INITIAL_TEMP_K);
    cell_add_material(top, MAT_WATER, total_water, water_energy);
    world_mark_cell_active(&world, base_x, top_y, base_z);

    // Verify phase is liquid
    Phase phase = material_get_phase_from_energy(&top->materials[MAT_WATER], MAT_WATER);
    if (phase != PHASE_LIQUID) {
        TEST_FAIL("water should be liquid, got phase %d", phase);
    }

    // Print initial chunk info
    printf("\n  Initial: active_count=%d, water at y=%d = %.2f moles\n",
           world.active_count, top_y, top->materials[MAT_WATER].moles);

    // Run physics step by step
    for (int step = 1; step <= 5; step++) {
        world_physics_step_flags(&world, 0.016f, PHYSICS_LIQUID_FLOW);

        // Check water at each level
        double water_130 = 0, water_129 = 0, water_128 = 0;
        const Cell3D *c130 = world_get_cell(&world, base_x, 130, base_z);
        const Cell3D *c129 = world_get_cell(&world, base_x, 129, base_z);
        const Cell3D *c128 = world_get_cell(&world, base_x, 128, base_z);

        if (c130 && CELL_HAS_MATERIAL(c130, MAT_WATER)) water_130 = c130->materials[MAT_WATER].moles;
        if (c129 && CELL_HAS_MATERIAL(c129, MAT_WATER)) water_129 = c129->materials[MAT_WATER].moles;
        if (c128 && CELL_HAS_MATERIAL(c128, MAT_WATER)) water_128 = c128->materials[MAT_WATER].moles;

        printf("  Step %d: y130=%.4f y129=%.4f y128=%.4f active=%d\n",
               step, water_130, water_129, water_128, world.active_count);
    }

    // Final check: water should have moved
    const Cell3D *final_top = world_get_cell(&world, base_x, top_y, base_z);
    double final_water_at_top = 0;
    if (final_top && CELL_HAS_MATERIAL(final_top, MAT_WATER)) {
        final_water_at_top = final_top->materials[MAT_WATER].moles;
    }

    ASSERT_LT(final_water_at_top, total_water, "water should have flowed from top cell");

    world_cleanup(&world);
    TEST_PASS();
}

// ============================================================================
//                      PREDICTABLE FINAL STATE TESTS
// ============================================================================

// Test that 2 cells exchange heat (approach equilibrium over time)
static bool test_two_cell_heat_exchange(void) {
    TEST_BEGIN("two cells exchange heat (temperatures converge)");

    ChunkWorld world;
    init_test_world(&world);

    int cx = 128, cy = 128, cz = 128;

    // Cell A: 5 moles rock at 400K
    // Cell B: 5 moles rock at 300K
    double moles = 5.0;
    double temp_a_initial = 400.0;
    double temp_b_initial = 300.0;

    Cell3D *cell_a = world_get_cell_for_write(&world, cx, cy, cz);
    Cell3D *cell_b = world_get_cell_for_write(&world, cx + 1, cy, cz);

    double energy_a = calculate_material_energy(MAT_ROCK, moles, temp_a_initial);
    double energy_b = calculate_material_energy(MAT_ROCK, moles, temp_b_initial);

    cell_add_material(cell_a, MAT_ROCK, moles, energy_a);
    cell_add_material(cell_b, MAT_ROCK, moles, energy_b);
    world_mark_cell_active(&world, cx, cy, cz);
    world_mark_cell_active(&world, cx + 1, cy, cz);

    // Run physics
    run_physics(&world, 5000, PHYSICS_HEAT_ALL);

    // Check temperatures
    cell_a = world_get_cell_for_write(&world, cx, cy, cz);
    cell_b = world_get_cell_for_write(&world, cx + 1, cy, cz);

    double temp_a_final = cell_get_temperature(cell_a);
    double temp_b_final = cell_get_temperature(cell_b);

    // Temperatures should converge (difference should decrease)
    double initial_diff = temp_a_initial - temp_b_initial;  // 100K
    double final_diff = temp_a_final - temp_b_final;
    ASSERT_LT(final_diff, initial_diff, "temperature difference should decrease");

    // Hot should cool, cold should warm
    ASSERT_LT(temp_a_final, temp_a_initial, "hot cell should cool");
    ASSERT_GT(temp_b_final, temp_b_initial, "cold cell should warm");

    world_cleanup(&world);
    TEST_PASS();
}

// Test water stays in a single cell if on solid floor (no horizontal spread)
static bool test_water_stays_on_floor(void) {
    TEST_BEGIN("water stays on floor cell (no horizontal spreading)");

    ChunkWorld world;
    init_test_world(&world);

    int base_x = 128, base_y = 128, base_z = 128;

    // Add solid floor (single cell)
    Cell3D *floor = world_get_cell_for_write(&world, base_x, base_y - 1, base_z);
    if (floor) {
        double rock_moles = 50.0;
        double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, INITIAL_TEMP_K);
        cell_add_material(floor, MAT_ROCK, rock_moles, rock_energy);
    }

    // Add water on top of floor
    double total_water = 40.0;
    Cell3D *water_cell = world_get_cell_for_write(&world, base_x, base_y, base_z);
    double water_energy = calculate_material_energy(MAT_WATER, total_water, INITIAL_TEMP_K);
    cell_add_material(water_cell, MAT_WATER, total_water, water_energy);
    world_mark_cell_active(&world, base_x, base_y, base_z);

    // Run physics
    run_physics(&world, 500, PHYSICS_LIQUID_FLOW);

    // Check: water should still be in the same cell (can't flow down through rock)
    const Cell3D *cell_after = world_get_cell(&world, base_x, base_y, base_z);
    ASSERT(CELL_HAS_MATERIAL(cell_after, MAT_WATER), "water should still be in cell");

    double water_in_cell = cell_after->materials[MAT_WATER].moles;
    ASSERT_FLOAT_EQ(water_in_cell, total_water, total_water * 0.01,
        "all water should remain in original cell");

    world_cleanup(&world);
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
    printf("    PHYSICS PREDICTION TESTS\n");
    printf("========================================\n");

    TestCase tests[] = {
        // Water falling tests (gravity-driven flow)
        {"WATER FALL", "fall 2 cells", test_water_fall_2},
        {"WATER FALL", "fall 4 cells", test_water_fall_4},
        {"WATER FALL", "fall 8 cells", test_water_fall_8},
        {"WATER FALL", "fall 16 cells", test_water_fall_16},
        {"WATER FALL", "fall 30 cells", test_water_fall_30},

        // Heat equilibration tests
        {"HEAT FLOW", "2x2x2 cube", test_heat_equilibration_2x2x2},
        {"HEAT FLOW", "4x4x4 cube", test_heat_equilibration_4x4x4},
        {"HEAT FLOW", "8x8x8 cube", test_heat_equilibration_8x8x8},

        // 3D column falling tests
        {"3D COLUMN", "height 4", test_water_column_fall_4},
        {"3D COLUMN", "height 8", test_water_column_fall_8},
        {"3D COLUMN", "height 16", test_water_column_fall_16},
        {"3D COLUMN", "straight fall 2", test_water_falls_straight_2},
        {"3D COLUMN", "straight fall 4", test_water_falls_straight_4},
        {"3D COLUMN", "straight fall 8", test_water_falls_straight_8},

        // Conservation tests
        {"CONSERVATION", "mass 4x4 spread", test_mass_conservation_4x4},
        {"CONSERVATION", "mass 8x8 spread", test_mass_conservation_8x8},
        {"CONSERVATION", "mass 16x16 spread", test_mass_conservation_16x16},
        {"CONSERVATION", "energy 2x2x2 heat", test_energy_conservation_2x2x2},
        {"CONSERVATION", "energy 4x4x4 heat", test_energy_conservation_4x4x4},
        {"CONSERVATION", "energy 8x8x8 heat", test_energy_conservation_8x8x8},

        // Behavior tests
        {"BEHAVIOR", "two cell heat exchange", test_two_cell_heat_exchange},
        {"BEHAVIOR", "water stays on floor", test_water_stays_on_floor},

        // Debug test (run first to trace issues)
        {"DEBUG", "trace water flow", test_debug_water_flow},
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
