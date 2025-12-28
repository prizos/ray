#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include "octree.h"

// ============ TEST HELPERS ============

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

// ============ TESTS ============

void test_create_destroy(void) {
    printf("test_create_destroy...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    TEST_ASSERT(tree != NULL, "octree_create should return non-NULL");
    TEST_ASSERT(tree->node_count == 1, "new octree should have 1 node (root)");
    TEST_ASSERT(tree->total_items == 0, "new octree should have 0 items");

    octree_destroy(tree);
    printf("  passed\n");
}

void test_insert_single(void) {
    printf("test_insert_single...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    bool result = octree_insert(tree, 50, 50, 50, 42);
    TEST_ASSERT(result, "insert should succeed");
    TEST_ASSERT(tree->total_items == 1, "should have 1 item");

    octree_destroy(tree);
    printf("  passed\n");
}

void test_insert_out_of_bounds(void) {
    printf("test_insert_out_of_bounds...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    bool result = octree_insert(tree, 150, 50, 50, 1);
    TEST_ASSERT(!result, "insert out of bounds should fail");
    TEST_ASSERT(tree->total_items == 0, "should have 0 items");

    octree_destroy(tree);
    printf("  passed\n");
}

void test_insert_many(void) {
    printf("test_insert_many...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    int count = 1000;
    for (int i = 0; i < count; i++) {
        float x = randf() * 99.9f;
        float y = randf() * 99.9f;
        float z = randf() * 99.9f;
        octree_insert(tree, x, y, z, i);
    }

    TEST_ASSERT(tree->total_items == count, "should have all items inserted");
    TEST_ASSERT(tree->node_count > 1, "should have subdivided");

    octree_destroy(tree);
    printf("  passed (inserted %d items, %d nodes)\n", count, tree ? 0 : 0);
}

void test_query_sphere(void) {
    printf("test_query_sphere...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    // Insert items at known positions
    octree_insert(tree, 50, 50, 50, 0);  // Center
    octree_insert(tree, 55, 50, 50, 1);  // 5 units away
    octree_insert(tree, 60, 50, 50, 2);  // 10 units away
    octree_insert(tree, 70, 50, 50, 3);  // 20 units away

    OctreeQueryResult *result = octree_result_create(16);

    // Query sphere radius 6 - should find 2 items
    octree_query_sphere(tree, 50, 50, 50, 6, result);
    TEST_ASSERT(result->count == 2, "radius 6 should find 2 items");

    // Query sphere radius 15 - should find 3 items
    octree_result_clear(result);
    octree_query_sphere(tree, 50, 50, 50, 15, result);
    TEST_ASSERT(result->count == 3, "radius 15 should find 3 items");

    // Query sphere radius 25 - should find 4 items
    octree_result_clear(result);
    octree_query_sphere(tree, 50, 50, 50, 25, result);
    TEST_ASSERT(result->count == 4, "radius 25 should find 4 items");

    octree_result_destroy(result);
    octree_destroy(tree);
    printf("  passed\n");
}

void test_query_sphere_vs_brute_force(void) {
    printf("test_query_sphere_vs_brute_force...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    // Insert random items and track positions
    int count = 500;
    float *positions = (float *)malloc(count * 3 * sizeof(float));

    for (int i = 0; i < count; i++) {
        float x = randf() * 99.9f;
        float y = randf() * 99.9f;
        float z = randf() * 99.9f;
        positions[i * 3 + 0] = x;
        positions[i * 3 + 1] = y;
        positions[i * 3 + 2] = z;
        octree_insert(tree, x, y, z, i);
    }

    // Query from center with radius 20
    float cx = 50, cy = 50, cz = 50, radius = 20;
    OctreeQueryResult *result = octree_result_create(count);
    octree_query_sphere(tree, cx, cy, cz, radius, result);

    // Brute force count
    int brute_count = 0;
    float radius_sq = radius * radius;
    for (int i = 0; i < count; i++) {
        float dx = positions[i * 3 + 0] - cx;
        float dy = positions[i * 3 + 1] - cy;
        float dz = positions[i * 3 + 2] - cz;
        if (dx*dx + dy*dy + dz*dz <= radius_sq) {
            brute_count++;
        }
    }

    TEST_ASSERT(result->count == brute_count,
                "octree query should match brute force count");

    free(positions);
    octree_result_destroy(result);
    octree_destroy(tree);
    printf("  passed (found %d items)\n", brute_count);
}

void test_query_nearest(void) {
    printf("test_query_nearest...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    // Insert items at known positions
    octree_insert(tree, 10, 10, 10, 0);
    octree_insert(tree, 20, 20, 20, 1);
    octree_insert(tree, 80, 80, 80, 2);

    uint32_t nearest_data;
    float nearest_dist;

    // Query near first item
    bool found = octree_query_nearest(tree, 12, 12, 12, 100, &nearest_data, &nearest_dist);
    TEST_ASSERT(found, "should find nearest");
    TEST_ASSERT(nearest_data == 0, "nearest should be item 0");

    // Query near third item
    found = octree_query_nearest(tree, 75, 75, 75, 100, &nearest_data, &nearest_dist);
    TEST_ASSERT(found, "should find nearest");
    TEST_ASSERT(nearest_data == 2, "nearest should be item 2");

    // Query with too small max distance
    found = octree_query_nearest(tree, 50, 50, 50, 1, &nearest_data, &nearest_dist);
    TEST_ASSERT(!found, "should not find anything within radius 1");

    octree_destroy(tree);
    printf("  passed\n");
}

void test_remove(void) {
    printf("test_remove...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    octree_insert(tree, 50, 50, 50, 42);
    TEST_ASSERT(tree->total_items == 1, "should have 1 item");

    bool removed = octree_remove(tree, 50, 50, 50, 42);
    TEST_ASSERT(removed, "remove should succeed");
    TEST_ASSERT(tree->total_items == 0, "should have 0 items");

    // Try to remove again
    removed = octree_remove(tree, 50, 50, 50, 42);
    TEST_ASSERT(!removed, "second remove should fail");

    octree_destroy(tree);
    printf("  passed\n");
}

void test_query_range(void) {
    printf("test_query_range...\n");

    OctreeBounds bounds = {0, 0, 0, 100, 100, 100};
    Octree *tree = octree_create(bounds, 6);

    // Insert items in a grid
    for (int x = 0; x < 10; x++) {
        for (int z = 0; z < 10; z++) {
            octree_insert(tree, x * 10 + 5, 50, z * 10 + 5, x * 10 + z);
        }
    }

    OctreeQueryResult *result = octree_result_create(100);

    // Query a range that should contain 4 items (2x2 grid cells)
    OctreeBounds range = {0, 0, 0, 20, 100, 20};
    octree_query_range(tree, range, result);
    TEST_ASSERT(result->count == 4, "2x2 grid range should find 4 items");

    octree_result_destroy(result);
    octree_destroy(tree);
    printf("  passed\n");
}

void test_bounds_helpers(void) {
    printf("test_bounds_helpers...\n");

    OctreeBounds b = {0, 0, 0, 10, 10, 10};

    // Point inside
    TEST_ASSERT(octree_bounds_contains_point(&b, 5, 5, 5), "center should be inside");
    TEST_ASSERT(octree_bounds_contains_point(&b, 0, 0, 0), "min corner should be inside");
    TEST_ASSERT(octree_bounds_contains_point(&b, 10, 10, 10), "max corner should be inside");

    // Point outside
    TEST_ASSERT(!octree_bounds_contains_point(&b, -1, 5, 5), "outside x- should fail");
    TEST_ASSERT(!octree_bounds_contains_point(&b, 11, 5, 5), "outside x+ should fail");

    // Bounds intersection
    OctreeBounds b2 = {5, 5, 5, 15, 15, 15};
    TEST_ASSERT(octree_bounds_intersects(&b, &b2), "overlapping bounds should intersect");

    OctreeBounds b3 = {20, 20, 20, 30, 30, 30};
    TEST_ASSERT(!octree_bounds_intersects(&b, &b3), "non-overlapping bounds should not intersect");

    // Distance to point
    float dist_sq = octree_bounds_point_dist_sq(&b, 15, 5, 5);
    TEST_ASSERT(fabsf(dist_sq - 25.0f) < 0.001f, "distance squared should be 25");

    dist_sq = octree_bounds_point_dist_sq(&b, 5, 5, 5);
    TEST_ASSERT(dist_sq == 0.0f, "point inside should have 0 distance");

    printf("  passed\n");
}

void test_performance_insert(void) {
    printf("test_performance_insert...\n");

    OctreeBounds bounds = {0, 0, 0, 1000, 1000, 1000};
    Octree *tree = octree_create(bounds, 8);

    int count = 10000;
    clock_t start = clock();

    for (int i = 0; i < count; i++) {
        float x = randf() * 999.9f;
        float y = randf() * 999.9f;
        float z = randf() * 999.9f;
        octree_insert(tree, x, y, z, i);
    }

    clock_t end = clock();
    double ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    printf("  inserted %d items in %.2f ms (%.0f items/sec)\n",
           count, ms, count / (ms / 1000.0));
    printf("  nodes: %d, avg items/leaf: %.2f\n",
           tree->node_count,
           tree->node_count > 0 ? (float)tree->total_items / tree->node_count : 0);

    TEST_ASSERT(ms < 100, "insert 10K items should take < 100ms");

    octree_destroy(tree);
    printf("  passed\n");
}

void test_performance_query(void) {
    printf("test_performance_query...\n");

    OctreeBounds bounds = {0, 0, 0, 1000, 1000, 1000};
    Octree *tree = octree_create(bounds, 8);

    // Insert items
    int count = 10000;
    for (int i = 0; i < count; i++) {
        float x = randf() * 999.9f;
        float y = randf() * 999.9f;
        float z = randf() * 999.9f;
        octree_insert(tree, x, y, z, i);
    }

    // Benchmark queries
    int query_count = 1000;
    OctreeQueryResult *result = octree_result_create(count);

    clock_t start = clock();
    int total_found = 0;

    for (int i = 0; i < query_count; i++) {
        float cx = randf() * 1000;
        float cy = randf() * 1000;
        float cz = randf() * 1000;
        octree_result_clear(result);
        octree_query_sphere(tree, cx, cy, cz, 50, result);
        total_found += result->count;
    }

    clock_t end = clock();
    double ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    printf("  %d sphere queries in %.2f ms (%.0f queries/sec)\n",
           query_count, ms, query_count / (ms / 1000.0));
    printf("  avg items found per query: %.1f\n", (float)total_found / query_count);

    TEST_ASSERT(ms < 50, "1000 queries should take < 50ms");

    octree_result_destroy(result);
    octree_destroy(tree);
    printf("  passed\n");
}

// ============ MAIN ============

int main(void) {
    printf("\n=== Octree Unit Tests ===\n\n");

    srand(12345);  // Fixed seed for reproducibility

    test_create_destroy();
    test_insert_single();
    test_insert_out_of_bounds();
    test_insert_many();
    test_query_sphere();
    test_query_sphere_vs_brute_force();
    test_query_nearest();
    test_remove();
    test_query_range();
    test_bounds_helpers();
    test_performance_insert();
    test_performance_query();

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
