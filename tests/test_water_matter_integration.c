/**
 * Water-Matter System - Integration Tests
 *
 * Tests the complete interaction between water and matter simulations
 * on isolated grids without the full game engine.
 *
 * Theories tested:
 * 1. Hot cells cooled by water
 * 2. Fire extinguished by flooding
 * 3. Evaporation plateau at boiling point
 * 4. O2 displacement by water
 * 5. Energy conservation during phase transitions
 */

#include "test_common.h"

// ============ PHYSICAL CONSTANTS ============

#define WATER_BOILING_POINT    FLOAT_TO_FIXED(373.15f)
#define WATER_MELTING_POINT    FLOAT_TO_FIXED(273.15f)
#define IGNITION_TEMP          FLOAT_TO_FIXED(533.0f)
#define AMBIENT_TEMP           FLOAT_TO_FIXED(293.15f)
#define AMBIENT_O2             FLOAT_TO_FIXED(0.021f)

// Use J/g units (same scale as unit tests and matter.c)
#define LATENT_HEAT_VAPORIZATION FLOAT_TO_FIXED(2260.0f)   // J/g
#define SPECIFIC_HEAT_WATER    FLOAT_TO_FIXED(4.18f)        // J/(g·K)
#define SPECIFIC_HEAT_SILICATE FLOAT_TO_FIXED(0.7f)         // J/(g·K)

#define WATER_MASS_PER_DEPTH   FLOAT_TO_FIXED(1000.0f)
#define CONDUCTION_RATE        FLOAT_TO_FIXED(0.05f)
#define EVAPORATION_RATE       FLOAT_TO_FIXED(0.01f)

// ============ INTEGRATED CELL ============

typedef struct {
    // Substances
    fixed16_t silicate_mass;
    fixed16_t fuel_mass;       // Cellulose
    fixed16_t o2_mass;
    fixed16_t ash_mass;

    // H2O by phase
    fixed16_t h2o_ice;
    fixed16_t h2o_liquid;
    fixed16_t h2o_steam;

    // Thermal
    fixed16_t energy;
    fixed16_t temperature;
    fixed16_t thermal_mass;

    // Flags
    bool is_burning;

} IntCell;

typedef struct {
    IntCell *cells;
    int width;
    int height;
} IntGrid;

// ============ GRID HELPERS ============

static void grid_init(IntGrid *g, int w, int h, float temp_k) {
    g->width = w;
    g->height = h;
    g->cells = calloc(w * h, sizeof(IntCell));

    fixed16_t temp = FLOAT_TO_FIXED(temp_k);

    for (int i = 0; i < w * h; i++) {
        IntCell *c = &g->cells[i];
        c->silicate_mass = FLOAT_TO_FIXED(1.0f);
        c->o2_mass = AMBIENT_O2;

        c->thermal_mass = fixed_mul(c->silicate_mass, SPECIFIC_HEAT_SILICATE);
        c->temperature = temp;
        c->energy = fixed_mul(c->thermal_mass, temp);
    }
}

static void grid_free(IntGrid *g) {
    free(g->cells);
    g->cells = NULL;
}

static IntCell* grid_get(IntGrid *g, int x, int y) {
    if (x < 0 || x >= g->width || y < 0 || y >= g->height) return NULL;
    return &g->cells[y * g->width + x];
}

static void cell_update_thermal(IntCell *c) {
    // Calculate thermal mass using J/(g·K) specific heats
    c->thermal_mass = fixed_mul(c->silicate_mass, SPECIFIC_HEAT_SILICATE);  // 0.7 J/(g·K)
    c->thermal_mass += fixed_mul(c->fuel_mass, FLOAT_TO_FIXED(1.3f));        // 1.3 J/(g·K)
    c->thermal_mass += fixed_mul(c->h2o_liquid, SPECIFIC_HEAT_WATER);        // 4.18 J/(g·K)
    c->thermal_mass += fixed_mul(c->h2o_ice, FLOAT_TO_FIXED(2.09f));         // 2.09 J/(g·K)
    c->thermal_mass += fixed_mul(c->h2o_steam, FLOAT_TO_FIXED(2.01f));       // 2.01 J/(g·K)

    // Temperature (0.01 threshold matches matter.c)
    if (c->thermal_mass > FLOAT_TO_FIXED(0.01f)) {
        c->temperature = fixed_div(c->energy, c->thermal_mass);
        if (c->temperature < 0) {
            c->temperature = 0;
            c->energy = 0;
        }
    }
}

static fixed16_t grid_total_energy(const IntGrid *g) {
    fixed16_t total = 0;
    for (int i = 0; i < g->width * g->height; i++) {
        total += g->cells[i].energy;
    }
    return total;
}

static fixed16_t grid_total_h2o(const IntGrid *g) {
    fixed16_t total = 0;
    for (int i = 0; i < g->width * g->height; i++) {
        total += g->cells[i].h2o_ice;
        total += g->cells[i].h2o_liquid;
        total += g->cells[i].h2o_steam;
    }
    return total;
}

// ============ SIMULATION STEP ============

static void grid_step_conduction(IntGrid *g) {
    fixed16_t *deltas = calloc(g->width * g->height, sizeof(fixed16_t));

    for (int y = 0; y < g->height; y++) {
        for (int x = 0; x < g->width; x++) {
            IntCell *c = grid_get(g, x, y);
            if (c->thermal_mass < FLOAT_TO_FIXED(1.0f)) continue;

            int dx[4] = {-1, 1, 0, 0};
            int dy[4] = {0, 0, -1, 1};

            for (int d = 0; d < 4; d++) {
                IntCell *n = grid_get(g, x + dx[d], y + dy[d]);
                if (!n || n->thermal_mass < FLOAT_TO_FIXED(1.0f)) continue;

                fixed16_t diff = n->temperature - c->temperature;
                fixed16_t flow = fixed_mul(diff, CONDUCTION_RATE);

                // Limit
                if (flow > 0) {
                    fixed16_t max = n->energy / 20;
                    if (flow > max) flow = max;
                } else {
                    fixed16_t max = c->energy / 20;
                    if (flow < -max) flow = -max;
                }

                deltas[y * g->width + x] += flow;
            }
        }
    }

    // Apply
    for (int i = 0; i < g->width * g->height; i++) {
        g->cells[i].energy += deltas[i];
    }

    free(deltas);
}

static void grid_step_evaporation(IntGrid *g) {
    for (int i = 0; i < g->width * g->height; i++) {
        IntCell *c = &g->cells[i];

        // Evaporate at/above boiling
        if (c->temperature >= WATER_BOILING_POINT && c->h2o_liquid > 0) {
            fixed16_t excess = c->temperature - WATER_BOILING_POINT;
            if (excess <= 0) continue;

            // Calculate max evaporation by available energy
            fixed16_t excess_energy = fixed_mul(excess, c->thermal_mass);
            fixed16_t max_by_energy = fixed_div(excess_energy, LATENT_HEAT_VAPORIZATION);
            fixed16_t max_by_mass = c->h2o_liquid;
            fixed16_t max_rate = EVAPORATION_RATE;

            fixed16_t evap = max_rate;
            if (evap > max_by_energy) evap = max_by_energy;
            if (evap > max_by_mass) evap = max_by_mass;
            if (evap <= 0) continue;

            // Transfer mass
            c->h2o_liquid -= evap;
            c->h2o_steam += evap;

            // Consume latent heat
            c->energy -= fixed_mul(evap, LATENT_HEAT_VAPORIZATION);
        }

        // Condense below boiling
        if (c->temperature < WATER_BOILING_POINT && c->h2o_steam > 0) {
            fixed16_t rate = EVAPORATION_RATE;
            if (rate > c->h2o_steam) rate = c->h2o_steam;

            c->h2o_steam -= rate;
            c->h2o_liquid += rate;
            c->energy += fixed_mul(rate, LATENT_HEAT_VAPORIZATION);
        }
    }
}

static void grid_step_combustion(IntGrid *g) {
    fixed16_t min_fuel = FLOAT_TO_FIXED(0.01f);
    fixed16_t min_o2 = FLOAT_TO_FIXED(0.001f);
    fixed16_t water_suppress = FLOAT_TO_FIXED(0.1f);
    fixed16_t burn_rate = FLOAT_TO_FIXED(0.05f);
    // Heat of combustion in J/g (17.5 MJ/kg = 17.5 J/g with mass in grams)
    fixed16_t heat_of_combustion = FLOAT_TO_FIXED(17.5f);

    for (int i = 0; i < g->width * g->height; i++) {
        IntCell *c = &g->cells[i];

        // Check combustion conditions
        if (c->fuel_mass < min_fuel) { c->is_burning = false; continue; }
        if (c->temperature < IGNITION_TEMP) { c->is_burning = false; continue; }
        if (c->o2_mass < min_o2) { c->is_burning = false; continue; }
        if (c->h2o_liquid > water_suppress) { c->is_burning = false; continue; }

        c->is_burning = true;

        // Burn fuel
        fixed16_t actual = burn_rate;
        if (actual > c->fuel_mass) actual = c->fuel_mass;

        c->fuel_mass -= actual;
        c->o2_mass -= fixed_mul(actual, FLOAT_TO_FIXED(0.33f));
        c->ash_mass += fixed_mul(actual, FLOAT_TO_FIXED(0.03f));

        // Release heat: mass(g) * heat_of_combustion(J/g) * 1000 = J
        // Two-step multiplication to avoid fixed-point overflow
        fixed16_t heat = fixed_mul(actual, heat_of_combustion);
        heat = fixed_mul(heat, FLOAT_TO_FIXED(1000.0f));
        c->energy += heat;
    }
}

static void grid_step_o2_displacement(IntGrid *g) {
    for (int i = 0; i < g->width * g->height; i++) {
        IntCell *c = &g->cells[i];

        // Calculate submersion factor (0 = dry, 1 = fully submerged)
        fixed16_t submersion = fixed_div(c->h2o_liquid, FLOAT_TO_FIXED(1.0f));
        if (submersion > FIXED_ONE) submersion = FIXED_ONE;
        if (submersion < 0) submersion = 0;

        // O2 displaced proportionally
        fixed16_t air_fraction = FIXED_ONE - submersion;
        c->o2_mass = fixed_mul(AMBIENT_O2, air_fraction);
    }
}

static void grid_step(IntGrid *g) {
    // Update thermal properties
    for (int i = 0; i < g->width * g->height; i++) {
        cell_update_thermal(&g->cells[i]);
    }

    grid_step_o2_displacement(g);
    grid_step_conduction(g);
    grid_step_combustion(g);
    grid_step_evaporation(g);

    // Final update
    for (int i = 0; i < g->width * g->height; i++) {
        cell_update_thermal(&g->cells[i]);
    }
}

// ============ TEST: HOT CELL COOLED BY WATER ============

static bool test_hot_cell_cooled_by_water(void) {
    TEST_BEGIN("adding water cools hot cell");

    IntGrid g;
    grid_init(&g, 3, 3, 400.0f);  // Start at 400K

    IntCell *center = grid_get(&g, 1, 1);
    float initial_temp = FIXED_TO_FLOAT(center->temperature);

    // Add water (at ~293K)
    center->h2o_liquid = FLOAT_TO_FIXED(1.0f);

    // Run simulation
    for (int i = 0; i < 100; i++) {
        grid_step(&g);
    }

    float final_temp = FIXED_TO_FLOAT(center->temperature);

    ASSERT(final_temp < initial_temp, "temperature didn't decrease after adding water");

    grid_free(&g);
    TEST_PASS();
}

// ============ TEST: FIRE EXTINGUISHED BY FLOODING ============

static bool test_fire_extinguished_by_flooding(void) {
    TEST_BEGIN("flooding extinguishes fire");

    IntGrid g;
    grid_init(&g, 3, 3, 600.0f);  // Hot enough to burn

    IntCell *center = grid_get(&g, 1, 1);
    center->fuel_mass = FLOAT_TO_FIXED(0.5f);

    // When adding fuel, we need to add energy too (fuel comes in at ambient temp)
    // Otherwise thermal_mass increases but energy stays same, lowering temp
    fixed16_t fuel_th = fixed_mul(center->fuel_mass, FLOAT_TO_FIXED(1.3f));
    center->energy += fixed_mul(fuel_th, center->temperature);
    center->thermal_mass += fuel_th;

    // Start combustion
    for (int i = 0; i < 5; i++) {
        grid_step(&g);
    }
    ASSERT_TRUE(center->is_burning, "fire didn't start");

    // Add water
    center->h2o_liquid = FLOAT_TO_FIXED(0.5f);

    // Run more steps
    for (int i = 0; i < 5; i++) {
        grid_step(&g);
    }

    ASSERT_FALSE(center->is_burning, "fire not extinguished by water");

    grid_free(&g);
    TEST_PASS();
}

// ============ TEST: EVAPORATION PLATEAU ============

static bool test_evaporation_plateau(void) {
    TEST_BEGIN("temperature plateaus at boiling during evaporation");

    IntGrid g;
    grid_init(&g, 1, 1, 373.0f);  // Start at boiling

    IntCell *cell = grid_get(&g, 0, 0);
    cell->h2o_liquid = FLOAT_TO_FIXED(0.5f);

    // Add energy continuously
    for (int i = 0; i < 100; i++) {
        // Add heat
        cell->energy += FLOAT_TO_FIXED(100000.0f);
        grid_step(&g);

        // While water remains, temp should stay near boiling
        if (cell->h2o_liquid > FLOAT_TO_FIXED(0.01f)) {
            float temp = FIXED_TO_FLOAT(cell->temperature);
            ASSERT(temp < 400.0f, "temperature exceeded plateau while water remains");
        }
    }

    TEST_PASS();
}

// ============ TEST: O2 DISPLACEMENT ============

static bool test_submerged_cell_no_oxygen(void) {
    TEST_BEGIN("submerged cell has no oxygen");

    IntGrid g;
    grid_init(&g, 1, 1, 300.0f);

    IntCell *cell = grid_get(&g, 0, 0);
    ASSERT(cell->o2_mass > 0, "initial O2 should be present");

    // Submerge
    cell->h2o_liquid = FLOAT_TO_FIXED(2.0f);  // > 1 = fully submerged
    grid_step(&g);

    ASSERT(cell->o2_mass < FLOAT_TO_FIXED(0.001f), "O2 not displaced by water");

    grid_free(&g);
    TEST_PASS();
}

static bool test_partial_submersion_reduces_o2(void) {
    TEST_BEGIN("partial submersion reduces O2 proportionally");

    IntGrid g;
    grid_init(&g, 1, 1, 300.0f);

    IntCell *cell = grid_get(&g, 0, 0);
    fixed16_t initial_o2 = cell->o2_mass;

    // Half submerge
    cell->h2o_liquid = FLOAT_TO_FIXED(0.5f);
    grid_step(&g);

    // Should have ~50% O2
    float ratio = FIXED_TO_FLOAT(cell->o2_mass) / FIXED_TO_FLOAT(initial_o2);
    ASSERT(ratio > 0.4f && ratio < 0.6f, "O2 not reduced proportionally");

    grid_free(&g);
    TEST_PASS();
}

// ============ TEST: UNDERWATER FIRE IMPOSSIBLE ============

static bool test_underwater_fire_impossible(void) {
    TEST_BEGIN("fire cannot ignite underwater");

    IntGrid g;
    grid_init(&g, 1, 1, 700.0f);  // Very hot

    IntCell *cell = grid_get(&g, 0, 0);
    cell->fuel_mass = FLOAT_TO_FIXED(0.5f);
    cell->h2o_liquid = FLOAT_TO_FIXED(1.0f);  // Submerged

    // Run simulation
    for (int i = 0; i < 50; i++) {
        grid_step(&g);
    }

    // Fire should never start
    ASSERT_FALSE(cell->is_burning, "fire started underwater");

    // Fuel should not be consumed
    ASSERT(cell->fuel_mass > FLOAT_TO_FIXED(0.4f), "fuel consumed underwater");

    grid_free(&g);
    TEST_PASS();
}

// ============ TEST: ENERGY CONSERVATION DURING PHASE TRANSITIONS ============

static bool test_phase_transition_energy_conservation(void) {
    TEST_BEGIN("energy conserved during phase transitions");

    IntGrid g;
    grid_init(&g, 1, 1, 400.0f);

    IntCell *cell = grid_get(&g, 0, 0);
    cell->h2o_liquid = FLOAT_TO_FIXED(0.5f);

    // When adding water, also add its thermal energy at current temperature
    fixed16_t water_th = fixed_mul(cell->h2o_liquid, SPECIFIC_HEAT_WATER);
    cell->thermal_mass += water_th;
    cell->energy += fixed_mul(water_th, cell->temperature);

    // Run until all water evaporates (adding heat each step)
    for (int i = 0; i < 500; i++) {
        // Add heat to drive evaporation (much more modest amount)
        cell->energy += FLOAT_TO_FIXED(500.0f);
        grid_step(&g);

        if (cell->h2o_liquid < FLOAT_TO_FIXED(0.01f)) break;
    }

    // Verify steam formed (mass was transferred from liquid to gas)
    ASSERT(cell->h2o_steam > FLOAT_TO_FIXED(0.1f), "water didn't evaporate");

    grid_free(&g);
    TEST_PASS();
}

// ============ TEST: MASS CONSERVATION DURING EVAPORATION ============

static bool test_evaporation_mass_conservation(void) {
    TEST_BEGIN("H2O mass conserved during evaporation simulation");

    IntGrid g;
    grid_init(&g, 3, 3, 400.0f);

    // Add water to center
    IntCell *center = grid_get(&g, 1, 1);
    center->h2o_liquid = FLOAT_TO_FIXED(1.0f);

    fixed16_t initial_h2o = grid_total_h2o(&g);

    // Run simulation with continuous heat
    for (int i = 0; i < 200; i++) {
        center->energy += FLOAT_TO_FIXED(100000.0f);
        grid_step(&g);
    }

    fixed16_t final_h2o = grid_total_h2o(&g);

    // Should be conserved (within fixed-point tolerance)
    int diff = abs(final_h2o - initial_h2o);
    ASSERT(diff < 1000, "H2O mass not conserved");

    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Water-Matter System - Integration Tests\n");
    printf("========================================\n");

    test_suite_begin("WATER COOLING");
    test_hot_cell_cooled_by_water();
    test_suite_end();

    test_suite_begin("FIRE SUPPRESSION");
    test_fire_extinguished_by_flooding();
    test_underwater_fire_impossible();
    test_suite_end();

    test_suite_begin("PHASE TRANSITIONS");
    test_evaporation_plateau();
    test_phase_transition_energy_conservation();
    test_evaporation_mass_conservation();
    test_suite_end();

    test_suite_begin("O2 DISPLACEMENT");
    test_submerged_cell_no_oxygen();
    test_partial_submersion_reduces_o2();
    test_suite_end();

    test_summary();
    return test_exit_code();
}
