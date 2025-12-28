/**
 * Matter System - Unit Tests
 *
 * Tests individual functions in complete isolation.
 * No grids, no simulation steps - just pure function testing.
 *
 * Theories tested:
 * 1. Fixed-point math is accurate and reversible
 * 2. Cell cache computation is correct (T = E / thermal_mass)
 * 3. Heat flow direction follows thermodynamics (hot -> cold)
 * 4. Energy transfer limits work correctly
 */

#include "test_common.h"

// ============ TEST CELL (simplified for unit testing) ============

typedef struct {
    fixed16_t mass;
    fixed16_t specific_heat;
    fixed16_t energy;
    fixed16_t temperature;
    fixed16_t thermal_mass;
} TestCell;

static void cell_init(TestCell *c, float mass, float specific_heat, float temp_k) {
    c->mass = FLOAT_TO_FIXED(mass);
    c->specific_heat = FLOAT_TO_FIXED(specific_heat);
    c->thermal_mass = fixed_mul(c->mass, c->specific_heat);
    c->temperature = FLOAT_TO_FIXED(temp_k);
    c->energy = fixed_mul(c->thermal_mass, c->temperature);
}

static void cell_update_cache(TestCell *c) {
    if (c->thermal_mass > FLOAT_TO_FIXED(0.001f)) {
        c->temperature = fixed_div(c->energy, c->thermal_mass);
    } else {
        c->temperature = TEST_AMBIENT_TEMP;
    }
}

static float cell_temp(const TestCell *c) {
    return FIXED_TO_FLOAT(c->temperature);
}

// ============ FIXED-POINT MATH TESTS ============

static bool test_fixed_float_roundtrip(void) {
    TEST_BEGIN("fixed-point float roundtrip");

    float values[] = {0.0f, 1.0f, -1.0f, 293.15f, 0.001f, 1000.0f};
    for (int i = 0; i < 6; i++) {
        fixed16_t fixed = FLOAT_TO_FIXED(values[i]);
        float back = FIXED_TO_FLOAT(fixed);
        ASSERT_FLOAT_EQ(back, values[i], 0.001f, "roundtrip failed");
    }

    TEST_PASS();
}

static bool test_fixed_multiplication(void) {
    TEST_BEGIN("fixed-point multiplication");

    // 10 * 0.5 = 5
    fixed16_t a = FLOAT_TO_FIXED(10.0f);
    fixed16_t b = FLOAT_TO_FIXED(0.5f);
    fixed16_t result = fixed_mul(a, b);
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(result), 5.0f, 0.001f, "10 * 0.5 != 5");

    // 0.7 * 293.15 = 205.205
    a = FLOAT_TO_FIXED(0.7f);
    b = FLOAT_TO_FIXED(293.15f);
    result = fixed_mul(a, b);
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(result), 205.205f, 0.01f, "0.7 * 293.15 incorrect");

    TEST_PASS();
}

static bool test_fixed_division(void) {
    TEST_BEGIN("fixed-point division");

    // 100 / 4 = 25
    fixed16_t a = FLOAT_TO_FIXED(100.0f);
    fixed16_t b = FLOAT_TO_FIXED(4.0f);
    fixed16_t result = fixed_div(a, b);
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(result), 25.0f, 0.001f, "100 / 4 != 25");

    // 205.205 / 0.7 = 293.15
    a = FLOAT_TO_FIXED(205.205f);
    b = FLOAT_TO_FIXED(0.7f);
    result = fixed_div(a, b);
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(result), 293.15f, 0.1f, "energy/thermal_mass incorrect");

    TEST_PASS();
}

static bool test_fixed_mul_div_identity(void) {
    TEST_BEGIN("fixed mul/div identity");

    // (a * b) / b should equal a
    fixed16_t a = FLOAT_TO_FIXED(293.15f);
    fixed16_t b = FLOAT_TO_FIXED(0.8f);
    fixed16_t product = fixed_mul(a, b);
    fixed16_t back = fixed_div(product, b);

    // Allow small error due to rounding
    ASSERT(abs(back - a) <= 2, "mul/div roundtrip error too large");

    TEST_PASS();
}

// ============ CELL CACHE TESTS ============

static bool test_cell_thermal_mass_calculation(void) {
    TEST_BEGIN("cell thermal mass = mass * specific_heat");

    TestCell c;
    cell_init(&c, 1.0f, 0.7f, 293.15f);

    float expected = 1.0f * 0.7f;
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(c.thermal_mass), expected, 0.001f,
                    "thermal_mass incorrect");

    TEST_PASS();
}

static bool test_cell_energy_from_temp(void) {
    TEST_BEGIN("cell energy = thermal_mass * temperature");

    TestCell c;
    cell_init(&c, 1.0f, 0.7f, 293.15f);

    float expected = 0.7f * 293.15f;
    ASSERT_FLOAT_EQ(FIXED_TO_FLOAT(c.energy), expected, 0.1f,
                    "energy incorrect");

    TEST_PASS();
}

static bool test_cell_temp_from_energy(void) {
    TEST_BEGIN("cell temperature = energy / thermal_mass");

    TestCell c;
    cell_init(&c, 1.0f, 0.7f, 300.0f);

    // Manually set energy and verify temp calculation
    c.energy = FLOAT_TO_FIXED(280.0f);  // 280 / 0.7 = 400K
    cell_update_cache(&c);

    ASSERT_FLOAT_EQ(cell_temp(&c), 400.0f, 0.1f, "temperature from energy incorrect");

    TEST_PASS();
}

static bool test_cell_temp_uniform_across_thermal_mass(void) {
    TEST_BEGIN("same temp regardless of thermal_mass");

    // Cells with different thermal mass should have same temp after init
    TestCell cells[4];
    float masses[] = {0.5f, 0.8f, 1.0f, 1.5f};

    for (int i = 0; i < 4; i++) {
        cell_init(&cells[i], 1.0f, masses[i], 293.15f);
        cell_update_cache(&cells[i]);
    }

    for (int i = 1; i < 4; i++) {
        ASSERT_FLOAT_EQ(cell_temp(&cells[i]), cell_temp(&cells[0]), 0.001f,
                        "temperature varies with thermal_mass");
    }

    TEST_PASS();
}

// ============ HEAT FLOW DIRECTION TESTS ============

static fixed16_t calc_heat_flow(const TestCell *from, const TestCell *to, float rate) {
    fixed16_t temp_diff = to->temperature - from->temperature;
    return fixed_mul(temp_diff, FLOAT_TO_FIXED(rate));
}

static bool test_heat_flows_hot_to_cold(void) {
    TEST_BEGIN("heat flows from hot to cold");

    TestCell hot, cold;
    cell_init(&hot, 1.0f, 0.7f, 400.0f);
    cell_init(&cold, 1.0f, 0.7f, 300.0f);

    // From cold's perspective: should receive (positive flow)
    fixed16_t flow_to_cold = calc_heat_flow(&cold, &hot, 0.1f);
    ASSERT(flow_to_cold > 0, "cold cell should receive heat (positive flow)");

    // From hot's perspective: should lose (negative flow)
    fixed16_t flow_to_hot = calc_heat_flow(&hot, &cold, 0.1f);
    ASSERT(flow_to_hot < 0, "hot cell should lose heat (negative flow)");

    TEST_PASS();
}

static bool test_no_heat_flow_at_equilibrium(void) {
    TEST_BEGIN("no heat flow at same temperature");

    TestCell a, b;
    cell_init(&a, 1.0f, 0.7f, 300.0f);
    cell_init(&b, 1.0f, 0.8f, 300.0f);  // Different thermal mass, same temp

    fixed16_t flow = calc_heat_flow(&a, &b, 0.1f);
    ASSERT(flow == 0, "no flow expected at equilibrium");

    TEST_PASS();
}

static bool test_heat_flow_proportional_to_diff(void) {
    TEST_BEGIN("heat flow proportional to temp difference");

    TestCell base, target1, target2;
    cell_init(&base, 1.0f, 0.7f, 300.0f);
    cell_init(&target1, 1.0f, 0.7f, 350.0f);  // 50K diff
    cell_init(&target2, 1.0f, 0.7f, 400.0f);  // 100K diff

    fixed16_t flow1 = calc_heat_flow(&base, &target1, 0.1f);
    fixed16_t flow2 = calc_heat_flow(&base, &target2, 0.1f);

    // flow2 should be ~2x flow1
    float ratio = (float)flow2 / (float)flow1;
    ASSERT_FLOAT_EQ(ratio, 2.0f, 0.01f, "flow not proportional to temp diff");

    TEST_PASS();
}

// ============ ENERGY LIMIT TESTS ============

static bool test_donor_energy_limit(void) {
    TEST_BEGIN("heat transfer limited by donor energy");

    TestCell donor, receiver;
    cell_init(&donor, 1.0f, 0.7f, 400.0f);
    cell_init(&receiver, 1.0f, 0.7f, 100.0f);  // Very cold - big temp diff

    fixed16_t temp_diff = donor.temperature - receiver.temperature;
    fixed16_t uncapped_flow = fixed_mul(temp_diff, FLOAT_TO_FIXED(1.0f));  // High rate

    // 5% of donor's energy
    fixed16_t max_transfer = donor.energy / 20;

    // Uncapped flow should exceed limit
    ASSERT(uncapped_flow > max_transfer, "test setup: uncapped should exceed max");

    // Capped flow
    fixed16_t capped_flow = (uncapped_flow > max_transfer) ? max_transfer : uncapped_flow;
    ASSERT(capped_flow == max_transfer, "flow should be capped to 5%% of donor energy");

    TEST_PASS();
}

static bool test_energy_conservation_two_cells(void) {
    TEST_BEGIN("energy conserved in two-cell exchange");

    TestCell a, b;
    cell_init(&a, 1.0f, 0.7f, 400.0f);
    cell_init(&b, 1.0f, 0.7f, 300.0f);

    fixed16_t initial_total = a.energy + b.energy;

    // Simulate heat transfer
    fixed16_t flow = calc_heat_flow(&a, &b, 0.1f);

    // Limit by donor (a is hot, gives to b)
    fixed16_t max = a.energy / 20;
    if (flow < -max) flow = -max;

    a.energy += flow;   // a loses (flow is negative)
    b.energy -= flow;   // b gains (subtracting negative)

    fixed16_t final_total = a.energy + b.energy;

    ASSERT_EQ(final_total, initial_total, "energy not conserved");

    TEST_PASS();
}

// ============ MAIN ============

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("Matter System - Unit Tests\n");
    printf("========================================\n");

    test_suite_begin("FIXED-POINT MATH");
    test_fixed_float_roundtrip();
    test_fixed_multiplication();
    test_fixed_division();
    test_fixed_mul_div_identity();
    test_suite_end();

    test_suite_begin("CELL CACHE COMPUTATION");
    test_cell_thermal_mass_calculation();
    test_cell_energy_from_temp();
    test_cell_temp_from_energy();
    test_cell_temp_uniform_across_thermal_mass();
    test_suite_end();

    test_suite_begin("HEAT FLOW DIRECTION");
    test_heat_flows_hot_to_cold();
    test_no_heat_flow_at_equilibrium();
    test_heat_flow_proportional_to_diff();
    test_suite_end();

    test_suite_begin("ENERGY LIMITS");
    test_donor_energy_limit();
    test_energy_conservation_two_cells();
    test_suite_end();

    test_summary();
    return test_exit_code();
}
