#include "terrain.h"
#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  Testing: %s... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)

// ============ TERRAIN GENERATION TESTS ============

void test_terrain_generate_bounds(void) {
    TEST("terrain_generate height bounds");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    terrain_generate(height);

    int min_height = 9999;
    int max_height = -9999;

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (height[x][z] < min_height) min_height = height[x][z];
            if (height[x][z] > max_height) max_height = height[x][z];
        }
    }

    // Heights should be reasonable (0-15 range based on the algorithm)
    if (min_height >= 0 && max_height <= 20 && max_height > min_height) {
        PASS();
    } else {
        FAIL("Heights should be in reasonable range with variation");
    }
}

void test_terrain_generate_variation(void) {
    TEST("terrain_generate has hills and valleys");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    terrain_generate(height);

    int below_water = 0;
    int above_water = 0;

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (height[x][z] < WATER_LEVEL) below_water++;
            else above_water++;
        }
    }

    // Should have both land and water areas
    if (below_water > 0 && above_water > 0) {
        PASS();
    } else {
        FAIL("Terrain should have both water and land areas");
    }
}

// ============ BURN INIT TESTS ============

void test_burn_init(void) {
    TEST("terrain_burn_init initializes to normal");

    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // Set to non-zero first
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            burn[x][z] = TERRAIN_BURNED;
            timers[x][z] = 99.0f;
        }
    }

    terrain_burn_init(burn, timers);

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (burn[x][z] != TERRAIN_NORMAL || timers[x][z] != 0) {
                FAIL("All cells should be TERRAIN_NORMAL with timer 0");
                return;
            }
        }
    }

    PASS();
}

// ============ BURN SPREAD TESTS ============

void test_burn_spreads_to_neighbors(void) {
    TEST("terrain_burn_update spreads fire");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // Initialize all to land above water
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            height[x][z] = WATER_LEVEL + 1;
            burn[x][z] = TERRAIN_NORMAL;
            timers[x][z] = 0;
        }
    }

    // Start fire in center
    int cx = TERRAIN_RESOLUTION / 2;
    int cz = TERRAIN_RESOLUTION / 2;
    burn[cx][cz] = TERRAIN_BURNING;
    timers[cx][cz] = BURN_DURATION;

    // Run many updates to ensure spread
    for (int i = 0; i < 50; i++) {
        terrain_burn_update(burn, timers, height, NULL, 0);
    }

    // Check if fire spread to any neighbors
    int burning_or_burned = 0;
    for (int dx = -3; dx <= 3; dx++) {
        for (int dz = -3; dz <= 3; dz++) {
            if (burn[cx + dx][cz + dz] != TERRAIN_NORMAL) {
                burning_or_burned++;
            }
        }
    }

    if (burning_or_burned > 1) {
        PASS();
    } else {
        FAIL("Fire should spread to neighboring cells");
    }
}

void test_burn_stops_at_water(void) {
    TEST("terrain_burn_update stops at water");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // Create a water barrier
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (x < TERRAIN_RESOLUTION / 2) {
                height[x][z] = WATER_LEVEL + 1; // Land
            } else {
                height[x][z] = WATER_LEVEL - 1; // Water
            }
            burn[x][z] = TERRAIN_NORMAL;
            timers[x][z] = 0;
        }
    }

    // Start fire on land side near water
    int fire_x = TERRAIN_RESOLUTION / 2 - 2;
    int fire_z = TERRAIN_RESOLUTION / 2;
    burn[fire_x][fire_z] = TERRAIN_BURNING;
    timers[fire_x][fire_z] = BURN_DURATION;

    // Run updates
    for (int i = 0; i < 100; i++) {
        terrain_burn_update(burn, timers, height, NULL, 0);
    }

    // Check water side - should not burn
    int water_burning = 0;
    for (int x = TERRAIN_RESOLUTION / 2 + 1; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (burn[x][z] != TERRAIN_NORMAL) {
                water_burning++;
            }
        }
    }

    if (water_burning == 0) {
        PASS();
    } else {
        FAIL("Fire should not spread into water");
    }
}

void test_burn_transitions_to_burned(void) {
    TEST("terrain_burn_update transitions to burned");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            height[x][z] = WATER_LEVEL + 1;
            burn[x][z] = TERRAIN_NORMAL;
            timers[x][z] = 0;
        }
    }

    int cx = TERRAIN_RESOLUTION / 2;
    int cz = TERRAIN_RESOLUTION / 2;
    burn[cx][cz] = TERRAIN_BURNING;
    timers[cx][cz] = BURN_DURATION;

    // Run until timer expires
    for (int i = 0; i < 20; i++) {
        terrain_burn_update(burn, timers, height, NULL, 0);
    }

    if (burn[cx][cz] == TERRAIN_BURNED) {
        PASS();
    } else {
        FAIL("Burning terrain should transition to burned");
    }
}

// ============ TREE BURNING TESTS ============

void test_burn_ignites_nearby_tree(void) {
    TEST("terrain_burn_update ignites nearby tree");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            height[x][z] = WATER_LEVEL + 1;
            burn[x][z] = TERRAIN_NORMAL;
            timers[x][z] = 0;
        }
    }

    // Create a tree
    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);
    tree.active = true;
    tree.base_x = 10;
    tree.base_z = 10;

    // Add some voxels
    for (int y = 0; y < 10; y++) {
        tree_add_voxel(&tree, 0, y, 0, y < 5 ? VOXEL_TRUNK : VOXEL_LEAF);
    }

    // Start fire near tree (convert tree grid pos to terrain pos)
    int terrain_x = (int)(tree.base_x * 5.0f / TERRAIN_SCALE);
    int terrain_z = (int)(tree.base_z * 5.0f / TERRAIN_SCALE);
    burn[terrain_x][terrain_z] = TERRAIN_BURNING;
    timers[terrain_x][terrain_z] = BURN_DURATION;

    // Run updates
    Tree trees[1] = { tree };
    for (int i = 0; i < 10; i++) {
        terrain_burn_update(burn, timers, height, trees, 1);
    }

    // Check if any voxels are burning
    int burning_voxels = 0;
    for (int v = 0; v < trees[0].voxel_count; v++) {
        if (trees[0].voxels[v].burn_state == VOXEL_BURNING) {
            burning_voxels++;
        }
    }

    if (burning_voxels > 0) {
        PASS();
    } else {
        FAIL("Tree near fire should have burning voxels");
    }
}

void test_burn_removes_leaves(void) {
    TEST("terrain_burn_update removes burned leaves");

    int height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float timers[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            height[x][z] = WATER_LEVEL + 1;
            burn[x][z] = TERRAIN_NORMAL;
            timers[x][z] = 0;
        }
    }

    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);
    tree.active = true;
    tree.base_x = 10;
    tree.base_z = 10;

    // Add leaves that are already burning
    for (int i = 0; i < 5; i++) {
        tree_add_voxel(&tree, i, 5, 0, VOXEL_LEAF);
        tree.voxels[i].burn_state = VOXEL_BURNING;
        tree.voxels[i].burn_timer = 0.01f; // About to burn out
    }

    int initial_leaves = tree.leaf_count;

    Tree trees[1] = { tree };
    for (int i = 0; i < 5; i++) {
        terrain_burn_update(burn, timers, height, trees, 1);
    }

    // Count remaining active leaves
    int remaining_leaves = 0;
    for (int v = 0; v < trees[0].voxel_count; v++) {
        if (trees[0].voxels[v].active && trees[0].voxels[v].type == VOXEL_LEAF) {
            remaining_leaves++;
        }
    }

    if (remaining_leaves < initial_leaves) {
        PASS();
    } else {
        FAIL("Burned leaves should be removed (made inactive)");
    }
}

// ============ REGENERATION TESTS ============

void test_regeneration_with_healthy_tree(void) {
    TEST("terrain_regenerate heals burned terrain");

    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // All burned
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            burn[x][z] = TERRAIN_BURNED;
        }
    }

    // Create healthy tree
    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);
    tree.active = true;
    tree.base_x = 20;
    tree.base_z = 20;

    // Add healthy leaves
    for (int i = 0; i < 10; i++) {
        tree_add_voxel(&tree, i, 10, 0, VOXEL_LEAF);
        // Leaves are VOXEL_NORMAL by default
    }

    Tree trees[1] = { tree };

    // Run regeneration many times
    for (int i = 0; i < 100; i++) {
        terrain_regenerate(burn, trees, 1);
    }

    // Check near tree for regenerated terrain
    int tree_terrain_x = (int)(tree.base_x * 5.0f / TERRAIN_SCALE);
    int tree_terrain_z = (int)(tree.base_z * 5.0f / TERRAIN_SCALE);

    int regenerated = 0;
    for (int dx = -TREE_REGEN_RADIUS; dx <= TREE_REGEN_RADIUS; dx++) {
        for (int dz = -TREE_REGEN_RADIUS; dz <= TREE_REGEN_RADIUS; dz++) {
            int tx = tree_terrain_x + dx;
            int tz = tree_terrain_z + dz;
            if (tx >= 0 && tx < TERRAIN_RESOLUTION && tz >= 0 && tz < TERRAIN_RESOLUTION) {
                if (burn[tx][tz] == TERRAIN_NORMAL) {
                    regenerated++;
                }
            }
        }
    }

    if (regenerated > 0) {
        PASS();
    } else {
        FAIL("Healthy tree should regenerate nearby burned terrain");
    }
}

void test_regeneration_requires_healthy_leaves(void) {
    TEST("terrain_regenerate requires healthy leaves");

    TerrainBurnState burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            burn[x][z] = TERRAIN_BURNED;
        }
    }

    // Create burned tree (no healthy leaves)
    Tree tree;
    memset(&tree, 0, sizeof(Tree));
    tree_hash_clear(&tree);
    tree.active = true;
    tree.base_x = 20;
    tree.base_z = 20;

    // Add burned trunk only
    for (int y = 0; y < 10; y++) {
        tree_add_voxel(&tree, 0, y, 0, VOXEL_TRUNK);
        tree.voxels[y].burn_state = VOXEL_BURNED;
    }

    Tree trees[1] = { tree };

    for (int i = 0; i < 100; i++) {
        terrain_regenerate(burn, trees, 1);
    }

    // Check - should still be all burned
    int still_burned = 0;
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            if (burn[x][z] == TERRAIN_BURNED) still_burned++;
        }
    }

    if (still_burned == TERRAIN_RESOLUTION * TERRAIN_RESOLUTION) {
        PASS();
    } else {
        FAIL("Burned tree without healthy leaves should not regenerate terrain");
    }
}

// ============ MAIN ============

int main(void) {
    printf("\n=== Terrain Module Tests ===\n\n");

    printf("Terrain Generation Tests:\n");
    test_terrain_generate_bounds();
    test_terrain_generate_variation();

    printf("\nBurn Init Tests:\n");
    test_burn_init();

    printf("\nBurn Spread Tests:\n");
    test_burn_spreads_to_neighbors();
    test_burn_stops_at_water();
    test_burn_transitions_to_burned();

    printf("\nTree Burning Tests:\n");
    test_burn_ignites_nearby_tree();
    test_burn_removes_leaves();

    printf("\nRegeneration Tests:\n");
    test_regeneration_with_healthy_tree();
    test_regeneration_requires_healthy_leaves();

    printf("\n=== Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
