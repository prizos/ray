/**
 * End-to-End Simulation Tests (Non-Graphical)
 *
 * These tests verify the complete simulation loop works correctly,
 * including terrain generation, physics stepping, and tool interactions.
 *
 * Each test documents a THEORY of what should be observable in the
 * data structures if the simulation is working correctly.
 */

#include "chunk.h"
#include "terrain.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============ TEST INFRASTRUCTURE ============

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %s... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)

// ============ SIMULATION HELPERS ============

/**
 * Create terrain with a specific seed for reproducibility.
 * Returns the terrain height array.
 */
static void generate_terrain_with_seed(int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION], uint32_t seed) {
    TerrainConfig config = terrain_config_default(seed);
    terrain_generate_seeded(terrain, &config);
}

/**
 * Initialize the full simulation (SVO with terrain).
 */
static void init_simulation(MatterSVO *svo, int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION], uint32_t seed) {
    generate_terrain_with_seed(terrain, seed);
    svo_init(svo, terrain);
}

/**
 * Run N physics steps.
 */
static void run_physics_steps(MatterSVO *svo, int steps) {
    float dt = 0.016f;  // ~60 FPS timestep
    for (int i = 0; i < steps; i++) {
        svo_physics_step(svo, dt);
    }
}

/**
 * Count total water moles in the entire SVO.
 */
static double count_total_water_moles(MatterSVO *svo);

/**
 * Count total thermal energy in the entire SVO.
 */
static double count_total_thermal_energy(MatterSVO *svo);

/**
 * Get average temperature in a region.
 */
static double get_region_avg_temperature(MatterSVO *svo, int cx, int cy, int cz, int radius);

/**
 * Check if water exists at a specific cell.
 */
static bool has_water_at(MatterSVO *svo, int cx, int cy, int cz);

/**
 * Get water moles at a specific cell.
 */
static double get_water_moles_at(MatterSVO *svo, int cx, int cy, int cz);

/**
 * Count total water moles by iterating over all chunks.
 */
static double count_total_water_moles(MatterSVO *svo) {
    double total = 0.0;
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = svo->hash_table[h];
        while (chunk) {
            for (int i = 0; i < CHUNK_VOLUME; i++) {
                Cell3D *cell = &chunk->cells[i];
                if (CELL_HAS_MATERIAL(cell, MAT_WATER)) {
                    total += cell->materials[MAT_WATER].moles;
                }
            }
            chunk = chunk->hash_next;
        }
    }
    return total;
}

/**
 * Count total thermal energy by iterating over all chunks.
 */
static double count_total_thermal_energy(MatterSVO *svo) {
    double total = 0.0;
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = svo->hash_table[h];
        while (chunk) {
            for (int i = 0; i < CHUNK_VOLUME; i++) {
                Cell3D *cell = &chunk->cells[i];
                CELL_FOR_EACH_MATERIAL(cell, type) {
                    total += cell->materials[type].thermal_energy;
                }
            }
            chunk = chunk->hash_next;
        }
    }
    return total;
}

static bool has_water_at(MatterSVO *svo, int cx, int cy, int cz) {
    const Cell3D *cell = svo_get_cell(svo, cx, cy, cz);
    if (!cell) return false;

    return CELL_HAS_MATERIAL(cell, MAT_WATER) &&
           cell->materials[MAT_WATER].moles > MOLES_EPSILON;
}

static double get_water_moles_at(MatterSVO *svo, int cx, int cy, int cz) {
    const Cell3D *cell = svo_get_cell(svo, cx, cy, cz);
    if (!cell) return 0.0;

    if (CELL_HAS_MATERIAL(cell, MAT_WATER)) {
        return cell->materials[MAT_WATER].moles;
    }
    return 0.0;
}

static double get_region_avg_temperature(MatterSVO *svo, int cx, int cy, int cz, int radius) {
    double sum_temp = 0;
    int count = 0;

    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dz = -radius; dz <= radius; dz++) {
                Cell3D *cell = svo_get_cell_for_write(svo, cx + dx, cy + dy, cz + dz);
                if (cell && cell->present != 0) {
                    double temp = cell_get_temperature(cell);
                    if (temp > 0) {
                        sum_temp += temp;
                        count++;
                    }
                }
            }
        }
    }

    return count > 0 ? sum_temp / count : 0;
}

// ============ TEST: BASIC SIMULATION INITIALIZATION ============

/**
 * THEORY: After initialization with a seed:
 * 1. The SVO should have a valid root node
 * 2. Active and dirty lists should be initialized
 * 3. Terrain cells should contain rock/dirt materials underground
 * 4. Air cells should exist above ground
 * 5. Same seed should produce identical terrain
 */
void test_basic_initialization(void) {
    TEST("basic simulation initialization");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 12345);

    // Check chunk world was initialized (has hash table)
    // The chunk system uses lazy allocation - cells don't exist until written
    // Verify we can write and read back a cell
    Cell3D *test_cell = svo_get_cell_for_write(&svo, 128, 200, 128);
    if (!test_cell) {
        FAIL("Cannot write to test cell");
        svo_cleanup(&svo);
        return;
    }
    // Verify we can read it back
    const Cell3D *read_cell = svo_get_cell(&svo, 128, 200, 128);
    if (!read_cell) {
        FAIL("Cannot read test cell after write");
        svo_cleanup(&svo);
        return;
    }

    // Check terrain reproducibility with same seed
    int terrain2[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    generate_terrain_with_seed(terrain2, 12345);

    bool terrain_matches = true;
    for (int x = 0; x < TERRAIN_RESOLUTION && terrain_matches; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION && terrain_matches; z++) {
            if (terrain[x][z] != terrain2[x][z]) {
                terrain_matches = false;
            }
        }
    }

    if (!terrain_matches) {
        FAIL("Same seed produces different terrain");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

/**
 * THEORY: Different seeds should produce different terrain.
 */
void test_seed_variation(void) {
    TEST("different seeds produce different terrain");

    int terrain1[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    int terrain2[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    generate_terrain_with_seed(terrain1, 11111);
    generate_terrain_with_seed(terrain2, 22222);

    int differences = 0;
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (terrain1[x][z] != terrain2[x][z]) {
                differences++;
            }
        }
    }

    // Should have significant differences
    if (differences < TERRAIN_RESOLUTION * TERRAIN_RESOLUTION / 2) {
        FAIL("Seeds don't produce enough variation");
        return;
    }

    PASS();
}

/**
 * THEORY: Running physics on an idle world should:
 * 1. Not crash
 * 2. Maintain stable memory (no unbounded growth)
 * 3. Eventually reach equilibrium (active count stabilizes)
 */
void test_idle_physics(void) {
    TEST("idle physics runs without issues");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 54321);

    // Run 100 physics steps
    run_physics_steps(&svo, 100);

    // Should not crash (if we got here, it didn't)
    // Active count should stabilize (not grow unboundedly)
    int final_active = svo.active_count;

    // After equilibrium, active count should be reasonable
    // (less than total possible cells)
    if (final_active > 1000000) {
        FAIL("Active count grew unboundedly");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

// ============ TEST: WATER INJECTION ============

/**
 * THEORY: When water is injected at a location:
 * 1. Water moles should appear at or near that location
 * 2. Total water mass should equal what was injected
 * 3. Water should be at approximately ambient temperature (INITIAL_TEMP_K)
 */
void test_water_injection_basic(void) {
    TEST("water injection creates water at location");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 99999);

    // Inject water at center, above ground
    // World coords (0, 20, 0) maps to cell coords (128, 136, 128)
    float wx = 0.0f;
    float wy = 20.0f;  // Above ground
    float wz = 0.0f;
    double injected_moles = 5.0;

    double water_before = count_total_water_moles(&svo);

    svo_add_water_at(&svo, wx, wy, wz, injected_moles);

    double water_after = count_total_water_moles(&svo);
    double water_added = water_after - water_before;

    // Check water was added (within tolerance for floating point)
    if (fabs(water_added - injected_moles) > 0.01) {
        printf("\n    Expected %.3f moles, got %.3f\n", injected_moles, water_added);
        FAIL("Water moles don't match injected amount");
        svo_cleanup(&svo);
        return;
    }

    // Check water is near injection point
    int cx, cy, cz;
    svo_world_to_cell(wx, wy, wz, &cx, &cy, &cz);

    if (!has_water_at(&svo, cx, cy, cz)) {
        FAIL("Water not found at injection cell");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

/**
 * THEORY: After physics steps, water should:
 * 1. Flow downward due to gravity
 * 2. Accumulate at lower elevations
 * 3. Maintain conservation of mass (total moles unchanged)
 */
void test_water_flows_down(void) {
    TEST("water flows downward and conserves mass");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 77777);

    // Inject water high up
    // World coords (0, 50, 0) maps to cell coords (128, 148, 128)
    float wx = 0.0f;
    float wy = 50.0f;  // High above ground
    float wz = 0.0f;
    double injected_moles = 10.0;

    int cx, cy, cz;
    svo_world_to_cell(wx, wy, wz, &cx, &cy, &cz);

    svo_add_water_at(&svo, wx, wy, wz, injected_moles);

    double initial_water = count_total_water_moles(&svo);
    double initial_at_injection = get_water_moles_at(&svo, cx, cy, cz);

    // Run physics to let water flow
    run_physics_steps(&svo, 200);

    double final_water = count_total_water_moles(&svo);
    double final_at_injection = get_water_moles_at(&svo, cx, cy, cz);

    // Mass should be conserved (within 1% tolerance)
    double mass_error = fabs(final_water - initial_water) / initial_water;
    if (mass_error > 0.01) {
        printf("\n    Initial: %.3f, Final: %.3f, Error: %.2f%%\n",
               initial_water, final_water, mass_error * 100);
        FAIL("Water mass not conserved");
        svo_cleanup(&svo);
        return;
    }

    // Water should have flowed away from injection point
    if (final_at_injection >= initial_at_injection * 0.9) {
        printf("\n    Initial at point: %.3f, Final: %.3f\n",
               initial_at_injection, final_at_injection);
        FAIL("Water didn't flow away from injection point");
        svo_cleanup(&svo);
        return;
    }

    // Check that water exists at lower elevation
    bool found_water_below = false;
    for (int y = cy - 1; y >= cy - 20 && y >= 0; y--) {
        if (has_water_at(&svo, cx, y, cz)) {
            found_water_below = true;
            break;
        }
    }

    if (!found_water_below) {
        FAIL("No water found below injection point");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

// ============ TEST: HEAT INJECTION ============

/**
 * THEORY: When heat is injected at a location:
 * 1. Temperature at that location should increase
 * 2. Total thermal energy should increase by injected amount
 * 3. Heat should begin spreading to neighbors
 */
void test_heat_injection_basic(void) {
    TEST("heat injection increases temperature");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 55555);

    // Use cell coordinates directly - cy=150 is above ground level (128)
    int cx = 128, cy = 150, cz = 128;
    float wx, wy, wz;
    svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);

    // Force the cell to exist and add some matter (chunk system doesn't have air by default)
    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!cell) {
        FAIL("Cannot access test cell");
        svo_cleanup(&svo);
        return;
    }
    // Add some water to have material to heat (empty cells have no temperature)
    double initial_moles = 1.0;
    cell_add_material(cell, MAT_WATER, initial_moles,
                      initial_moles * MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid * INITIAL_TEMP_K);

    double temp_before = cell_get_temperature(cell);
    double energy_before = count_total_thermal_energy(&svo);

    // Inject significant heat
    double injected_energy = 50000.0;  // 50kJ
    svo_add_heat_at(&svo, wx, wy, wz, injected_energy);

    // Re-read cell (might have been modified)
    cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    double temp_after = cell_get_temperature(cell);
    double energy_after = count_total_thermal_energy(&svo);

    // Temperature should have increased
    if (temp_after <= temp_before) {
        printf("\n    Before: %.1fK, After: %.1fK\n", temp_before, temp_after);
        FAIL("Temperature didn't increase");
        svo_cleanup(&svo);
        return;
    }

    // Energy should have increased (approximately by injected amount)
    double energy_increase = energy_after - energy_before;
    if (energy_increase < injected_energy * 0.9) {
        printf("\n    Injected: %.0fJ, Increase: %.0fJ\n", injected_energy, energy_increase);
        FAIL("Energy increase less than expected");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

/**
 * THEORY: Heat should conduct to neighbors according to Fourier's law:
 * 1. Heat flows from hot to cold
 * 2. After equilibration, temperature gradient should reduce
 * 3. Total energy should be conserved (closed system)
 */
void test_heat_conduction(void) {
    TEST("heat conducts to neighbors and conserves energy");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 44444);

    int cx = 128, cy = 150, cz = 128;
    float wx, wy, wz;
    svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);

    // Add water to both cells (empty cells have no temperature)
    double moles = 1.0;
    double initial_energy = moles * MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid * INITIAL_TEMP_K;

    Cell3D *cell1 = svo_get_cell_for_write(&svo, cx, cy, cz);
    Cell3D *cell2 = svo_get_cell_for_write(&svo, cx + 1, cy, cz);
    cell_add_material(cell1, MAT_WATER, moles, initial_energy);
    cell_add_material(cell2, MAT_WATER, moles, initial_energy);

    // Get initial temperature in region
    double region_temp_before = get_region_avg_temperature(&svo, cx, cy, cz, 0);

    // Create a hot spot with significant energy
    svo_add_heat_at(&svo, wx, wy, wz, 50000.0);  // 50kJ

    double energy_after_injection = count_total_thermal_energy(&svo);

    // Get hot spot temperature after heating
    double hot_temp_before = get_region_avg_temperature(&svo, cx, cy, cz, 0);

    // Run physics for heat conduction
    run_physics_steps(&svo, 200);

    double hot_temp_after = get_region_avg_temperature(&svo, cx, cy, cz, 0);
    double region_temp_after = get_region_avg_temperature(&svo, cx, cy, cz, 2);
    double energy_after_physics = count_total_thermal_energy(&svo);

    // Hot spot should cool down (spread to neighbors)
    // Check for any meaningful cooling (even small amount shows heat is spreading)
    if (hot_temp_after >= hot_temp_before) {
        printf("\n    Hot spot: %.1fK -> %.1fK (no cooling)\n", hot_temp_before, hot_temp_after);
        FAIL("Hot spot didn't cool down");
        svo_cleanup(&svo);
        return;
    }

    // Region average should have increased (heat spread out)
    if (region_temp_after <= region_temp_before) {
        printf("\n    Region avg: %.1fK -> %.1fK\n", region_temp_before, region_temp_after);
        FAIL("Region didn't warm up");
        svo_cleanup(&svo);
        return;
    }

    // Energy should be conserved (within 5% - some numerical error expected)
    double energy_error = fabs(energy_after_physics - energy_after_injection) / energy_after_injection;
    if (energy_error > 0.05) {
        printf("\n    After injection: %.0fJ, After physics: %.0fJ, Error: %.1f%%\n",
               energy_after_injection, energy_after_physics, energy_error * 100);
        FAIL("Energy not conserved during conduction");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

// ============ TEST: COLD INJECTION ============

/**
 * THEORY: When cold (negative heat) is injected:
 * 1. Temperature at that location should decrease
 * 2. Temperature cannot go below absolute zero
 * 3. Total thermal energy should decrease
 */
void test_cold_injection_basic(void) {
    TEST("cold injection decreases temperature");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 33333);

    int cx = 128, cy = 150, cz = 128;
    float wx, wy, wz;
    svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);

    // Create cell and add material (empty cells have no temperature)
    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    if (!cell) {
        FAIL("Cannot access test cell");
        svo_cleanup(&svo);
        return;
    }
    // Add water at room temperature
    double moles = 1.0;
    cell_add_material(cell, MAT_WATER, moles,
                      moles * MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid * INITIAL_TEMP_K);

    double temp_before = cell_get_temperature(cell);

    // Remove heat (cool down)
    svo_remove_heat_at(&svo, wx, wy, wz, 5000.0);

    cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    double temp_after = cell_get_temperature(cell);

    // Temperature should decrease
    if (temp_after >= temp_before) {
        printf("\n    Before: %.1fK, After: %.1fK\n", temp_before, temp_after);
        FAIL("Temperature didn't decrease");
        svo_cleanup(&svo);
        return;
    }

    // Temperature should not be negative
    if (temp_after < 0) {
        printf("\n    Temperature went negative: %.1fK\n", temp_after);
        FAIL("Temperature below absolute zero");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

/**
 * THEORY: Extreme cooling should not cause temperature to go below 0K.
 */
void test_cold_clamps_at_zero(void) {
    TEST("extreme cold cannot go below absolute zero");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 22222);

    int cx = 128, cy = 150, cz = 128;
    float wx, wy, wz;
    svo_cell_to_world(cx, cy, cz, &wx, &wy, &wz);

    // Force cell to exist
    svo_get_cell_for_write(&svo, cx, cy, cz);

    // Try to remove massive amount of heat
    svo_remove_heat_at(&svo, wx, wy, wz, 1000000000.0);  // 1 GJ

    const Cell3D *cell = svo_get_cell(&svo, cx, cy, cz);
    if (cell && cell->present != 0) {
        CELL_FOR_EACH_MATERIAL(cell, type) {
            if (cell->materials[type].thermal_energy < 0) {
                FAIL("Thermal energy went negative");
                svo_cleanup(&svo);
                return;
            }
        }
    }

    svo_cleanup(&svo);
    PASS();
}

// ============ TEST: COMBINED INTERACTIONS ============

/**
 * THEORY: When water, heat, and cold are all applied:
 * 1. Water should still flow correctly
 * 2. Heated water should have higher temperature
 * 3. Cooled water should have lower temperature
 * 4. Mass and energy conservation should hold
 */
void test_combined_water_heat_cold(void) {
    TEST("combined water + heat + cold interactions");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 11111);

    // Use different cell coordinates that are far apart to avoid uniform node sharing
    int cx_hot = 100, cy = 150, cz = 128;   // Heated water
    int cx_control = 128;                    // Control water
    int cx_cold = 156;                       // Cooled water

    // Force cells to exist as individual leaf nodes
    Cell3D *cell_hot = svo_get_cell_for_write(&svo, cx_hot, cy, cz);
    Cell3D *cell_control = svo_get_cell_for_write(&svo, cx_control, cy, cz);
    Cell3D *cell_cold = svo_get_cell_for_write(&svo, cx_cold, cy, cz);

    if (!cell_hot || !cell_control || !cell_cold) {
        FAIL("Cannot access test cells");
        svo_cleanup(&svo);
        return;
    }

    // Add water to each cell directly
    double water_moles = 5.0;
    cell3d_add_material(cell_hot, MAT_WATER, water_moles,
                        water_moles * MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid * INITIAL_TEMP_K);
    cell3d_add_material(cell_control, MAT_WATER, water_moles,
                        water_moles * MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid * INITIAL_TEMP_K);
    cell3d_add_material(cell_cold, MAT_WATER, water_moles,
                        water_moles * MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid * INITIAL_TEMP_K);

    double initial_water = count_total_water_moles(&svo);

    // Apply heat to hot cell (50kJ)
    float wx_hot, wy_f, wz_f;
    svo_cell_to_world(cx_hot, cy, cz, &wx_hot, &wy_f, &wz_f);
    svo_add_heat_at(&svo, wx_hot, wy_f, wz_f, 50000.0);

    // Apply cold to cold cell (20kJ)
    float wx_cold;
    svo_cell_to_world(cx_cold, cy, cz, &wx_cold, &wy_f, &wz_f);
    svo_remove_heat_at(&svo, wx_cold, wy_f, wz_f, 20000.0);

    // Re-read cells and get temperatures (cells may have been modified)
    cell_hot = svo_get_cell_for_write(&svo, cx_hot, cy, cz);
    cell_control = svo_get_cell_for_write(&svo, cx_control, cy, cz);
    cell_cold = svo_get_cell_for_write(&svo, cx_cold, cy, cz);

    double hot_temp = cell_get_temperature(cell_hot);
    double control_temp = cell_get_temperature(cell_control);
    double cold_temp = cell_get_temperature(cell_cold);

    // Run physics
    run_physics_steps(&svo, 50);

    double final_water = count_total_water_moles(&svo);

    // Water mass should be conserved
    double water_error = fabs(final_water - initial_water) / initial_water;
    if (water_error > 0.01) {
        printf("\n    Water: %.3f -> %.3f (%.1f%% error)\n",
               initial_water, final_water, water_error * 100);
        FAIL("Water mass not conserved");
        svo_cleanup(&svo);
        return;
    }

    // Verify temperature relationships held after applying heat/cold
    // Hot water should be hotter than control
    if (hot_temp <= control_temp) {
        printf("\n    Hot: %.1fK, Control: %.1fK\n", hot_temp, control_temp);
        FAIL("Heated water wasn't hotter than control");
        svo_cleanup(&svo);
        return;
    }

    // Cold water should be colder than control
    if (cold_temp >= control_temp) {
        printf("\n    Cold: %.1fK, Control: %.1fK\n", cold_temp, control_temp);
        FAIL("Cooled water wasn't colder than control");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

/**
 * THEORY: Long simulation should reach stable equilibrium:
 * 1. Physics step count should remain bounded
 * 2. Active node count should stabilize
 * 3. Temperature gradients should diminish
 */
void test_equilibrium_reached(void) {
    TEST("simulation reaches stable equilibrium");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 88888);

    // Create a thermal disturbance
    float wx = 128 * SVO_CELL_SIZE;
    float wy = 30.0f;
    float wz = 128 * SVO_CELL_SIZE;

    svo_add_heat_at(&svo, wx, wy, wz, 100000.0);

    // Run physics for a while
    run_physics_steps(&svo, 200);
    int active_mid = svo.active_count;

    // Run more physics
    run_physics_steps(&svo, 200);
    int active_end = svo.active_count;

    // Active count should not be growing exponentially
    // (It should be similar or decreasing as system equilibrates)
    if (active_end > active_mid * 2) {
        printf("\n    Mid: %d, End: %d (%.1fx growth)\n",
               active_mid, active_end, (float)active_end / active_mid);
        FAIL("Active count growing unexpectedly");
        svo_cleanup(&svo);
        return;
    }

    svo_cleanup(&svo);
    PASS();
}

/**
 * THEORY: Physics performance should be stable (no exponential slowdown).
 * This test runs many physics steps and verifies completion in reasonable time.
 */
void test_physics_performance_stable(void) {
    TEST("physics performance remains stable");

    int terrain[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    MatterSVO svo;

    init_simulation(&svo, terrain, 66666);

    // Add some activity
    for (int i = 0; i < 5; i++) {
        float wx = (100 + i * 10) * SVO_CELL_SIZE;
        float wy = 30.0f;
        float wz = 128 * SVO_CELL_SIZE;
        svo_add_water_at(&svo, wx, wy, wz, 3.0);
        svo_add_heat_at(&svo, wx, wy, wz, 10000.0);
    }

    // Run 500 physics steps - should complete quickly if O(1) removal works
    // (Before the fix, this would hang due to O(n^2) behavior)
    run_physics_steps(&svo, 500);

    // If we got here, performance is acceptable
    svo_cleanup(&svo);
    PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("    END-TO-END SIMULATION TESTS\n");
    printf("========================================\n");
    printf("\n");

    printf("=== INITIALIZATION ===\n\n");
    test_basic_initialization();
    test_seed_variation();
    test_idle_physics();

    printf("\n=== WATER INJECTION ===\n\n");
    test_water_injection_basic();
    test_water_flows_down();

    printf("\n=== HEAT INJECTION ===\n\n");
    test_heat_injection_basic();
    test_heat_conduction();

    printf("\n=== COLD INJECTION ===\n\n");
    test_cold_injection_basic();
    test_cold_clamps_at_zero();

    printf("\n=== COMBINED INTERACTIONS ===\n\n");
    test_combined_water_heat_cold();
    test_equilibrium_reached();
    test_physics_performance_stable();

    printf("\n========================================\n");
    printf("    RESULTS: %d/%d tests passed\n", tests_passed, tests_passed + tests_failed);
    if (tests_failed == 0) {
        printf("    ALL TESTS PASSED\n");
    }
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
