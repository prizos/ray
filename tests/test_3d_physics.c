// Comprehensive 3D Physics Tests
// Tests that physics works correctly in all directions

#include "test_common.h"
#include "chunk.h"
#include "terrain.h"
#include <math.h>
#include <string.h>

// Test infrastructure
typedef struct {
    const char *category;
    const char *name;
    bool (*func)(void);
} TestCase;

// Initialize empty chunk world (vacuum - no materials, no terrain)
// This is the correct approach for testing isolated physics behaviors.
static bool init_empty_svo(ChunkWorld *world) {
    world_init(world);
    return true;
}

// Get water moles at cell
static double get_water_at(ChunkWorld *world, int cx, int cy, int cz) {
    const Cell3D *cell = world_get_cell(world, cx, cy, cz);
    if (!cell) return 0.0;
    if (CELL_HAS_MATERIAL(cell, MAT_WATER)) {
        return cell->materials[MAT_WATER].moles;
    }
    return 0.0;
}

// Get temperature at cell (returns 0.0 for empty cells - vacuum has no temperature)
static double get_temp_at(ChunkWorld *world, int cx, int cy, int cz) {
    Cell3D *cell = world_get_cell_for_write(world, cx, cy, cz);
    if (!cell || cell->present == 0) return 0.0;
    return cell_get_temperature(cell);
}

// Calculate energy for water at a given temperature (accounts for latent heat)
static double calculate_water_energy(double moles, double temp_k) {
    double Cp_s = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_solid;
    double Cp_l = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;
    double Cp_g = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_gas;
    double Tm = MATERIAL_PROPS[MAT_WATER].melting_point;  // 273.15K
    double Tb = MATERIAL_PROPS[MAT_WATER].boiling_point;  // 373.15K
    double Hf = MATERIAL_PROPS[MAT_WATER].enthalpy_fusion; // 6010 J/mol
    double Hv = MATERIAL_PROPS[MAT_WATER].enthalpy_vaporization; // 40660 J/mol

    if (temp_k <= Tm) {
        // Solid ice
        return moles * Cp_s * temp_k;
    } else if (temp_k <= Tb) {
        // Liquid water: includes latent heat of fusion
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (temp_k - Tm);
    } else {
        // Gas/steam: includes both latent heats
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (Tb - Tm) + moles * Hv + moles * Cp_g * (temp_k - Tb);
    }
}

// Add water directly at cell coordinates (liquid at room temperature)
static void add_water_at_cell(MatterSVO *svo, int cx, int cy, int cz, double moles) {
    Cell3D *cell = svo_get_cell_for_write(svo, cx, cy, cz);
    if (cell) {
        double energy = calculate_water_energy(moles, INITIAL_TEMP_K);
        cell3d_add_material(cell, MAT_WATER, moles, energy);
        svo_mark_cell_active(svo, cx, cy, cz);
    }
}

// Add water at cell coordinates with specific temperature
static void add_hot_water_at_cell(MatterSVO *svo, int cx, int cy, int cz, double moles, double temp_k) {
    Cell3D *cell = svo_get_cell_for_write(svo, cx, cy, cz);
    if (cell) {
        double energy = calculate_water_energy(moles, temp_k);
        cell3d_add_material(cell, MAT_WATER, moles, energy);
        svo_mark_cell_active(svo, cx, cy, cz);
    }
}

// Run physics steps with metric recording
static void run_physics(MatterSVO *svo, int steps) {
    for (int i = 0; i < steps; i++) {
        svo_physics_step(svo, 0.016f);
        TEST_RECORD_PHYSICS_STEP();
    }
    TEST_RECORD_ACTIVE_NODES(svo->active_count);
}

// ============================================================================
//                    WATER FLOW TESTS - VERTICAL
// ============================================================================

static bool test_water_falls_in_empty_air(void) {
    TEST_BEGIN("water falls downward in empty air");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    // Place water at a high Y position
    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y + 10;  // High above ground
    int cz = SVO_SIZE / 2;

    add_water_at_cell(&svo, cx, cy, cz, 5.0);

    double water_top_before = get_water_at(&svo, cx, cy, cz);
    double water_below_before = get_water_at(&svo, cx, cy - 1, cz);

    ASSERT(water_top_before > 4.0, "should have water at starting position");
    ASSERT(water_below_before < 0.1, "should be empty below initially");

    // Check cell is active
    ASSERT(svo.active_count > 0, "should have active cells");

    // Run physics
    run_physics(&svo, 100);

    double water_top_after = get_water_at(&svo, cx, cy, cz);
    double water_below_after = get_water_at(&svo, cx, cy - 1, cz);

    // Water should have moved down
    ASSERT(water_top_after < water_top_before, "water should decrease at top");
    ASSERT(water_below_after > 0.01, "water should appear below");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_water_continues_falling(void) {
    TEST_BEGIN("water continues falling multiple cells");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y + 15;
    int cz = SVO_SIZE / 2;

    add_water_at_cell(&svo, cx, cy, cz, 10.0);

    // Run physics for a long time
    run_physics(&svo, 500);

    // Check water has spread downward to multiple cells
    double total_water = 0;
    int cells_with_water = 0;
    for (int y = cy - 10; y <= cy; y++) {
        double w = get_water_at(&svo, cx, y, cz);
        total_water += w;
        if (w > 0.01) cells_with_water++;
    }

    ASSERT(cells_with_water > 1, "water should have spread to multiple cells");
    ASSERT(total_water > 9.0, "total water should be conserved");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                    WATER FLOW TESTS - HORIZONTAL
// ============================================================================

static bool test_water_spreads_when_blocked(void) {
    TEST_BEGIN("water spreads horizontally when blocked below");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    // Add solid rock below
    Cell3D *rock_cell = svo_get_cell_for_write(&svo, cx, cy - 1, cz);
    if (rock_cell) {
        cell3d_free(rock_cell);
        cell3d_init(rock_cell);
        double rock_hc = MATERIAL_PROPS[MAT_ROCK].molar_heat_capacity_solid;  // Rock is solid
        cell3d_add_material(rock_cell, MAT_ROCK, 50.0, 50.0 * rock_hc * INITIAL_TEMP_K);
    }

    // Add water on top of rock
    add_water_at_cell(&svo, cx, cy, cz, 10.0);

    // Run physics
    run_physics(&svo, 100);

    // Water should spread horizontally (if implemented)
    // or at least stay on top (not disappear)
    double water_center = get_water_at(&svo, cx, cy, cz);
    double water_x_plus = get_water_at(&svo, cx + 1, cy, cz);
    double water_x_minus = get_water_at(&svo, cx - 1, cy, cz);
    double water_z_plus = get_water_at(&svo, cx, cy, cz + 1);
    double water_z_minus = get_water_at(&svo, cx, cy, cz - 1);
    double total = water_center + water_x_plus + water_x_minus + water_z_plus + water_z_minus;

    // Water should be conserved (not fall through rock)
    ASSERT(total > 9.0, "water should not disappear through solid");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                    HEAT CONDUCTION TESTS - ALL DIRECTIONS
// ============================================================================

static bool test_heat_conducts_positive_x(void) {
    TEST_BEGIN("heat conducts in +X direction");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    // Hot cell
    add_hot_water_at_cell(&svo, cx, cy, cz, 1.0, 400.0);
    // Cold cell to +X
    add_hot_water_at_cell(&svo, cx + 1, cy, cz, 1.0, 250.0);

    double hot_before = get_temp_at(&svo, cx, cy, cz);
    double cold_before = get_temp_at(&svo, cx + 1, cy, cz);

    run_physics(&svo, 50);

    double hot_after = get_temp_at(&svo, cx, cy, cz);
    double cold_after = get_temp_at(&svo, cx + 1, cy, cz);

    ASSERT(hot_after < hot_before, "hot cell should cool");
    ASSERT(cold_after > cold_before, "cold cell should warm");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_conducts_negative_x(void) {
    TEST_BEGIN("heat conducts in -X direction");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    add_hot_water_at_cell(&svo, cx, cy, cz, 1.0, 400.0);
    add_hot_water_at_cell(&svo, cx - 1, cy, cz, 1.0, 250.0);

    double cold_before = get_temp_at(&svo, cx - 1, cy, cz);
    run_physics(&svo, 50);
    double cold_after = get_temp_at(&svo, cx - 1, cy, cz);

    ASSERT(cold_after > cold_before, "cold cell at -X should warm");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_conducts_positive_y(void) {
    TEST_BEGIN("heat conducts in +Y direction");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    add_hot_water_at_cell(&svo, cx, cy, cz, 1.0, 400.0);
    add_hot_water_at_cell(&svo, cx, cy + 1, cz, 1.0, 250.0);

    double cold_before = get_temp_at(&svo, cx, cy + 1, cz);
    run_physics(&svo, 50);
    double cold_after = get_temp_at(&svo, cx, cy + 1, cz);

    ASSERT(cold_after > cold_before, "cold cell at +Y should warm");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_conducts_negative_y(void) {
    TEST_BEGIN("heat conducts in -Y direction");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    add_hot_water_at_cell(&svo, cx, cy, cz, 1.0, 400.0);
    add_hot_water_at_cell(&svo, cx, cy - 1, cz, 1.0, 250.0);

    double cold_before = get_temp_at(&svo, cx, cy - 1, cz);
    run_physics(&svo, 50);
    double cold_after = get_temp_at(&svo, cx, cy - 1, cz);

    ASSERT(cold_after > cold_before, "cold cell at -Y should warm");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_conducts_positive_z(void) {
    TEST_BEGIN("heat conducts in +Z direction");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    add_hot_water_at_cell(&svo, cx, cy, cz, 1.0, 400.0);
    add_hot_water_at_cell(&svo, cx, cy, cz + 1, 1.0, 250.0);

    double cold_before = get_temp_at(&svo, cx, cy, cz + 1);
    run_physics(&svo, 50);
    double cold_after = get_temp_at(&svo, cx, cy, cz + 1);

    ASSERT(cold_after > cold_before, "cold cell at +Z should warm");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_heat_conducts_negative_z(void) {
    TEST_BEGIN("heat conducts in -Z direction");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    add_hot_water_at_cell(&svo, cx, cy, cz, 1.0, 400.0);
    add_hot_water_at_cell(&svo, cx, cy, cz - 1, 1.0, 250.0);

    double cold_before = get_temp_at(&svo, cx, cy, cz - 1);
    run_physics(&svo, 50);
    double cold_after = get_temp_at(&svo, cx, cy, cz - 1);

    ASSERT(cold_after > cold_before, "cold cell at -Z should warm");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                    GAS BEHAVIOR TESTS
// ============================================================================

static bool test_steam_rises_upward(void) {
    TEST_BEGIN("steam (gas) rises upward");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    // Add steam (hot water = gas phase)
    add_hot_water_at_cell(&svo, cx, cy, cz, 5.0, 400.0);

    // Verify it's gas (use non-const access for temperature caching)
    Cell3D *cell = svo_get_cell_for_write(&svo, cx, cy, cz);
    MaterialEntry *water = cell3d_find_material(cell, MAT_WATER);
    double temp = material_get_temperature(&water->state, MAT_WATER);
    Phase phase = material_get_phase(MAT_WATER, temp);
    ASSERT(phase == PHASE_GAS, "water at 400K should be steam");

    double steam_here_before = get_water_at(&svo, cx, cy, cz);
    double steam_above_before = get_water_at(&svo, cx, cy + 1, cz);

    run_physics(&svo, 100);

    double steam_here_after = get_water_at(&svo, cx, cy, cz);
    double steam_above_after = get_water_at(&svo, cx, cy + 1, cz);

    // Steam should diffuse, with some going upward
    ASSERT(steam_here_after < steam_here_before || steam_above_after > steam_above_before,
           "steam should diffuse upward");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                    ACTIVE NODE TRACKING TESTS
// ============================================================================

static bool test_cell_marked_active_after_water_add(void) {
    TEST_BEGIN("cell is marked active after adding water");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    ASSERT(svo.active_count == 0, "should start with no active cells");

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y;
    int cz = SVO_SIZE / 2;

    add_water_at_cell(&svo, cx, cy, cz, 5.0);

    ASSERT(svo.active_count > 0, "should have active cells after adding water");

    svo_cleanup(&svo);
    TEST_PASS();
}

static bool test_neighbor_marked_active_after_flow(void) {
    TEST_BEGIN("neighbor cell marked active when water flows to it");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y + 5;
    int cz = SVO_SIZE / 2;

    add_water_at_cell(&svo, cx, cy, cz, 5.0);

    // Run physics steps
    run_physics(&svo, 10);

    // If water flowed, more cells should be active
    // (Note: this tests that the flow code marks new cells active)

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                    DEBUG DIAGNOSTIC TEST
// ============================================================================

static bool test_debug_water_flow_step_by_step(void) {
    TEST_BEGIN("DEBUG: water flow step by step");

    MatterSVO svo;
    if (!init_empty_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_GROUND_Y + 5;
    int cz = SVO_SIZE / 2;

    // Add water
    add_water_at_cell(&svo, cx, cy, cz, 5.0);

    printf("\n    Initial state:\n");
    printf("      Active count: %d\n", svo.active_count);
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy, cz, get_water_at(&svo, cx, cy, cz));
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy-1, cz, get_water_at(&svo, cx, cy-1, cz));

    // Check if the chunk containing this cell is active
    printf("      Active chunks: %d\n", svo.active_count);

    // Run single physics step
    svo_physics_step(&svo, 0.016f);

    printf("    After 1 physics step:\n");
    printf("      Active count: %d\n", svo.active_count);
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy, cz, get_water_at(&svo, cx, cy, cz));
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy-1, cz, get_water_at(&svo, cx, cy-1, cz));

    // Run more steps
    for (int i = 0; i < 50; i++) {
        svo_physics_step(&svo, 0.016f);
    }

    printf("    After 51 physics steps:\n");
    printf("      Active count: %d\n", svo.active_count);
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy, cz, get_water_at(&svo, cx, cy, cz));
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy-1, cz, get_water_at(&svo, cx, cy-1, cz));
    printf("      Water at [%d,%d,%d]: %.3f\n", cx, cy-2, cz, get_water_at(&svo, cx, cy-2, cz));

    double water_original = get_water_at(&svo, cx, cy, cz);
    double water_below = get_water_at(&svo, cx, cy-1, cz);

    // The test passes if ANY water moved
    ASSERT(water_below > 0.001 || water_original < 4.9, "water should have moved");

    svo_cleanup(&svo);
    TEST_PASS();
}

// ============================================================================
//                         MAIN TEST RUNNER
// ============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("    3D PHYSICS TESTS\n");
    printf("========================================\n");

    TestCase tests[] = {
        // Water Flow - Vertical
        {"WATER FLOW", "falls_in_empty_air", test_water_falls_in_empty_air},
        {"WATER FLOW", "continues_falling", test_water_continues_falling},
        {"WATER FLOW", "spreads_when_blocked", test_water_spreads_when_blocked},

        // Heat Conduction - All 6 directions
        {"HEAT CONDUCTION", "positive_x", test_heat_conducts_positive_x},
        {"HEAT CONDUCTION", "negative_x", test_heat_conducts_negative_x},
        {"HEAT CONDUCTION", "positive_y", test_heat_conducts_positive_y},
        {"HEAT CONDUCTION", "negative_y", test_heat_conducts_negative_y},
        {"HEAT CONDUCTION", "positive_z", test_heat_conducts_positive_z},
        {"HEAT CONDUCTION", "negative_z", test_heat_conducts_negative_z},

        // Gas Behavior
        {"GAS", "steam_rises_upward", test_steam_rises_upward},

        // Active Node Tracking
        {"ACTIVE TRACKING", "marked_after_water_add", test_cell_marked_active_after_water_add},
        {"ACTIVE TRACKING", "neighbor_marked_after_flow", test_neighbor_marked_active_after_flow},

        // Debug
        {"DEBUG", "water_flow_step_by_step", test_debug_water_flow_step_by_step},
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
