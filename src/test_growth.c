#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Test configuration
#define TEST_GROWTH_ITERATIONS 200
#define MIN_CROWN_SPREAD_RATIO 0.3f    // Crown width should be at least 30% of height
#define MIN_HORIZONTAL_STDDEV 3.0f      // Minimum spread in X/Z directions
#define MAX_VERTICAL_CONCENTRATION 0.7f // No more than 70% voxels in center column
#define MIN_BRANCH_VOXEL_RATIO 0.15f    // At least 15% should be branch type
#define MIN_LEAF_VOXEL_RATIO 0.20f      // At least 20% should be leaf type

typedef struct {
    float crown_spread_ratio;
    float horizontal_stddev_x;
    float horizontal_stddev_z;
    float center_column_ratio;
    float trunk_ratio;
    float branch_ratio;
    float leaf_ratio;
    int total_voxels;
    int max_height;
    int max_spread_x;
    int max_spread_z;
} TreeStats;

// Helper to add voxel with duplicate check
static void test_add_voxel(Tree *tree, int x, int y, int z, VoxelType type) {
    if (tree->voxel_count >= MAX_VOXELS_PER_TREE) return;
    for (int v = 0; v < tree->voxel_count; v++) {
        if (tree->voxels[v].active && tree->voxels[v].x == x &&
            tree->voxels[v].y == y && tree->voxels[v].z == z) return;
    }
    tree->voxels[tree->voxel_count].x = x;
    tree->voxels[tree->voxel_count].y = y;
    tree->voxels[tree->voxel_count].z = z;
    tree->voxels[tree->voxel_count].type = type;
    tree->voxels[tree->voxel_count].active = true;
    tree->voxel_count++;
}

// Simulate tree growth without graphics
static void simulate_growth(Tree *tree, int iterations) {
    for (int i = 0; i < iterations; i++) {
        float influence_radius = 15.0f;
        float kill_radius = 4.0f;

        for (int b = 0; b < tree->branch_count; b++) {
            GrowthTip *tip = &tree->branches[b];
            if (!tip->active) continue;

            // Find CLOSEST attractor
            float closest_dist = 9999.0f;
            int closest_idx = -1;
            float closest_dx = 0, closest_dy = 0, closest_dz = 0;

            for (int a = 0; a < tree->attractor_count; a++) {
                Attractor *attr = &tree->attractors[a];
                if (!attr->active) continue;

                float dx = attr->x - tip->x;
                float dy = attr->y - tip->y;
                float dz = attr->z - tip->z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);

                if (dist < kill_radius) {
                    attr->active = false;
                    // Add small leaf cluster
                    test_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    test_add_voxel(tree, (int)tip->x + 1, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    test_add_voxel(tree, (int)tip->x - 1, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    test_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z + 1, VOXEL_LEAF);
                    test_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z - 1, VOXEL_LEAF);
                } else if (dist < influence_radius && dist < closest_dist) {
                    closest_dist = dist;
                    closest_idx = a;
                    closest_dx = dx;
                    closest_dy = dy;
                    closest_dz = dz;
                }
            }

            if (closest_idx >= 0) {
                float len = sqrtf(closest_dx*closest_dx + closest_dy*closest_dy + closest_dz*closest_dz);
                if (len > 0) {
                    // Horizontal bias
                    float horiz_bias = 1.2f;
                    tip->x += (closest_dx / len) * horiz_bias;
                    tip->y += (closest_dy / len) * 0.7f;
                    tip->z += (closest_dz / len) * horiz_bias;

                    // Add BRANCH voxels - thicker branches
                    test_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z, VOXEL_BRANCH);
                    test_add_voxel(tree, (int)tip->x + 1, (int)tip->y, (int)tip->z, VOXEL_BRANCH);
                    test_add_voxel(tree, (int)tip->x - 1, (int)tip->y, (int)tip->z, VOXEL_BRANCH);
                    test_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z + 1, VOXEL_BRANCH);
                    test_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z - 1, VOXEL_BRANCH);

                    // Add leaves only at outer edges
                    float dist_from_center = sqrtf(tip->x*tip->x + tip->z*tip->z);
                    if (dist_from_center > 12 && (float)rand() / RAND_MAX < 0.3f) {
                        test_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                        test_add_voxel(tree, (int)tip->x + 1, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                        test_add_voxel(tree, (int)tip->x - 1, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                        test_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z + 1, VOXEL_LEAF);
                        test_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z - 1, VOXEL_LEAF);
                    }

                    // Branch spawning - outward
                    if ((float)rand() / RAND_MAX < 0.25f && tree->branch_count < MAX_TIPS_PER_TREE) {
                        GrowthTip *new_tip = &tree->branches[tree->branch_count++];
                        new_tip->x = tip->x;
                        new_tip->y = tip->y;
                        new_tip->z = tip->z;
                        float branch_angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
                        float outward_dir = sqrtf(tip->x*tip->x + tip->z*tip->z);
                        if (outward_dir > 0.1f) {
                            new_tip->dx = (tip->x / outward_dir) * 0.6f + cosf(branch_angle) * 0.4f;
                            new_tip->dz = (tip->z / outward_dir) * 0.6f + sinf(branch_angle) * 0.4f;
                        } else {
                            new_tip->dx = cosf(branch_angle) * 0.8f;
                            new_tip->dz = sinf(branch_angle) * 0.8f;
                        }
                        new_tip->dy = 0.2f + (float)rand() / RAND_MAX * 0.3f;
                        new_tip->generation = tip->generation + 1;
                        new_tip->active = true;
                    }
                }
            } else {
                // No attractors - add small leaf tuft
                for (int lx = -1; lx <= 1; lx++) {
                    for (int lz = -1; lz <= 1; lz++) {
                        test_add_voxel(tree, (int)tip->x + lx, (int)tip->y, (int)tip->z + lz, VOXEL_LEAF);
                        test_add_voxel(tree, (int)tip->x + lx, (int)tip->y + 1, (int)tip->z + lz, VOXEL_LEAF);
                    }
                }
                tip->active = false;
            }
        }
    }
}

static TreeStats analyze_tree(Tree *tree) {
    TreeStats stats = {0};

    if (tree->voxel_count == 0) return stats;

    int trunk_count = 0, branch_count = 0, leaf_count = 0;
    int min_x = 9999, max_x = -9999;
    int min_z = 9999, max_z = -9999;
    int max_y = 0;
    float sum_x = 0, sum_z = 0;
    int center_column_count = 0;

    // First pass: basic stats
    for (int i = 0; i < tree->voxel_count; i++) {
        if (!tree->voxels[i].active) continue;

        int x = tree->voxels[i].x;
        int y = tree->voxels[i].y;
        int z = tree->voxels[i].z;

        stats.total_voxels++;
        sum_x += x;
        sum_z += z;

        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (z < min_z) min_z = z;
        if (z > max_z) max_z = z;
        if (y > max_y) max_y = y;

        // Count voxels in center column (within 2 units of center)
        if (abs(x) <= 2 && abs(z) <= 2) center_column_count++;

        switch (tree->voxels[i].type) {
            case VOXEL_TRUNK: trunk_count++; break;
            case VOXEL_BRANCH: branch_count++; break;
            case VOXEL_LEAF: leaf_count++; break;
        }
    }

    if (stats.total_voxels == 0) return stats;

    float mean_x = sum_x / stats.total_voxels;
    float mean_z = sum_z / stats.total_voxels;

    // Second pass: calculate standard deviation
    float var_x = 0, var_z = 0;
    for (int i = 0; i < tree->voxel_count; i++) {
        if (!tree->voxels[i].active) continue;
        float dx = tree->voxels[i].x - mean_x;
        float dz = tree->voxels[i].z - mean_z;
        var_x += dx * dx;
        var_z += dz * dz;
    }

    stats.horizontal_stddev_x = sqrtf(var_x / stats.total_voxels);
    stats.horizontal_stddev_z = sqrtf(var_z / stats.total_voxels);
    stats.max_height = max_y;
    stats.max_spread_x = max_x - min_x;
    stats.max_spread_z = max_z - min_z;

    // Crown spread ratio: average spread / height
    float avg_spread = (stats.max_spread_x + stats.max_spread_z) / 2.0f;
    stats.crown_spread_ratio = (max_y > 0) ? avg_spread / (float)max_y : 0;

    stats.center_column_ratio = (float)center_column_count / stats.total_voxels;
    stats.trunk_ratio = (float)trunk_count / stats.total_voxels;
    stats.branch_ratio = (float)branch_count / stats.total_voxels;
    stats.leaf_ratio = (float)leaf_count / stats.total_voxels;

    return stats;
}

static void print_stats(const char *label, TreeStats *stats) {
    printf("\n=== %s ===\n", label);
    printf("Total voxels:        %d\n", stats->total_voxels);
    printf("Max height:          %d\n", stats->max_height);
    printf("Max spread X:        %d\n", stats->max_spread_x);
    printf("Max spread Z:        %d\n", stats->max_spread_z);
    printf("Crown spread ratio:  %.3f (min: %.3f)\n", stats->crown_spread_ratio, MIN_CROWN_SPREAD_RATIO);
    printf("Horizontal stddev X: %.3f (min: %.3f)\n", stats->horizontal_stddev_x, MIN_HORIZONTAL_STDDEV);
    printf("Horizontal stddev Z: %.3f (min: %.3f)\n", stats->horizontal_stddev_z, MIN_HORIZONTAL_STDDEV);
    printf("Center column ratio: %.3f (max: %.3f)\n", stats->center_column_ratio, MAX_VERTICAL_CONCENTRATION);
    printf("Trunk ratio:         %.3f\n", stats->trunk_ratio);
    printf("Branch ratio:        %.3f (min: %.3f)\n", stats->branch_ratio, MIN_BRANCH_VOXEL_RATIO);
    printf("Leaf ratio:          %.3f (min: %.3f)\n", stats->leaf_ratio, MIN_LEAF_VOXEL_RATIO);
}

static int run_test(Tree *tree, const char *label) {
    TreeStats stats = analyze_tree(tree);
    print_stats(label, &stats);

    int failures = 0;

    if (stats.crown_spread_ratio < MIN_CROWN_SPREAD_RATIO) {
        printf("  FAIL: Crown spread ratio %.3f < %.3f (too pole-like)\n",
               stats.crown_spread_ratio, MIN_CROWN_SPREAD_RATIO);
        failures++;
    }

    if (stats.horizontal_stddev_x < MIN_HORIZONTAL_STDDEV) {
        printf("  FAIL: X spread stddev %.3f < %.3f (not enough horizontal variation)\n",
               stats.horizontal_stddev_x, MIN_HORIZONTAL_STDDEV);
        failures++;
    }

    if (stats.horizontal_stddev_z < MIN_HORIZONTAL_STDDEV) {
        printf("  FAIL: Z spread stddev %.3f < %.3f (not enough horizontal variation)\n",
               stats.horizontal_stddev_z, MIN_HORIZONTAL_STDDEV);
        failures++;
    }

    if (stats.center_column_ratio > MAX_VERTICAL_CONCENTRATION) {
        printf("  FAIL: Center column ratio %.3f > %.3f (too concentrated in center)\n",
               stats.center_column_ratio, MAX_VERTICAL_CONCENTRATION);
        failures++;
    }

    if (stats.branch_ratio < MIN_BRANCH_VOXEL_RATIO) {
        printf("  FAIL: Branch ratio %.3f < %.3f (not enough branching)\n",
               stats.branch_ratio, MIN_BRANCH_VOXEL_RATIO);
        failures++;
    }

    if (stats.leaf_ratio < MIN_LEAF_VOXEL_RATIO) {
        printf("  FAIL: Leaf ratio %.3f < %.3f (not enough leaves)\n",
               stats.leaf_ratio, MIN_LEAF_VOXEL_RATIO);
        failures++;
    }

    if (failures == 0) {
        printf("  PASS: All distribution checks passed\n");
    }

    return failures;
}

// Initialize tree for space colonization (copied from game.c logic)
static void init_test_tree(Tree *tree) {
    memset(tree, 0, sizeof(Tree));
    tree->active = true;
    tree->algorithm = TREE_SPACE_COLONIZATION;

    // Create attraction points in a wide crown shape
    for (int i = 0; i < MAX_ATTRACTORS; i++) {
        float angle = (float)rand() / RAND_MAX * 2.0f * 3.14159f;
        float height = 20.0f + (float)rand() / RAND_MAX * 60.0f;
        // Wide crown - radius increases with height
        float min_radius = 5.0f + height * 0.2f;
        float max_radius = 15.0f + height * 0.4f;
        float radius = min_radius + (float)rand() / RAND_MAX * (max_radius - min_radius);

        tree->attractors[i].x = cosf(angle) * radius;
        tree->attractors[i].y = height;
        tree->attractors[i].z = sinf(angle) * radius;
        tree->attractors[i].active = true;
        tree->attractor_count++;
    }

    // Initialize 8 branch tips pointing OUTWARD
    int num_initial_branches = 8;
    for (int i = 0; i < num_initial_branches; i++) {
        float angle = (float)i * 2.0f * 3.14159f / num_initial_branches;
        float outward = 3.0f;
        tree->branches[i].x = cosf(angle) * outward;
        tree->branches[i].y = 18;
        tree->branches[i].z = sinf(angle) * outward;
        tree->branches[i].dx = cosf(angle) * 0.7f;
        tree->branches[i].dy = 0.5f;
        tree->branches[i].dz = sinf(angle) * 0.7f;
        tree->branches[i].generation = 0;
        tree->branches[i].active = true;
        tree->branch_count++;
    }

    // Build trunk (shorter)
    for (int y = 0; y < 20; y++) {
        int trunk_radius = (y < 5) ? 2 : (y < 12) ? 1 : 0;
        for (int tx = -trunk_radius; tx <= trunk_radius; tx++) {
            for (int tz = -trunk_radius; tz <= trunk_radius; tz++) {
                if (tx*tx + tz*tz <= trunk_radius*trunk_radius + 1) {
                    tree->voxels[tree->voxel_count].x = tx;
                    tree->voxels[tree->voxel_count].y = y;
                    tree->voxels[tree->voxel_count].z = tz;
                    tree->voxels[tree->voxel_count].type = VOXEL_TRUNK;
                    tree->voxels[tree->voxel_count].active = true;
                    tree->voxel_count++;
                }
            }
        }
    }
}

int main(void) {
    printf("Tree Growth Distribution Test\n");
    printf("==============================\n");

    srand(42);  // Fixed seed for reproducibility

    int total_failures = 0;

    // Test multiple trees
    for (int t = 0; t < 3; t++) {
        Tree tree;
        init_test_tree(&tree);

        printf("\nInitial voxel count: %d\n", tree.voxel_count);
        printf("Attractor count: %d\n", tree.attractor_count);
        printf("Branch tip count: %d\n", tree.branch_count);

        // Simulate growth
        simulate_growth(&tree, TEST_GROWTH_ITERATIONS);

        char label[32];
        snprintf(label, sizeof(label), "Tree %d (seed offset %d)", t + 1, t);
        total_failures += run_test(&tree, label);
    }

    printf("\n==============================\n");
    if (total_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("TOTAL FAILURES: %d\n", total_failures);
        return 1;
    }
}
