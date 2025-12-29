/**
 * test_flow_behavior.c - Flow Physics Tests
 *
 * Tests that verify matter flows correctly:
 * - Solids stay put (no flow)
 * - Liquids flow downhill
 * - Ice blocks water flow
 * - Gases diffuse
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

// Create flat terrain at given height
static void create_flat_terrain(int terrain[MATTER_RES][MATTER_RES], int height) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            terrain[x][z] = height;
        }
    }
}

// Create terrain with a depression (valley) in center
static void create_valley_terrain(int terrain[MATTER_RES][MATTER_RES], int rim_height, int valley_height) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Valley in center 40x40 area
            if (x >= 60 && x < 100 && z >= 60 && z < 100) {
                terrain[x][z] = valley_height;
            } else {
                terrain[x][z] = rim_height;
            }
        }
    }
}

// Create sloped terrain (higher on one side)
static void create_sloped_terrain(int terrain[MATTER_RES][MATTER_RES], int min_height, int max_height) {
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Linear slope from left (high) to right (low)
            terrain[x][z] = max_height - (x * (max_height - min_height) / MATTER_RES);
        }
    }
}

// Run N simulation steps
static void run_matter_steps(MatterState *state, int steps) {
    for (int i = 0; i < steps; i++) {
        matter_step(state);
    }
}

// Get water depth at position
static float get_water_at(const MatterState *state, int x, int z) {
    if (x < 0 || x >= MATTER_RES || z < 0 || z >= MATTER_RES) return 0;
    return FIXED_TO_FLOAT(CELL_H2O_LIQUID(&state->cells[x][z]));
}

// Add water at ambient temperature (properly handles energy)
static void add_water_with_energy(MatterCell *cell, fixed16_t mass) {
    CELL_H2O_LIQUID(cell) += mass;
    // Add energy for the water at ambient temperature (293K)
    fixed16_t energy_for_water = fixed_mul(fixed_mul(mass, SPECIFIC_HEAT_H2O_LIQUID), AMBIENT_TEMP);
    cell->energy += energy_for_water;
    cell_update_cache(cell);
}

// ============ SOLIDS STAY PUT TESTS ============

void test_solid_silicate_does_not_flow(void) {
    TEST("solid silicate does not flow");

    int terrain[MATTER_RES][MATTER_RES];
    create_sloped_terrain(terrain, 2, 10);

    MatterState state;
    matter_init(&state, terrain, 11111);

    // Record initial silicate distribution
    fixed16_t silicate_before[MATTER_RES][MATTER_RES];
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            silicate_before[x][z] = CELL_SILICATE_SOLID(&state.cells[x][z]);
        }
    }

    // Run simulation
    run_matter_steps(&state, 500);

    // Verify silicate hasn't moved
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            fixed16_t before = silicate_before[x][z];
            fixed16_t after = CELL_SILICATE_SOLID(&state.cells[x][z]);
            ASSERT(before == after, "silicate moved when it shouldn't");
        }
    }

    PASS();
}

void test_ice_does_not_flow(void) {
    TEST("ice does not flow");

    int terrain[MATTER_RES][MATTER_RES];
    create_sloped_terrain(terrain, 2, 10);

    MatterState state;
    matter_init(&state, terrain, 22222);

    // Add ice to sloped area (left side, high terrain)
    for (int x = 10; x < 30; x++) {
        for (int z = 60; z < 100; z++) {
            CELL_H2O_ICE(&state.cells[x][z]) = FLOAT_TO_FIXED(2.0f);
            state.cells[x][z].temperature = FLOAT_TO_FIXED(260.0f); // Below freezing
            cell_update_cache(&state.cells[x][z]);
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(260.0f));
        }
    }

    // Record initial ice positions
    fixed16_t ice_before[MATTER_RES][MATTER_RES];
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            ice_before[x][z] = CELL_H2O_ICE(&state.cells[x][z]);
        }
    }

    // Run simulation (keeping cold so ice doesn't melt)
    for (int i = 0; i < 100; i++) {
        matter_step(&state);
        // Keep cells cold
        for (int x = 10; x < 30; x++) {
            for (int z = 60; z < 100; z++) {
                state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(260.0f));
            }
        }
    }

    // Verify ice hasn't flowed downhill (check right side, low terrain)
    for (int x = 130; x < 150; x++) {
        for (int z = 60; z < 100; z++) {
            fixed16_t ice = CELL_H2O_ICE(&state.cells[x][z]);
            ASSERT(ice == 0, "ice flowed to low terrain when it shouldn't");
        }
    }

    PASS();
}

void test_ash_does_not_flow(void) {
    TEST("ash does not flow");

    int terrain[MATTER_RES][MATTER_RES];
    create_sloped_terrain(terrain, 2, 10);

    MatterState state;
    matter_init(&state, terrain, 33333);

    // Add ash to high terrain
    for (int x = 10; x < 30; x++) {
        for (int z = 60; z < 100; z++) {
            state.cells[x][z].ash_solid = FLOAT_TO_FIXED(1.0f);
            cell_update_cache(&state.cells[x][z]);
        }
    }

    fixed16_t ash_before = 0;
    for (int x = 10; x < 30; x++) {
        for (int z = 60; z < 100; z++) {
            ash_before += state.cells[x][z].ash_solid;
        }
    }

    // Run simulation
    run_matter_steps(&state, 500);

    fixed16_t ash_after = 0;
    for (int x = 10; x < 30; x++) {
        for (int z = 60; z < 100; z++) {
            ash_after += state.cells[x][z].ash_solid;
        }
    }

    // Check ash on low terrain (should be zero)
    fixed16_t ash_low = 0;
    for (int x = 130; x < 150; x++) {
        for (int z = 60; z < 100; z++) {
            ash_low += state.cells[x][z].ash_solid;
        }
    }

    ASSERT(ash_low == 0, "ash flowed to low terrain");
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(ash_after), FIXED_TO_FLOAT(ash_before), 0.01f, "ash disappeared");
    PASS();
}

void test_cellulose_does_not_flow(void) {
    TEST("cellulose (vegetation) does not flow");

    int terrain[MATTER_RES][MATTER_RES];
    create_sloped_terrain(terrain, 2, 10);

    MatterState state;
    matter_init(&state, terrain, 44444);

    // Clear initial cellulose and add specific amount to high terrain
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            state.cells[x][z].cellulose_solid = 0;
        }
    }

    for (int x = 10; x < 30; x++) {
        for (int z = 60; z < 100; z++) {
            state.cells[x][z].cellulose_solid = FLOAT_TO_FIXED(0.5f);
            cell_update_cache(&state.cells[x][z]);
        }
    }

    // Run simulation (at ambient temp so it doesn't burn)
    run_matter_steps(&state, 500);

    // Check low terrain (should have no cellulose)
    fixed16_t cellulose_low = 0;
    for (int x = 130; x < 150; x++) {
        for (int z = 60; z < 100; z++) {
            cellulose_low += state.cells[x][z].cellulose_solid;
        }
    }

    ASSERT(cellulose_low == 0, "cellulose flowed to low terrain");
    PASS();
}

// ============ LIQUIDS FLOW DOWNHILL TESTS ============

void test_water_flows_to_lower_terrain(void) {
    TEST("water flows to lower terrain");

    // Create sloped terrain
    int terrain[MATTER_RES][MATTER_RES];
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Linear slope from left (high=10) to right (low=2)
            terrain[x][z] = 10 - (x * 8 / MATTER_RES);
        }
    }

    MatterState state;
    matter_init(&state, terrain, 55555);

    // Add water to high side (left) - with proper energy at ambient temp
    int center = MATTER_RES / 2;
    for (int x = 5; x < 15; x++) {
        for (int z = center - 5; z < center + 5; z++) {
            add_water_with_energy(&state.cells[x][z], FLOAT_TO_FIXED(5.0f));
        }
    }

    // Get total water before
    float total_before = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            total_before += get_water_at(&state, x, z);
        }
    }

    // Run matter simulation
    run_matter_steps(&state, 500);

    // Get total water after
    float total_after = 0;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            total_after += get_water_at(&state, x, z);
        }
    }

    // Check water at original location vs new locations
    float water_at_start = 0;
    for (int x = 5; x < 15; x++) {
        for (int z = center - 5; z < center + 5; z++) {
            water_at_start += get_water_at(&state, x, z);
        }
    }

    float water_downstream = 0;
    for (int x = 20; x < 60; x++) {  // Downstream from original location
        for (int z = center - 10; z < center + 10; z++) {
            water_downstream += get_water_at(&state, x, z);
        }
    }

    // Debug output
    printf("\n    [DEBUG] total before: %.2f, after: %.2f\n", total_before, total_after);
    printf("    [DEBUG] water at start: %.2f, downstream: %.2f\n", water_at_start, water_downstream);

    // Water should have spread downstream
    ASSERT(water_downstream > 0.1f, "water didn't flow downstream");
    PASS();
}

void test_water_pools_in_depression(void) {
    TEST("water pools in depression");

    // Create valley terrain
    int terrain[MATTER_RES][MATTER_RES];
    int valley_start = 60;
    int valley_end = 100;
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            if (x >= valley_start && x < valley_end && z >= valley_start && z < valley_end) {
                terrain[x][z] = 3;  // Valley floor
            } else {
                terrain[x][z] = 8;  // Rim
            }
        }
    }

    MatterState state;
    matter_init(&state, terrain, 66666);

    // Add water to rim (high terrain) near the valley edge - with proper energy at ambient temp
    // Valley is at 60-100, so add water just outside the valley at x=50-60
    for (int x = 50; x < 60; x++) {
        for (int z = 70; z < 90; z++) {
            add_water_with_energy(&state.cells[x][z], FLOAT_TO_FIXED(3.0f));
        }
    }

    // Run simulation
    run_matter_steps(&state, 500);

    // Check water accumulated in valley (center)
    float water_in_valley = 0;
    for (int x = valley_start; x < valley_end; x++) {
        for (int z = valley_start; z < valley_end; z++) {
            water_in_valley += get_water_at(&state, x, z);
        }
    }

    ASSERT(water_in_valley > 0.1f, "water didn't pool in valley");
    PASS();
}

void test_water_spreads_on_flat_surface(void) {
    TEST("water spreads on flat surface");

    // Create flat terrain
    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 77777);

    // Add water to center area - with proper energy at ambient temp
    int center = MATTER_RES / 2;
    for (int dx = -2; dx <= 2; dx++) {
        for (int dz = -2; dz <= 2; dz++) {
            add_water_with_energy(&state.cells[center+dx][center+dz], FLOAT_TO_FIXED(5.0f));
        }
    }

    float center_before = get_water_at(&state, center, center);

    // Run simulation
    run_matter_steps(&state, 500);

    float center_after = get_water_at(&state, center, center);

    // Water should have spread out from center (center height decreases)
    ASSERT(center_after < center_before, "water didn't spread from center");

    // Check water spread to outer ring
    float outer_water = 0;
    for (int dx = -5; dx <= 5; dx++) {
        for (int dz = -5; dz <= 5; dz++) {
            if (abs(dx) == 5 || abs(dz) == 5) {  // Outer ring only
                outer_water += get_water_at(&state, center+dx, center+dz);
            }
        }
    }

    ASSERT(outer_water > 0.1f, "water didn't spread to outer ring");
    PASS();
}

void test_liquid_blocked_by_higher_terrain(void) {
    TEST("liquid blocked by higher terrain");

    // Create terrain with a wall
    int terrain[MATTER_RES][MATTER_RES];
    int wall_x = MATTER_RES / 2;  // Wall in center

    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            // Create wall in center (4 cells wide)
            if (x >= wall_x - 2 && x < wall_x + 2) {
                terrain[x][z] = 15; // High wall
            } else {
                terrain[x][z] = 3;  // Low ground
            }
        }
    }

    MatterState state;
    matter_init(&state, terrain, 88888);

    // Add water on left side of wall - with proper energy at ambient temp
    for (int x = 10; x < 25; x++) {
        for (int z = 60; z < 100; z++) {
            add_water_with_energy(&state.cells[x][z], FLOAT_TO_FIXED(5.0f));
        }
    }

    // Run simulation
    run_matter_steps(&state, 500);

    // Check water on right side of wall (should be minimal)
    float water_right = 0;
    for (int x = wall_x + 5; x < MATTER_RES - 5; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            water_right += get_water_at(&state, x, z);
        }
    }

    // Water level (5) + terrain (3) = 8, which is less than wall (15)
    // So water should not cross
    ASSERT(water_right < 1.0f, "water crossed wall barrier");
    PASS();
}

// ============ ICE BLOCKS WATER TESTS ============

void test_ice_blocks_water_flow(void) {
    TEST("ice blocks water inflow");

    // Create flat terrain
    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 99999);

    // Create ice barrier in center (cold cells with ice)
    int center = MATTER_RES / 2;
    for (int x = center - 2; x < center + 2; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            CELL_H2O_ICE(&state.cells[x][z]) = FLOAT_TO_FIXED(5.0f);
            // Keep cells cold so ice doesn't melt
            state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(260.0f));
            cell_update_cache(&state.cells[x][z]);
        }
    }

    // Add water on one side of ice - with proper energy at ambient temp
    for (int x = 10; x < center - 5; x++) {
        for (int z = center - 10; z < center + 10; z++) {
            add_water_with_energy(&state.cells[x][z], FLOAT_TO_FIXED(3.0f));
        }
    }

    // Run simulation (keeping ice cold)
    for (int i = 0; i < 500; i++) {
        matter_step(&state);
        // Keep ice barrier cold
        for (int x = center - 2; x < center + 2; x++) {
            for (int z = 0; z < MATTER_RES; z++) {
                state.cells[x][z].energy = fixed_mul(state.cells[x][z].thermal_mass, FLOAT_TO_FIXED(260.0f));
            }
        }
    }

    // Check water on other side of ice (should be minimal)
    float water_blocked_side = 0;
    for (int x = center + 5; x < MATTER_RES - 5; x++) {
        for (int z = center - 10; z < center + 10; z++) {
            water_blocked_side += get_water_at(&state, x, z);
        }
    }

    ASSERT(water_blocked_side < 0.5f, "water crossed ice barrier");
    PASS();
}

// ============ GAS DIFFUSION TESTS ============

void test_gas_spreads_uniformly(void) {
    TEST("gas spreads uniformly");

    int terrain[MATTER_RES][MATTER_RES];
    create_flat_terrain(terrain, 5);

    MatterState state;
    matter_init(&state, terrain, 55555);

    // Clear existing gases and add concentrated CO2 in center
    for (int x = 0; x < MATTER_RES; x++) {
        for (int z = 0; z < MATTER_RES; z++) {
            state.cells[x][z].co2_gas = 0;
        }
    }

    // Add high concentration in center
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            state.cells[x][z].co2_gas = FLOAT_TO_FIXED(10.0f);
            cell_update_cache(&state.cells[x][z]);
        }
    }

    fixed16_t center_before = 0;
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            center_before += state.cells[x][z].co2_gas;
        }
    }

    // Run simulation (matter_step now calls matter_diffuse_gases)
    run_matter_steps(&state, 500);

    fixed16_t center_after = 0;
    for (int x = 78; x < 82; x++) {
        for (int z = 78; z < 82; z++) {
            center_after += state.cells[x][z].co2_gas;
        }
    }

    // Check gas spread to surrounding area
    fixed16_t ring_gas = 0;
    for (int x = 70; x < 90; x++) {
        for (int z = 70; z < 90; z++) {
            if (x < 78 || x >= 82 || z < 78 || z >= 82) {
                ring_gas += state.cells[x][z].co2_gas;
            }
        }
    }

    // Center should have decreased
    ASSERT(center_after < center_before, "gas didn't diffuse from center");
    // Surrounding area should have gas
    ASSERT(ring_gas > 0, "gas didn't spread to surrounding area");
    PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n========================================\n");
    printf("Flow Behavior Tests\n");
    printf("========================================\n\n");

    printf("=== SOLIDS STAY PUT ===\n\n");
    test_solid_silicate_does_not_flow();
    test_ice_does_not_flow();
    test_ash_does_not_flow();
    test_cellulose_does_not_flow();

    printf("\n=== LIQUIDS FLOW DOWNHILL ===\n\n");
    test_water_flows_to_lower_terrain();
    test_water_pools_in_depression();
    test_water_spreads_on_flat_surface();
    test_liquid_blocked_by_higher_terrain();

    printf("\n=== ICE BLOCKS WATER ===\n\n");
    test_ice_blocks_water_flow();

    printf("\n=== GAS DIFFUSION ===\n\n");
    test_gas_spreads_uniformly();

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
