/**
 * Matter System - Integration Tests
 *
 * Tests heat conduction algorithm on isolated grids.
 * No full engine - just the grid simulation logic.
 *
 * Theories tested:
 * 1. Energy is conserved across grid operations
 * 2. Heat propagates correctly from hot spots
 * 3. System reaches expected equilibrium
 * 4. No spontaneous heating in uniform grids
 * 5. Boundary conditions don't break conservation
 */

#include "test_common.h"

// ============ SIMULATION PARAMETERS ============

#define CONDUCTION_RATE 0.05f
#define FIRE_BOOST 2.0f
#define RADIATION_RATE 0.002f

// ============ TEST GRID ============

typedef struct {
    fixed16_t mass;
    fixed16_t specific_heat;
    fixed16_t energy;
    fixed16_t temperature;
    fixed16_t thermal_mass;
    bool has_fuel;
} GridCell;

typedef struct {
    GridCell *cells;
    int width;
    int height;
} TestGrid;

static void grid_init(TestGrid *g, int w, int h, float temp_k) {
    g->width = w;
    g->height = h;
    g->cells = calloc(w * h, sizeof(GridCell));

    fixed16_t temp = FLOAT_TO_FIXED(temp_k);
    fixed16_t specific_heat = FLOAT_TO_FIXED(0.7f);
    fixed16_t mass = FLOAT_TO_FIXED(1.0f);
    fixed16_t thermal_mass = fixed_mul(mass, specific_heat);
    fixed16_t energy = fixed_mul(thermal_mass, temp);

    for (int i = 0; i < w * h; i++) {
        g->cells[i].mass = mass;
        g->cells[i].specific_heat = specific_heat;
        g->cells[i].thermal_mass = thermal_mass;
        g->cells[i].temperature = temp;
        g->cells[i].energy = energy;
        g->cells[i].has_fuel = false;
    }
}

static void grid_free(TestGrid *g) {
    free(g->cells);
    g->cells = NULL;
}

static GridCell* grid_get(TestGrid *g, int x, int y) {
    if (x < 0 || x >= g->width || y < 0 || y >= g->height) return NULL;
    return &g->cells[y * g->width + x];
}

static void cell_update_temp(GridCell *c) {
    if (c->thermal_mass > FLOAT_TO_FIXED(0.001f)) {
        c->temperature = fixed_div(c->energy, c->thermal_mass);
    }
}

static fixed16_t grid_total_energy(const TestGrid *g) {
    fixed16_t total = 0;
    for (int i = 0; i < g->width * g->height; i++) {
        total += g->cells[i].energy;
    }
    return total;
}

static float grid_avg_temp(const TestGrid *g) {
    float sum = 0;
    for (int i = 0; i < g->width * g->height; i++) {
        sum += FIXED_TO_FLOAT(g->cells[i].temperature);
    }
    return sum / (g->width * g->height);
}

static float grid_max_temp(const TestGrid *g) {
    float max = -1e9f;
    for (int i = 0; i < g->width * g->height; i++) {
        float t = FIXED_TO_FLOAT(g->cells[i].temperature);
        if (t > max) max = t;
    }
    return max;
}

static float grid_min_temp(const TestGrid *g) {
    float min = 1e9f;
    for (int i = 0; i < g->width * g->height; i++) {
        float t = FIXED_TO_FLOAT(g->cells[i].temperature);
        if (t < min) min = t;
    }
    return min;
}

// ============ SIMULATION STEP ============

static void grid_step(TestGrid *g, bool use_fuel_filter, bool use_radiation) {
    fixed16_t *deltas = calloc(g->width * g->height, sizeof(fixed16_t));

    for (int y = 0; y < g->height; y++) {
        for (int x = 0; x < g->width; x++) {
            GridCell *cell = grid_get(g, x, y);

            bool i_have_fuel = cell->has_fuel;
            bool i_am_hot = cell->temperature > TEST_FIRE_TEMP;

            int dx[4] = {-1, 1, 0, 0};
            int dy[4] = {0, 0, -1, 1};

            for (int d = 0; d < 4; d++) {
                GridCell *neighbor = grid_get(g, x + dx[d], y + dy[d]);
                if (!neighbor) continue;

                bool they_have_fuel = neighbor->has_fuel;
                bool they_are_hot = neighbor->temperature > TEST_FIRE_TEMP;

                // Check for cold cells (below ambient - 50K)
                bool i_am_cold = cell->temperature < TEST_COLD_TEMP;
                bool they_are_cold = neighbor->temperature < TEST_COLD_TEMP;

                // Skip if fuel filter is on and neither has fuel/is hot/is cold
                if (use_fuel_filter) {
                    if (!i_have_fuel && !they_have_fuel && !i_am_hot && !they_are_hot &&
                        !i_am_cold && !they_are_cold) {
                        continue;
                    }
                }

                fixed16_t temp_diff = neighbor->temperature - cell->temperature;
                fixed16_t rate = FLOAT_TO_FIXED(CONDUCTION_RATE);

                if (i_am_hot || they_are_hot) {
                    rate = fixed_mul(rate, FLOAT_TO_FIXED(FIRE_BOOST));
                }

                fixed16_t heat_flow = fixed_mul(temp_diff, rate);

                // Limit by donor
                if (heat_flow > 0) {
                    fixed16_t max = neighbor->energy / 20;
                    if (heat_flow > max) heat_flow = max;
                } else if (heat_flow < 0) {
                    fixed16_t max = cell->energy / 20;
                    if (heat_flow < -max) heat_flow = -max;
                }

                deltas[y * g->width + x] += heat_flow;
            }

            // Radiation
            if (use_radiation) {
                fixed16_t excess = cell->temperature - TEST_AMBIENT_TEMP;
                if (excess > 0) {
                    fixed16_t rad = fixed_mul(excess, FLOAT_TO_FIXED(RADIATION_RATE));
                    fixed16_t max_rad = cell->energy / 100;
                    if (rad > max_rad) rad = max_rad;
                    deltas[y * g->width + x] -= rad;
                }
            }
        }
    }

    // Apply deltas
    for (int i = 0; i < g->width * g->height; i++) {
        g->cells[i].energy += deltas[i];
    }

    // Update temperatures
    for (int i = 0; i < g->width * g->height; i++) {
        cell_update_temp(&g->cells[i]);
    }

    free(deltas);
}

// ============ ENERGY CONSERVATION TESTS ============

static bool test_uniform_grid_no_change(void) {
    TEST_BEGIN("uniform grid: no spontaneous change");

    TestGrid g;
    grid_init(&g, 16, 16, TEST_AMBIENT_TEMP_K);

    fixed16_t initial = grid_total_energy(&g);

    for (int i = 0; i < 500; i++) {
        grid_step(&g, false, false);
    }

    fixed16_t final = grid_total_energy(&g);

    ASSERT_EQ(final, initial, "energy changed in uniform grid");

    float variance = grid_max_temp(&g) - grid_min_temp(&g);
    ASSERT(variance < 0.001f, "temperature variance developed");

    grid_free(&g);
    TEST_PASS();
}

static bool test_two_cell_conservation(void) {
    TEST_BEGIN("two cells: energy conserved");

    TestGrid g;
    grid_init(&g, 2, 1, 300.0f);

    // Add fuel to enable heat exchange
    g.cells[0].has_fuel = true;
    g.cells[1].has_fuel = true;

    // Heat one cell
    g.cells[0].energy = fixed_mul(g.cells[0].thermal_mass, FLOAT_TO_FIXED(400.0f));
    cell_update_temp(&g.cells[0]);

    fixed16_t initial = grid_total_energy(&g);

    for (int i = 0; i < 100; i++) {
        grid_step(&g, false, false);
    }

    fixed16_t final = grid_total_energy(&g);

    // Allow drift from fixed-point rounding over 100 iterations
    // In 2-cell exchange, small rounding each step accumulates
    int drift = abs(final - initial);
    ASSERT(drift < 100, "energy not conserved (drift=%d)", drift);

    grid_free(&g);
    TEST_PASS();
}

static bool test_3x3_center_injection(void) {
    TEST_BEGIN("3x3 grid: center heat injection");

    TestGrid g;
    grid_init(&g, 3, 3, 300.0f);

    // Heat center
    GridCell *center = grid_get(&g, 1, 1);
    center->energy = fixed_mul(center->thermal_mass, FLOAT_TO_FIXED(500.0f));
    cell_update_temp(center);

    fixed16_t initial = grid_total_energy(&g);

    // Expected equilibrium: (8*300 + 1*500) / 9 = 322.22K
    float expected_eq = (8.0f * 300.0f + 500.0f) / 9.0f;

    for (int i = 0; i < 500; i++) {
        grid_step(&g, false, false);
    }

    fixed16_t final = grid_total_energy(&g);
    float drift_pct = 100.0f * fabsf((float)(final - initial)) / (float)initial;

    ASSERT(drift_pct < 0.01f, "energy drift > 0.01%%");

    float avg = grid_avg_temp(&g);
    ASSERT_FLOAT_EQ(avg, expected_eq, 0.5f, "wrong equilibrium");

    float variance = grid_max_temp(&g) - grid_min_temp(&g);
    ASSERT(variance < 0.5f, "not at equilibrium");

    grid_free(&g);
    TEST_PASS();
}

static bool test_16x16_conservation(void) {
    TEST_BEGIN("16x16 grid: energy conservation");

    TestGrid g;
    grid_init(&g, 16, 16, 300.0f);

    // Heat corner
    g.cells[0].energy = fixed_mul(g.cells[0].thermal_mass, FLOAT_TO_FIXED(1000.0f));
    cell_update_temp(&g.cells[0]);

    fixed16_t initial = grid_total_energy(&g);

    for (int i = 0; i < 2000; i++) {
        grid_step(&g, false, false);
    }

    fixed16_t final = grid_total_energy(&g);
    float drift_pct = 100.0f * fabsf((float)(final - initial)) / (float)initial;

    ASSERT(drift_pct < 0.5f, "energy drift > 0.5%%");

    grid_free(&g);
    TEST_PASS();
}

// ============ HEAT PROPAGATION TESTS ============

static bool test_heat_spreads_from_source(void) {
    TEST_BEGIN("heat spreads from hot cell");

    TestGrid g;
    grid_init(&g, 5, 5, 300.0f);

    // Heat center
    GridCell *center = grid_get(&g, 2, 2);
    center->energy = fixed_mul(center->thermal_mass, FLOAT_TO_FIXED(500.0f));
    cell_update_temp(center);

    // Run a few steps
    for (int i = 0; i < 10; i++) {
        grid_step(&g, false, false);
    }

    // Neighbors should be warmer than corners
    float neighbor_temp = FIXED_TO_FLOAT(grid_get(&g, 2, 1)->temperature);
    float corner_temp = FIXED_TO_FLOAT(grid_get(&g, 0, 0)->temperature);

    ASSERT(neighbor_temp > corner_temp, "heat didn't spread to neighbors first");

    grid_free(&g);
    TEST_PASS();
}

static bool test_cold_spreads_from_source(void) {
    TEST_BEGIN("cold spreads from frozen cell");

    TestGrid g;
    grid_init(&g, 5, 5, 293.0f);  // Start at ambient (~20°C)

    // Freeze center to absolute zero
    GridCell *center = grid_get(&g, 2, 2);
    center->energy = 0;  // 0K
    cell_update_temp(center);

    float initial_neighbor_temp = FIXED_TO_FLOAT(grid_get(&g, 2, 1)->temperature);

    // Run steps - heat should flow INTO the cold cell, cooling neighbors
    for (int i = 0; i < 10; i++) {
        grid_step(&g, true, false);  // With fuel filter (tests cold detection)
    }

    // Neighbors should be cooler than they started
    float final_neighbor_temp = FIXED_TO_FLOAT(grid_get(&g, 2, 1)->temperature);
    ASSERT(final_neighbor_temp < initial_neighbor_temp,
           "neighbors didn't cool down near frozen cell");

    // Center should have warmed up (received heat)
    float center_temp = FIXED_TO_FLOAT(center->temperature);
    ASSERT(center_temp > 0.0f, "frozen cell didn't warm up");

    grid_free(&g);
    TEST_PASS();
}

static bool test_equilibrium_reached(void) {
    TEST_BEGIN("system reaches equilibrium");

    TestGrid g;
    grid_init(&g, 8, 8, 300.0f);

    // Create hot spot
    grid_get(&g, 4, 4)->energy = fixed_mul(
        grid_get(&g, 4, 4)->thermal_mass, FLOAT_TO_FIXED(600.0f));
    cell_update_temp(grid_get(&g, 4, 4));

    float prev_variance = 1e9f;
    for (int step = 0; step < 1000; step++) {
        grid_step(&g, false, false);

        if ((step + 1) % 100 == 0) {
            float variance = grid_max_temp(&g) - grid_min_temp(&g);
            ASSERT(variance <= prev_variance + 0.01f, "variance increased");
            prev_variance = variance;
        }
    }

    float final_variance = grid_max_temp(&g) - grid_min_temp(&g);
    ASSERT(final_variance < 1.0f, "didn't reach equilibrium");

    grid_free(&g);
    TEST_PASS();
}

// ============ FUEL FILTER TESTS ============

static bool test_no_exchange_without_fuel(void) {
    TEST_BEGIN("no exchange when no fuel and not hot");

    TestGrid g;
    grid_init(&g, 4, 4, 300.0f);

    // Create temp gradient but no fuel
    g.cells[0].energy = fixed_mul(g.cells[0].thermal_mass, FLOAT_TO_FIXED(350.0f));
    cell_update_temp(&g.cells[0]);

    for (int i = 0; i < 100; i++) {
        grid_step(&g, true, false);  // With fuel filter
    }

    // Temperatures should be unchanged (all skipped)
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(g.cells[0].temperature), 350.0f, 0.01f,
                    "hot cell changed without fuel");

    grid_free(&g);
    TEST_PASS();
}

static bool test_exchange_with_fuel(void) {
    TEST_BEGIN("exchange occurs with fuel");

    TestGrid g;
    grid_init(&g, 4, 4, 300.0f);

    // Add fuel to some cells
    for (int i = 0; i < 16; i++) {
        g.cells[i].has_fuel = (i % 2 == 0);
    }

    // Heat a fuel cell
    g.cells[0].energy = fixed_mul(g.cells[0].thermal_mass, FLOAT_TO_FIXED(400.0f));
    cell_update_temp(&g.cells[0]);

    float initial_temp = FIXED_TO_FLOAT(g.cells[0].temperature);

    for (int i = 0; i < 100; i++) {
        grid_step(&g, true, false);
    }

    float final_temp = FIXED_TO_FLOAT(g.cells[0].temperature);

    ASSERT(final_temp < initial_temp, "hot fuel cell didn't cool down");

    grid_free(&g);
    TEST_PASS();
}

// ============ RADIATION TESTS ============

static bool test_radiation_cools_hot_cells(void) {
    TEST_BEGIN("radiation cools cells above ambient");

    TestGrid g;
    grid_init(&g, 1, 1, 400.0f);  // Single hot cell

    fixed16_t initial = g.cells[0].energy;

    for (int i = 0; i < 100; i++) {
        grid_step(&g, false, true);  // With radiation
    }

    fixed16_t final = g.cells[0].energy;

    ASSERT(final < initial, "hot cell didn't lose energy to radiation");

    grid_free(&g);
    TEST_PASS();
}

static bool test_no_radiation_at_ambient(void) {
    TEST_BEGIN("no radiation at ambient temperature");

    TestGrid g;
    grid_init(&g, 4, 4, TEST_AMBIENT_TEMP_K);

    fixed16_t initial = grid_total_energy(&g);

    for (int i = 0; i < 100; i++) {
        grid_step(&g, false, true);  // With radiation
    }

    fixed16_t final = grid_total_energy(&g);

    ASSERT_EQ(final, initial, "energy changed at ambient");

    grid_free(&g);
    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Matter System - Integration Tests\n");
    printf("========================================\n");

    test_suite_begin("ENERGY CONSERVATION");
    test_uniform_grid_no_change();
    test_two_cell_conservation();
    test_3x3_center_injection();
    test_16x16_conservation();
    test_suite_end();

    test_suite_begin("HEAT PROPAGATION");
    test_heat_spreads_from_source();
    test_cold_spreads_from_source();
    test_equilibrium_reached();
    test_suite_end();

    test_suite_begin("FUEL FILTER");
    test_no_exchange_without_fuel();
    test_exchange_with_fuel();
    test_suite_end();

    test_suite_begin("RADIATION");
    test_radiation_cools_hot_cells();
    test_no_radiation_at_ambient();
    test_suite_end();

    test_summary();
    return test_exit_code();
}
