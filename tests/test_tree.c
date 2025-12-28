#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  Testing: %s... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)

// ============ SPATIAL HASH TESTS ============

void test_hash_pack_key(void) {
    TEST("tree_pack_key basic packing");

    int key1 = tree_pack_key(0, 0, 0);
    int key2 = tree_pack_key(0, 0, 0);
    int key3 = tree_pack_key(1, 0, 0);
    int key4 = tree_pack_key(0, 1, 0);
    int key5 = tree_pack_key(0, 0, 1);

    if (key1 == key2 && key1 != key3 && key1 != key4 && key1 != key5) {
        PASS();
    } else {
        FAIL("Keys should be equal for same position, different for different positions");
    }
}

void test_hash_negative_coords(void) {
    TEST("tree_pack_key with negative coordinates");

    int key_neg = tree_pack_key(-10, 5, -10);
    int key_pos = tree_pack_key(10, 5, 10);

    if (key_neg != key_pos && key_neg > 0) {
        PASS();
    } else {
        FAIL("Negative coordinates should produce valid different keys");
    }
}

void test_hash_index_distribution(void) {
    TEST("tree_hash_index distribution");

    // Check that different keys produce indices within range
    int indices[100];
    for (int i = 0; i < 100; i++) {
        int key = tree_pack_key(i - 50, i % 50, (i * 7) % 50 - 25);
        indices[i] = tree_hash_index(key);

        if (indices[i] < 0 || indices[i] >= VOXEL_HASH_SIZE) {
            FAIL("Hash index out of range");
            return;
        }
    }

    // Check for some variation (not all same)
    int same_count = 0;
    for (int i = 1; i < 100; i++) {
        if (indices[i] == indices[0]) same_count++;
    }

    if (same_count < 90) {
        PASS();
    } else {
        FAIL("Hash indices not distributed (too many collisions)");
    }
}

// ============ VOXEL TESTS ============

void test_voxel_add_basic(void) {
    TEST("tree_add_voxel basic add");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);

    bool result = tree_add_voxel(&tree, 0, 0, 0, VOXEL_TRUNK);

    if (result && tree.voxel_count == 1 && tree.trunk_count == 1) {
        PASS();
    } else {
        FAIL("Should add voxel and increment counts");
    }
}

void test_voxel_duplicate_rejected(void) {
    TEST("tree_add_voxel rejects duplicates");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);

    tree_add_voxel(&tree, 5, 10, 5, VOXEL_BRANCH);
    bool result = tree_add_voxel(&tree, 5, 10, 5, VOXEL_LEAF);

    if (!result && tree.voxel_count == 1) {
        PASS();
    } else {
        FAIL("Should reject duplicate position");
    }
}

void test_voxel_exists(void) {
    TEST("tree_voxel_exists");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);

    tree_add_voxel(&tree, 3, 7, 2, VOXEL_LEAF);

    bool exists = tree_voxel_exists(&tree, 3, 7, 2);
    bool not_exists = tree_voxel_exists(&tree, 3, 7, 3);

    if (exists && !not_exists) {
        PASS();
    } else {
        FAIL("Should find existing voxel, not find non-existing");
    }
}

void test_voxel_height_bounds(void) {
    TEST("tree_add_voxel height bounds");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);

    bool below = tree_add_voxel(&tree, 0, -1, 0, VOXEL_TRUNK);
    bool above = tree_add_voxel(&tree, 0, MAX_TREE_HEIGHT + 1, 0, VOXEL_LEAF);
    bool valid = tree_add_voxel(&tree, 0, 50, 0, VOXEL_BRANCH);

    if (!below && !above && valid) {
        PASS();
    } else {
        FAIL("Should reject out-of-bounds heights");
    }
}

void test_voxel_capacity(void) {
    TEST("tree_add_voxel capacity limit");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);

    // Fill to capacity
    int added = 0;
    for (int x = -50; x < 50 && added < MAX_VOXELS_PER_TREE; x++) {
        for (int z = -50; z < 50 && added < MAX_VOXELS_PER_TREE; z++) {
            for (int y = 0; y < MAX_TREE_HEIGHT && added < MAX_VOXELS_PER_TREE; y++) {
                if (tree_add_voxel(&tree, x, y, z, VOXEL_BRANCH)) {
                    added++;
                }
            }
        }
    }

    // Try to add one more
    bool overflow = tree_add_voxel(&tree, 99, 99, 99, VOXEL_LEAF);

    if (!overflow && tree.voxel_count == MAX_VOXELS_PER_TREE) {
        PASS();
    } else {
        FAIL("Should reject voxels at capacity");
    }
}

void test_voxel_type_counts(void) {
    TEST("tree voxel type counting");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);

    tree_add_voxel(&tree, 0, 0, 0, VOXEL_TRUNK);
    tree_add_voxel(&tree, 0, 1, 0, VOXEL_TRUNK);
    tree_add_voxel(&tree, 1, 2, 0, VOXEL_BRANCH);
    tree_add_voxel(&tree, 2, 3, 0, VOXEL_BRANCH);
    tree_add_voxel(&tree, 3, 4, 0, VOXEL_BRANCH);
    tree_add_voxel(&tree, 0, 5, 0, VOXEL_LEAF);

    if (tree.trunk_count == 2 && tree.branch_count == 3 && tree.leaf_count == 1) {
        PASS();
    } else {
        FAIL("Type counts should match added voxels");
    }
}

// ============ TREE INIT TESTS ============

void test_tree_init_basic(void) {
    TEST("tree_init basic initialization");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));

    tree_init(&tree, 10, 5, 20, TREE_SPACE_COLONIZATION);

    if (tree.active && tree.base_x == 10 && tree.base_y == 5 && tree.base_z == 20 &&
        tree.algorithm == TREE_SPACE_COLONIZATION && tree.voxel_count > 0) {
        PASS();
    } else {
        FAIL("Tree should be initialized with correct values");
    }
}

void test_tree_init_has_trunk(void) {
    TEST("tree_init creates trunk voxels");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));

    tree_init(&tree, 0, 0, 0, TREE_SPACE_COLONIZATION);

    if (tree.trunk_count > 0) {
        PASS();
    } else {
        FAIL("Initialized tree should have trunk voxels");
    }
}

void test_tree_grow(void) {
    TEST("tree_grow adds voxels");

    Tree tree;
    memset(&tree, 0, sizeof(Tree));

    tree_init(&tree, 0, 0, 0, TREE_SPACE_COLONIZATION);
    int initial_count = tree.voxel_count;

    for (int i = 0; i < 10; i++) {
        tree_grow(&tree);
    }

    if (tree.voxel_count > initial_count) {
        PASS();
    } else {
        FAIL("Growing tree should add voxels");
    }
}

// ============ MAIN ============

int main(void) {
    printf("\n=== Tree Module Tests ===\n\n");

    printf("Spatial Hash Tests:\n");
    test_hash_pack_key();
    test_hash_negative_coords();
    test_hash_index_distribution();

    printf("\nVoxel Tests:\n");
    test_voxel_add_basic();
    test_voxel_duplicate_rejected();
    test_voxel_exists();
    test_voxel_height_bounds();
    test_voxel_capacity();
    test_voxel_type_counts();

    printf("\nTree Init Tests:\n");
    test_tree_init_basic();
    test_tree_init_has_trunk();
    test_tree_grow();

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
