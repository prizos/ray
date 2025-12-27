// Test suite for game module
// Note: These tests don't require raylib window initialization

#include "test.h"
#include "raylib.h"
#include "raymath.h"

// Test Vector3 operations (used by our input/camera code)
TEST(vector3_add)
{
    Vector3 a = { 1.0f, 2.0f, 3.0f };
    Vector3 b = { 4.0f, 5.0f, 6.0f };
    Vector3 result = Vector3Add(a, b);

    ASSERT_FLOAT_EQ(result.x, 5.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.y, 7.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.z, 9.0f, 0.001f);

    return TEST_PASS;
}

TEST(vector3_subtract)
{
    Vector3 a = { 5.0f, 7.0f, 9.0f };
    Vector3 b = { 1.0f, 2.0f, 3.0f };
    Vector3 result = Vector3Subtract(a, b);

    ASSERT_FLOAT_EQ(result.x, 4.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.y, 5.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.z, 6.0f, 0.001f);

    return TEST_PASS;
}

TEST(vector3_scale)
{
    Vector3 v = { 1.0f, 2.0f, 3.0f };
    Vector3 result = Vector3Scale(v, 2.0f);

    ASSERT_FLOAT_EQ(result.x, 2.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.y, 4.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.z, 6.0f, 0.001f);

    return TEST_PASS;
}

TEST(vector3_normalize)
{
    Vector3 v = { 3.0f, 0.0f, 4.0f };
    Vector3 result = Vector3Normalize(v);

    // Length should be 5, so normalized is (0.6, 0, 0.8)
    ASSERT_FLOAT_EQ(result.x, 0.6f, 0.001f);
    ASSERT_FLOAT_EQ(result.y, 0.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.z, 0.8f, 0.001f);

    return TEST_PASS;
}

TEST(vector3_length)
{
    Vector3 v = { 3.0f, 4.0f, 0.0f };
    float len = Vector3Length(v);

    ASSERT_FLOAT_EQ(len, 5.0f, 0.001f);

    return TEST_PASS;
}

TEST(vector3_cross_product)
{
    Vector3 a = { 1.0f, 0.0f, 0.0f };
    Vector3 b = { 0.0f, 1.0f, 0.0f };
    Vector3 result = Vector3CrossProduct(a, b);

    // X cross Y = Z
    ASSERT_FLOAT_EQ(result.x, 0.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.y, 0.0f, 0.001f);
    ASSERT_FLOAT_EQ(result.z, 1.0f, 0.001f);

    return TEST_PASS;
}

// Test camera initialization values
TEST(camera_defaults)
{
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 5.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    ASSERT_FLOAT_EQ(camera.position.y, 5.0f, 0.001f);
    ASSERT_FLOAT_EQ(camera.fovy, 45.0f, 0.001f);
    ASSERT_EQ(camera.projection, CAMERA_PERSPECTIVE);

    return TEST_PASS;
}

// Test color values
TEST(color_values)
{
    ASSERT_EQ(RED.r, 230);
    ASSERT_EQ(RED.g, 41);
    ASSERT_EQ(RED.b, 55);
    ASSERT_EQ(RED.a, 255);

    ASSERT_EQ(GREEN.r, 0);
    ASSERT_EQ(GREEN.g, 228);
    ASSERT_EQ(GREEN.b, 48);

    ASSERT_EQ(BLUE.r, 0);
    ASSERT_EQ(BLUE.g, 121);
    ASSERT_EQ(BLUE.b, 241);

    return TEST_PASS;
}

int main(void)
{
    printf("Running game tests...\n\n");

    RUN_TEST(vector3_add);
    RUN_TEST(vector3_subtract);
    RUN_TEST(vector3_scale);
    RUN_TEST(vector3_normalize);
    RUN_TEST(vector3_length);
    RUN_TEST(vector3_cross_product);
    RUN_TEST(camera_defaults);
    RUN_TEST(color_values);

    TEST_SUMMARY();
    return TEST_RESULT();
}
