/**
 * ENERGY CONSERVATION TEST
 *
 * THEORY: In a closed thermodynamic system, total energy is conserved.
 * E_total(t) = E_total(0) for all t > 0
 *
 * MATHEMATICAL MODEL:
 * Total energy = sum over all cells of sum over all materials of thermal_energy
 * E_total = Σ_cells Σ_materials thermal_energy[cell][material]
 *
 * TEST METHODOLOGY:
 * 1. Initialize a closed system with known total energy
 * 2. Run physics steps
 * 3. After EACH step, verify total energy equals initial energy
 * 4. Any deviation indicates energy creation or destruction (BUG)
 *
 * EXPECTED RESULT:
 * Energy should be exactly conserved (within floating point tolerance)
 * throughout the entire simulation.
 */

#include "test_common.h"
#include "chunk.h"
#include <math.h>
#include <string.h>

// Calculate total thermal energy by iterating over all chunks
static double calculate_total_energy(MatterSVO *svo) {
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

// Calculate total moles of a specific material
static double calculate_total_moles(MatterSVO *svo, MaterialType type) {
    double total = 0.0;
    for (int h = 0; h < CHUNK_HASH_SIZE; h++) {
        Chunk *chunk = svo->hash_table[h];
        while (chunk) {
            for (int i = 0; i < CHUNK_VOLUME; i++) {
                Cell3D *cell = &chunk->cells[i];
                if (CELL_HAS_MATERIAL(cell, type)) {
                    total += cell->materials[type].moles;
                }
            }
            chunk = chunk->hash_next;
        }
    }
    return total;
}

// Initialize minimal world for testing (vacuum - no materials)
static bool init_minimal_svo(MatterSVO *svo) {
    world_init(svo);
    return true;
}

// Add material with specific thermal energy (not temperature!)
static void add_matter(MatterSVO *svo, int cx, int cy, int cz,
                       MaterialType type, double moles, double thermal_energy) {
    Cell3D *cell = svo_get_cell_for_write(svo, cx, cy, cz);
    if (cell) {
        cell3d_add_material(cell, type, moles, thermal_energy);
        svo_mark_cell_active(svo, cx, cy, cz);
    }
}

/**
 * TEST: Energy conservation during heat conduction
 *
 * Setup: Two adjacent cells with different thermal energies
 * - Cell A: hot water (high thermal energy)
 * - Cell B: cold water (low thermal energy)
 *
 * Theory: Heat flows from hot to cold until equilibrium.
 * E_A + E_B = constant (energy just transfers, never created/destroyed)
 */
static bool test_energy_conservation_two_cells(void) {
    TEST_BEGIN("energy conservation: two cells heat conduction");

    MatterSVO svo;
    if (!init_minimal_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_SIZE / 2;
    int cz = SVO_SIZE / 2;

    // Cell A: 1 mol water with energy for ~350K (hot, below boiling)
    // Cell B: 1 mol water with energy for ~300K (room temp)
    // E = n * Cp * T, where Cp_water = 75.3 J/(mol·K)
    // Must stay above 273K to avoid freezing phase transition
    double moles = 1.0;
    double Cp_water = 75.3;  // J/(mol·K)
    double energy_A = moles * Cp_water * 350.0;  // ~26355 J at 350K
    double energy_B = moles * Cp_water * 300.0;  // ~22590 J at 300K

    add_matter(&svo, cx, cy, cz, MAT_WATER, moles, energy_A);
    add_matter(&svo, cx + 1, cy, cz, MAT_WATER, moles, energy_B);

    double initial_total = calculate_total_energy(&svo);
    double expected_total = energy_A + energy_B;

    printf("\n    Initial state:\n");
    printf("      Cell A energy: %.2f J\n", energy_A);
    printf("      Cell B energy: %.2f J\n", energy_B);
    printf("      Total energy: %.2f J\n", initial_total);
    printf("      Expected total: %.2f J\n", expected_total);

    // Tolerance for floating point errors
    double tolerance = expected_total * 0.001;  // 0.1%

    // Run physics and check conservation at each step
    for (int step = 0; step < 100; step++) {
        svo_physics_step(&svo, 0.016f);

        double current_total = calculate_total_energy(&svo);
        double deviation = fabs(current_total - expected_total);

        if (deviation > tolerance) {
            printf("    ENERGY VIOLATION at step %d:\n", step);
            printf("      Current total: %.2f J\n", current_total);
            printf("      Expected: %.2f J\n", expected_total);
            printf("      Deviation: %.2f J (%.2f%%)\n",
                   deviation, 100.0 * deviation / expected_total);

            svo_cleanup(&svo);
            TEST_FAIL("energy not conserved");
        }
    }

    double final_total = calculate_total_energy(&svo);
    printf("    After 100 steps:\n");
    printf("      Final total: %.2f J\n", final_total);
    printf("      Deviation: %.2f J (%.4f%%)\n",
           fabs(final_total - expected_total),
           100.0 * fabs(final_total - expected_total) / expected_total);

    svo_cleanup(&svo);
    TEST_PASS();
}

/**
 * TEST: Energy conservation during liquid flow
 *
 * Setup: Water with thermal energy flows downward
 *
 * Theory: Mass and energy are both conserved during flow.
 * When matter moves, its thermal energy moves with it.
 */
static bool test_energy_conservation_liquid_flow(void) {
    TEST_BEGIN("energy conservation: liquid flow");

    MatterSVO svo;
    if (!init_minimal_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_SIZE / 2 + 10;  // High up
    int cz = SVO_SIZE / 2;

    // Add water with energy corresponding to ~300K (above freezing)
    // E = n * Cp * T, Cp_water = 75.3 J/(mol·K)
    double moles = 5.0;
    double Cp_water = 75.3;
    double thermal_energy = moles * Cp_water * 300.0;  // ~112950 J at 300K

    add_matter(&svo, cx, cy, cz, MAT_WATER, moles, thermal_energy);

    double initial_energy = calculate_total_energy(&svo);
    double initial_water_moles = calculate_total_moles(&svo, MAT_WATER);

    printf("\n    Initial state:\n");
    printf("      Water moles: %.3f\n", initial_water_moles);
    printf("      Total energy: %.2f J\n", initial_energy);

    double tolerance = initial_energy * 0.01;  // 1%

    // Run physics
    for (int step = 0; step < 100; step++) {
        svo_physics_step(&svo, 0.016f);

        double current_energy = calculate_total_energy(&svo);
        double current_moles = calculate_total_moles(&svo, MAT_WATER);

        if (fabs(current_energy - initial_energy) > tolerance) {
            printf("    ENERGY VIOLATION at step %d:\n", step);
            printf("      Current energy: %.2f J (expected %.2f J)\n",
                   current_energy, initial_energy);
            svo_cleanup(&svo);
            TEST_FAIL("energy not conserved during flow");
        }

        if (fabs(current_moles - initial_water_moles) > 0.001) {
            printf("    MASS VIOLATION at step %d:\n", step);
            printf("      Current moles: %.3f (expected %.3f)\n",
                   current_moles, initial_water_moles);
            svo_cleanup(&svo);
            TEST_FAIL("mass not conserved during flow");
        }
    }

    double final_energy = calculate_total_energy(&svo);
    double final_moles = calculate_total_moles(&svo, MAT_WATER);

    printf("    After 100 steps:\n");
    printf("      Final water moles: %.3f\n", final_moles);
    printf("      Final energy: %.2f J\n", final_energy);
    printf("      Energy deviation: %.2f%%\n",
           100.0 * fabs(final_energy - initial_energy) / initial_energy);

    svo_cleanup(&svo);
    TEST_PASS();
}

/**
 * TEST: Energy not created from nothing
 *
 * Setup: Empty vacuum cell next to cell with matter
 *
 * Theory: Empty cells cannot gain energy from nowhere.
 * Energy can only transfer between cells that BOTH have matter.
 */
static bool test_no_energy_from_vacuum(void) {
    TEST_BEGIN("no energy creation from vacuum");

    MatterSVO svo;
    if (!init_minimal_svo(&svo)) { TEST_FAIL("init failed"); }

    int cx = SVO_SIZE / 2;
    int cy = SVO_SIZE / 2;
    int cz = SVO_SIZE / 2;

    // Only add matter to ONE cell, leave neighbors as vacuum
    // Use energy for ~300K (above freezing) to avoid phase transition
    double moles = 1.0;
    double Cp_water = 75.3;
    double thermal_energy = moles * Cp_water * 300.0;  // ~22590 J at 300K
    add_matter(&svo, cx, cy, cz, MAT_WATER, moles, thermal_energy);

    double initial_total = calculate_total_energy(&svo);

    printf("\n    Initial energy: %.2f J\n", initial_total);

    // Run physics - energy should not increase
    svo_physics_step(&svo, 0.016f);

    double after_total = calculate_total_energy(&svo);

    printf("    After 1 step: %.2f J\n", after_total);

    if (after_total > initial_total * 1.001) {
        printf("    ERROR: Energy INCREASED from %.2f to %.2f J!\n",
               initial_total, after_total);
        svo_cleanup(&svo);
        TEST_FAIL("energy created from vacuum");
    }

    svo_cleanup(&svo);
    TEST_PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("    ENERGY CONSERVATION TESTS\n");
    printf("========================================\n");
    printf("Theory: Total energy must be conserved\n");
    printf("        E_total(t) = E_total(0) for all t\n");
    printf("========================================\n\n");

    int passed = 0, failed = 0;

    if (test_energy_conservation_two_cells()) passed++; else failed++;
    if (test_energy_conservation_liquid_flow()) passed++; else failed++;
    if (test_no_energy_from_vacuum()) passed++; else failed++;

    printf("\n========================================\n");
    printf("    RESULTS: %d/%d tests passed\n", passed, passed + failed);
    if (failed > 0) {
        printf("    %d TESTS FAILED - ENERGY IS NOT CONSERVED!\n", failed);
    } else {
        printf("    ALL TESTS PASSED - ENERGY IS CONSERVED\n");
    }
    printf("========================================\n\n");

    return failed > 0 ? 1 : 0;
}
