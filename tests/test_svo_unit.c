/**
 * SVO Matter System - Unit Tests
 *
 * Tests individual functions in complete isolation.
 * No full simulation - just pure function testing.
 *
 * Theories tested:
 * 1. Cell3D operations work correctly (init, add, clone, free)
 * 2. Temperature calculation: T = thermal_energy / (moles * molar_heat_capacity)
 * 3. Material property lookups return correct values
 * 4. Coordinate conversions are correct and reversible
 * 5. SVO node operations work correctly
 */

// Include chunk.h for the new chunk-based system
#include "chunk.h"
#include "test_common.h"
#include <string.h>

// Test runner macro
#define RUN_TEST(test_fn) \
    do { \
        if (test_fn()) { passed++; } else { failed++; } \
    } while(0)

// ============ ENERGY CALCULATION HELPER ============

// Calculate energy for any material at a given temperature (accounts for latent heat)
// Uses phase-specific heat capacities
static double calculate_material_energy(MaterialType type, double moles, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double Cp_s = props->molar_heat_capacity_solid;
    double Cp_l = props->molar_heat_capacity_liquid;
    double Cp_g = props->molar_heat_capacity_gas;
    double Tm = props->melting_point;
    double Tb = props->boiling_point;
    double Hf = props->enthalpy_fusion;
    double Hv = props->enthalpy_vaporization;

    if (temp_k <= Tm) {
        // Solid
        return moles * Cp_s * temp_k;
    } else if (temp_k <= Tb) {
        // Liquid: includes latent heat of fusion
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (temp_k - Tm);
    } else {
        // Gas: includes both latent heats
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (Tb - Tm) + moles * Hv + moles * Cp_g * (temp_k - Tb);
    }
}

// ============ CELL3D OPERATION TESTS ============

static bool test_cell3d_init(void) {
    TEST_BEGIN("cell3d_init creates empty cell");

    Cell3D cell;
    cell3d_init(&cell);

    ASSERT(CELL_MATERIAL_COUNT(&cell) == 0, "material_count should be 0");
    ASSERT(cell.present == 0, "present bitmask should be 0");

    cell3d_free(&cell);
    TEST_PASS();
}

static bool test_cell3d_add_material(void) {
    TEST_BEGIN("cell3d_add_material adds material correctly");

    Cell3D cell;
    cell3d_init(&cell);

    // Add water: 1 mol at 293K (liquid)
    double moles = 1.0;
    double energy = calculate_material_energy(MAT_WATER, moles, 293.15);

    cell3d_add_material(&cell, MAT_WATER, moles, energy);

    ASSERT(CELL_MATERIAL_COUNT(&cell) == 1, "should have 1 material");
    ASSERT(CELL_HAS_MATERIAL(&cell, MAT_WATER), "should have water");
    ASSERT_FLOAT_EQ(cell.materials[MAT_WATER].moles, moles, 0.001, "moles incorrect");
    ASSERT_FLOAT_EQ(cell.materials[MAT_WATER].thermal_energy, energy, 1.0, "energy incorrect");

    cell3d_free(&cell);
    TEST_PASS();
}

static bool test_cell3d_add_multiple_materials(void) {
    TEST_BEGIN("cell3d can hold multiple materials");

    Cell3D cell;
    cell3d_init(&cell);

    // Add water
    cell3d_add_material(&cell, MAT_WATER, 1.0, 22000.0);
    // Add air
    cell3d_add_material(&cell, MAT_AIR, 0.5, 4000.0);
    // Add rock
    cell3d_add_material(&cell, MAT_ROCK, 2.0, 26000.0);

    ASSERT(CELL_MATERIAL_COUNT(&cell) == 3, "should have 3 materials");

    // Check each material using O(1) access
    ASSERT(CELL_HAS_MATERIAL(&cell, MAT_WATER), "should have water");
    ASSERT(CELL_HAS_MATERIAL(&cell, MAT_AIR), "should have air");
    ASSERT(CELL_HAS_MATERIAL(&cell, MAT_ROCK), "should have rock");

    ASSERT_FLOAT_EQ(cell.materials[MAT_WATER].moles, 1.0, 0.001, "water moles incorrect");
    ASSERT_FLOAT_EQ(cell.materials[MAT_AIR].moles, 0.5, 0.001, "air moles incorrect");
    ASSERT_FLOAT_EQ(cell.materials[MAT_ROCK].moles, 2.0, 0.001, "rock moles incorrect");

    cell3d_free(&cell);
    TEST_PASS();
}

static bool test_cell3d_add_existing_material(void) {
    TEST_BEGIN("adding existing material increases moles/energy");

    Cell3D cell;
    cell3d_init(&cell);

    // Add 1 mol water
    cell3d_add_material(&cell, MAT_WATER, 1.0, 22000.0);
    // Add another 0.5 mol water
    cell3d_add_material(&cell, MAT_WATER, 0.5, 11000.0);

    ASSERT(CELL_MATERIAL_COUNT(&cell) == 1, "should still have 1 material type");

    ASSERT(CELL_HAS_MATERIAL(&cell, MAT_WATER), "should have water");
    ASSERT_FLOAT_EQ(cell.materials[MAT_WATER].moles, 1.5, 0.001, "moles should be 1.5");
    ASSERT_FLOAT_EQ(cell.materials[MAT_WATER].thermal_energy, 33000.0, 1.0, "energy should be 33000");

    cell3d_free(&cell);
    TEST_PASS();
}

static bool test_cell3d_clone(void) {
    TEST_BEGIN("cell3d_clone creates independent copy");

    Cell3D original;
    cell3d_init(&original);
    cell3d_add_material(&original, MAT_WATER, 1.0, 22000.0);
    cell3d_add_material(&original, MAT_AIR, 0.5, 4000.0);

    Cell3D clone = cell3d_clone(&original);

    // Verify clone has same data
    ASSERT(CELL_MATERIAL_COUNT(&clone) == 2, "clone should have 2 materials");
    ASSERT(CELL_HAS_MATERIAL(&clone, MAT_WATER), "clone should have water");
    ASSERT_FLOAT_EQ(clone.materials[MAT_WATER].moles, 1.0, 0.001, "clone water moles incorrect");

    // Verify independence - modify clone directly, check original unchanged
    clone.materials[MAT_WATER].moles = 5.0;
    ASSERT_FLOAT_EQ(original.materials[MAT_WATER].moles, 1.0, 0.001, "original should be unchanged");

    cell3d_free(&original);
    cell3d_free(&clone);
    TEST_PASS();
}

// ============ TEMPERATURE CALCULATION TESTS ============

static bool test_temperature_calculation(void) {
    TEST_BEGIN("temperature correctly accounts for latent heat");

    Cell3D cell;
    cell3d_init(&cell);

    // Add 1 mol water at exactly 293.15K (liquid)
    // Must use proper energy calculation that includes latent heat of fusion
    double moles = 1.0;
    double target_temp = 293.15;
    double energy = calculate_material_energy(MAT_WATER, moles, target_temp);

    cell3d_add_material(&cell, MAT_WATER, moles, energy);

    double calculated_temp = cell_get_temperature(&cell);
    ASSERT_FLOAT_EQ(calculated_temp, target_temp, 0.01, "temperature calculation incorrect");

    cell3d_free(&cell);
    TEST_PASS();
}

static bool test_temperature_multiple_materials(void) {
    TEST_BEGIN("temperature weighted by heat capacity");

    Cell3D cell;
    cell3d_init(&cell);

    // Add water at 300K (liquid - needs latent heat)
    double water_moles = 1.0;
    double water_hc = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;  // 75.3 (liquid at 300K)
    double water_temp = 300.0;
    double water_energy = calculate_material_energy(MAT_WATER, water_moles, water_temp);

    // Add rock at 400K (solid below melting point of ~1983K for SiO2)
    double rock_moles = 1.0;
    double rock_hc = MATERIAL_PROPS[MAT_ROCK].molar_heat_capacity_solid;  // 44.4 (solid at 400K)
    double rock_temp = 400.0;
    double rock_energy = calculate_material_energy(MAT_ROCK, rock_moles, rock_temp);

    cell3d_add_material(&cell, MAT_WATER, water_moles, water_energy);
    cell3d_add_material(&cell, MAT_ROCK, rock_moles, rock_energy);

    // Expected: weighted average of temperatures by heat capacity
    // Water at 300K is liquid (Cp_l=75.3), rock at 400K is solid (Cp_s=44.4)
    double expected_temp = (water_temp * water_hc + rock_temp * rock_hc) / (water_hc + rock_hc);
    double calculated_temp = cell_get_temperature(&cell);

    ASSERT_FLOAT_EQ(calculated_temp, expected_temp, 0.1, "weighted temperature incorrect");

    cell3d_free(&cell);
    TEST_PASS();
}

static bool test_temperature_empty_cell(void) {
    TEST_BEGIN("empty cell returns 0.0 (vacuum has no temperature)");

    Cell3D cell;
    cell3d_init(&cell);

    double temp = cell_get_temperature(&cell);
    // Vacuum (no matter) has no temperature - 0.0 is the sentinel value
    ASSERT_FLOAT_EQ(temp, 0.0, 0.01, "empty cell should return 0.0 (vacuum)");

    cell3d_free(&cell);
    TEST_PASS();
}

// ============ MATERIAL PROPERTY TESTS ============

static bool test_material_get_temperature(void) {
    TEST_BEGIN("material_get_temperature accounts for latent heat");

    MaterialState state;
    state.moles = 2.0;
    double Cp_s = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_solid;
    double Cp_l = MATERIAL_PROPS[MAT_WATER].molar_heat_capacity_liquid;
    double Tm = MATERIAL_PROPS[MAT_WATER].melting_point;  // 273.15K
    double Hf = MATERIAL_PROPS[MAT_WATER].enthalpy_fusion; // 6010 J/mol
    double target_temp = 350.0;

    // For liquid water at 350K, energy must include latent heat:
    // E = n * Cp_s * Tm + n * Hf + n * Cp_l * (T - Tm)
    state.thermal_energy = state.moles * Cp_s * Tm +     // Energy to reach melting point (solid)
                           state.moles * Hf +             // Latent heat of fusion
                           state.moles * Cp_l * (target_temp - Tm);  // Energy to heat liquid

    double temp = material_get_temperature(&state, MAT_WATER);
    ASSERT_FLOAT_EQ(temp, target_temp, 0.01, "material temperature incorrect");

    TEST_PASS();
}

static bool test_material_get_phase_water(void) {
    TEST_BEGIN("water phase transitions at correct temperatures");

    // Ice: below 273.15K
    Phase phase = material_get_phase(MAT_WATER, 250.0);
    ASSERT(phase == PHASE_SOLID, "water at 250K should be solid");

    // Liquid: 273.15K to 373.15K
    phase = material_get_phase(MAT_WATER, 300.0);
    ASSERT(phase == PHASE_LIQUID, "water at 300K should be liquid");

    // Steam: above 373.15K
    phase = material_get_phase(MAT_WATER, 400.0);
    ASSERT(phase == PHASE_GAS, "water at 400K should be gas");

    TEST_PASS();
}

static bool test_material_properties_lookup(void) {
    TEST_BEGIN("material properties are accessible");

    // Water properties
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_WATER].molar_mass, 0.018, 0.001, "water molar mass");
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_WATER].melting_point, 273.15, 0.01, "water melting point");
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_WATER].boiling_point, 373.15, 0.01, "water boiling point");

    // Rock properties
    ASSERT_FLOAT_EQ(MATERIAL_PROPS[MAT_ROCK].molar_mass, 0.060, 0.001, "rock molar mass");
    ASSERT(MATERIAL_PROPS[MAT_ROCK].melting_point > 1900, "rock melting point should be high");

    TEST_PASS();
}

// ============ COORDINATE CONVERSION TESTS ============

static bool test_world_to_cell_conversion(void) {
    TEST_BEGIN("world to cell coordinate conversion");

    int cx, cy, cz;

    // World origin (0, 0, 0) maps to cell center (128, 128, 128)
    // The coordinate system is centered: world origin is at SVO center
    svo_world_to_cell(0.0f, 0.0f, 0.0f, &cx, &cy, &cz);
    ASSERT(cx == SVO_SIZE / 2, "x at origin should be 128");
    ASSERT(cy == SVO_GROUND_Y, "y at world 0 should be SVO_GROUND_Y (128)");
    ASSERT(cz == SVO_SIZE / 2, "z at origin should be 128");

    // Positive world coordinates offset from center
    svo_world_to_cell(SVO_CELL_SIZE * 10, SVO_CELL_SIZE * 5, SVO_CELL_SIZE * 20, &cx, &cy, &cz);
    ASSERT(cx == SVO_SIZE / 2 + 10, "x should be 138");
    ASSERT(cy == SVO_GROUND_Y + 5, "y should be ground + 5");
    ASSERT(cz == SVO_SIZE / 2 + 20, "z should be 148");

    TEST_PASS();
}

static bool test_cell_to_world_conversion(void) {
    TEST_BEGIN("cell to world coordinate conversion");

    float wx, wy, wz;

    // Cell at center (128, 128, 128) maps to world position at cell center
    // cell_to_world returns CENTER of cell (+0.5), so expected is 0.5 * CELL_SIZE
    svo_cell_to_world(SVO_SIZE / 2, SVO_GROUND_Y, SVO_SIZE / 2, &wx, &wy, &wz);
    ASSERT_FLOAT_EQ(wx, 0.5f * SVO_CELL_SIZE, 0.01, "world x at center cell");
    ASSERT_FLOAT_EQ(wy, 0.5f * SVO_CELL_SIZE, 0.01, "world y at ground cell");
    ASSERT_FLOAT_EQ(wz, 0.5f * SVO_CELL_SIZE, 0.01, "world z at center cell");

    // Cell offset from center - offset by 10 cells + the 0.5 for cell center
    svo_cell_to_world(SVO_SIZE / 2 + 10, SVO_GROUND_Y + 5, SVO_SIZE / 2 + 20, &wx, &wy, &wz);
    ASSERT_FLOAT_EQ(wx, 10.5f * SVO_CELL_SIZE, 0.01, "world x incorrect");
    ASSERT_FLOAT_EQ(wy, 5.5f * SVO_CELL_SIZE, 0.01, "world y incorrect");
    ASSERT_FLOAT_EQ(wz, 20.5f * SVO_CELL_SIZE, 0.01, "world z incorrect");

    TEST_PASS();
}

static bool test_coordinate_roundtrip(void) {
    TEST_BEGIN("coordinate conversion roundtrip");

    float orig_x = 25.0f, orig_y = 10.0f, orig_z = 50.0f;
    int cx, cy, cz;
    float back_x, back_y, back_z;

    svo_world_to_cell(orig_x, orig_y, orig_z, &cx, &cy, &cz);
    svo_cell_to_world(cx, cy, cz, &back_x, &back_y, &back_z);

    // Should be within one cell size due to quantization
    ASSERT(fabs(back_x - orig_x) < SVO_CELL_SIZE, "x roundtrip error too large");
    ASSERT(fabs(back_y - orig_y) < SVO_CELL_SIZE, "y roundtrip error too large");
    ASSERT(fabs(back_z - orig_z) < SVO_CELL_SIZE, "z roundtrip error too large");

    TEST_PASS();
}

// ============ CHUNK SYSTEM TESTS ============

static bool test_chunk_create(void) {
    TEST_BEGIN("chunk_create initializes correctly");

    Chunk *chunk = chunk_create(0, 0, 0);

    ASSERT(chunk != NULL, "chunk should not be NULL");
    ASSERT(chunk->cx == 0, "cx should be 0");
    ASSERT(chunk->cy == 0, "cy should be 0");
    ASSERT(chunk->cz == 0, "cz should be 0");
    ASSERT(chunk->is_active == false, "chunk should not be active initially");

    chunk_free(chunk);
    TEST_PASS();
}

static bool test_chunk_cell_access(void) {
    TEST_BEGIN("chunk_get_cell provides O(1) access");

    Chunk *chunk = chunk_create(0, 0, 0);

    // Add material to a cell
    Cell3D *cell = chunk_get_cell(chunk, 5, 10, 15);
    cell_init(cell);
    cell_add_material(cell, MAT_WATER, 1.0, 22000.0);

    // Verify we can retrieve it
    const Cell3D *cell_read = chunk_get_cell_const(chunk, 5, 10, 15);
    ASSERT(CELL_HAS_MATERIAL(cell_read, MAT_WATER), "should have water");
    ASSERT(fabs(cell_read->materials[MAT_WATER].moles - 1.0) < 0.001, "water moles incorrect");

    chunk_free(chunk);
    TEST_PASS();
}

static bool test_world_chunk_hash(void) {
    TEST_BEGIN("world uses hash table for O(1) chunk lookup");

    ChunkWorld world;
    world_init(&world);

    // Create chunks at different positions
    Chunk *chunk1 = world_get_or_create_chunk(&world, 0, 0, 0);
    Chunk *chunk2 = world_get_or_create_chunk(&world, 1, 2, 3);
    Chunk *chunk3 = world_get_or_create_chunk(&world, -1, -1, -1);

    ASSERT(chunk1 != NULL, "chunk1 should be created");
    ASSERT(chunk2 != NULL, "chunk2 should be created");
    ASSERT(chunk3 != NULL, "chunk3 should be created");
    ASSERT(world.chunk_count == 3, "should have 3 chunks");

    // Verify lookup returns same chunks
    ASSERT(world_get_chunk(&world, 0, 0, 0) == chunk1, "lookup should return chunk1");
    ASSERT(world_get_chunk(&world, 1, 2, 3) == chunk2, "lookup should return chunk2");
    ASSERT(world_get_chunk(&world, -1, -1, -1) == chunk3, "lookup should return chunk3");

    world_cleanup(&world);
    TEST_PASS();
}

// ============ CELLS MATCH TEST ============

static bool test_cells_match(void) {
    TEST_BEGIN("cells_match detects identical cells");

    Cell3D a, b;
    cell3d_init(&a);
    cell3d_init(&b);

    // Empty cells should match
    ASSERT(cells_match(&a, &b), "empty cells should match");

    // Add same material to both
    cell3d_add_material(&a, MAT_WATER, 1.0, 22000.0);
    cell3d_add_material(&b, MAT_WATER, 1.0, 22000.0);
    ASSERT(cells_match(&a, &b), "identical cells should match");

    // Different moles should not match
    cell3d_add_material(&a, MAT_WATER, 0.5, 11000.0);  // Now has 1.5 mol
    ASSERT(!cells_match(&a, &b), "different moles should not match");

    cell3d_free(&a);
    cell3d_free(&b);
    TEST_PASS();
}

// ============ TOOL FUNCTION TESTS ============

static bool test_svo_add_water(void) {
    TEST_BEGIN("world_add_water_at adds water to cell");

    // Create and initialize a chunk world
    ChunkWorld world;
    world_init(&world);

    // Add water at world origin
    world_add_water_at(&world, 0.0f, 0.0f, 0.0f, 1.0);

    // Check that water was added
    CellInfo info = world_get_cell_info(&world, 0.0f, 0.0f, 0.0f);
    ASSERT(info.valid, "cell should be valid");
    ASSERT(info.material_count >= 1, "cell should have materials");

    world_cleanup(&world);
    TEST_PASS();
}

static bool test_svo_add_heat(void) {
    TEST_BEGIN("world_add_heat_at increases temperature");

    // Create and initialize a chunk world
    ChunkWorld world;
    world_init(&world);

    // First add water so we have something to heat
    world_add_water_at(&world, 0.0f, 0.0f, 0.0f, 1.0);

    // Get initial temperature
    CellInfo info_before = world_get_cell_info(&world, 0.0f, 0.0f, 0.0f);
    double temp_before = info_before.temperature;

    // Add heat
    world_add_heat_at(&world, 0.0f, 0.0f, 0.0f, 1000.0);

    // Get new temperature
    CellInfo info_after = world_get_cell_info(&world, 0.0f, 0.0f, 0.0f);
    ASSERT(info_after.temperature > temp_before, "temperature should increase after adding heat");

    world_cleanup(&world);
    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n=== Chunk Matter System Unit Tests ===\n\n");

    int passed = 0, failed = 0;

    // Cell3D operations
    RUN_TEST(test_cell3d_init);
    RUN_TEST(test_cell3d_add_material);
    RUN_TEST(test_cell3d_add_multiple_materials);
    RUN_TEST(test_cell3d_add_existing_material);
    RUN_TEST(test_cell3d_clone);

    // Temperature calculations
    RUN_TEST(test_temperature_calculation);
    RUN_TEST(test_temperature_multiple_materials);
    RUN_TEST(test_temperature_empty_cell);
    RUN_TEST(test_material_get_temperature);

    // Phase transitions
    RUN_TEST(test_material_get_phase_water);
    RUN_TEST(test_material_properties_lookup);

    // Coordinate conversions
    RUN_TEST(test_world_to_cell_conversion);
    RUN_TEST(test_cell_to_world_conversion);
    RUN_TEST(test_coordinate_roundtrip);

    // Chunk system operations
    RUN_TEST(test_chunk_create);
    RUN_TEST(test_chunk_cell_access);
    RUN_TEST(test_world_chunk_hash);
    RUN_TEST(test_cells_match);

    // Tool functions
    RUN_TEST(test_svo_add_water);
    RUN_TEST(test_svo_add_heat);

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);

    return failed > 0 ? 1 : 0;
}
