#include "tree.h"
#include "attractor_octree.h"
#include <math.h>
#include <stdlib.h>

// ============ HELPER FUNCTIONS ============

static float randf(void) {
    return (float)GetRandomValue(0, 10000) / 10000.0f;
}

static float randf_range(float min, float max) {
    return min + randf() * (max - min);
}

// Pack (x,y,z) into a single int for hashing
// x,z range: roughly -50 to 50, y range: 0 to 120
// Offset to make positive: x+100, z+100, y stays positive
int tree_pack_key(int x, int y, int z) {
    return ((x + 100) << 16) | ((z + 100) << 8) | y;
}

// Hash function for spatial hash
int tree_hash_index(int key) {
    unsigned int h = (unsigned int)key;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return (int)(h % VOXEL_HASH_SIZE);
}

// Check if voxel exists in hash (O(1) average)
bool tree_voxel_exists(Tree *tree, int x, int y, int z) {
    int key = tree_pack_key(x, y, z);
    int idx = tree_hash_index(key);

    // Linear probing
    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        int probe = (idx + i) % VOXEL_HASH_SIZE;
        if (tree->voxel_hash[probe].key == -1) {
            return false;
        }
        if (tree->voxel_hash[probe].key == key) {
            return true;
        }
    }
    return false;
}

TreeVoxel *tree_get_voxel_at(Tree *tree, int x, int y, int z) {
    int key = tree_pack_key(x, y, z);
    int idx = tree_hash_index(key);

    // Linear probing
    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        int probe = (idx + i) % VOXEL_HASH_SIZE;
        if (tree->voxel_hash[probe].key == -1) {
            return NULL;
        }
        if (tree->voxel_hash[probe].key == key) {
            int voxel_idx = tree->voxel_hash[probe].voxel_idx;
            return &tree->voxels[voxel_idx];
        }
    }
    return NULL;
}

// Add voxel to hash table
static void tree_hash_insert(Tree *tree, int x, int y, int z, int voxel_idx) {
    int key = tree_pack_key(x, y, z);
    int idx = tree_hash_index(key);

    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        int probe = (idx + i) % VOXEL_HASH_SIZE;
        if (tree->voxel_hash[probe].key == -1) {
            tree->voxel_hash[probe].key = key;
            tree->voxel_hash[probe].voxel_idx = voxel_idx;
            return;
        }
    }
}

// Clear spatial hash
void tree_hash_clear(Tree *tree) {
    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        tree->voxel_hash[i].key = -1;
    }
    tree->trunk_count = 0;
    tree->branch_count = 0;
    tree->leaf_count = 0;
}

// Add a voxel to a tree
bool tree_add_voxel(Tree *tree, int x, int y, int z, VoxelType type) {
    if (tree->voxel_count >= MAX_VOXELS_PER_TREE) return false;
    if (y > MAX_TREE_HEIGHT || y < 0) return false;

    if (tree_voxel_exists(tree, x, y, z)) {
        return false;
    }

    int idx = tree->voxel_count;
    tree->voxels[idx].x = x;
    tree->voxels[idx].y = y;
    tree->voxels[idx].z = z;
    tree->voxels[idx].type = type;
    tree->voxels[idx].burn_state = VOXEL_NORMAL;
    tree->voxels[idx].burn_timer = 0;
    tree->voxels[idx].active = true;
    tree->voxel_count++;

    tree_hash_insert(tree, x, y, z, idx);

    switch (type) {
        case VOXEL_TRUNK: tree->trunk_count++; break;
        case VOXEL_BRANCH: tree->branch_count++; break;
        case VOXEL_LEAF: tree->leaf_count++; break;
    }

    return true;
}

// ============ L-SYSTEM GROWTH ============

static void grow_lsystem(Tree *tree) {
    if (tree->lsystem_iteration >= LSYSTEM_MAX_ITERATIONS) return;

    tree->lsystem_iteration++;

    int max_y = 0;
    for (int i = 0; i < tree->voxel_count; i++) {
        if (tree->voxels[i].active && tree->voxels[i].y > max_y) {
            max_y = tree->voxels[i].y;
        }
    }

    int initial_count = tree->voxel_count;
    for (int i = 0; i < initial_count; i++) {
        TreeVoxel *v = &tree->voxels[i];
        if (!v->active || v->y < max_y - 2) continue;

        if (v->x == 0 && v->z == 0 && v->y < 20) {
            tree_add_voxel(tree, 0, v->y + 1, 0, VOXEL_TRUNK);
            if (v->y < 5) {
                tree_add_voxel(tree, 1, v->y, 0, VOXEL_TRUNK);
                tree_add_voxel(tree, -1, v->y, 0, VOXEL_TRUNK);
                tree_add_voxel(tree, 0, v->y, 1, VOXEL_TRUNK);
                tree_add_voxel(tree, 0, v->y, -1, VOXEL_TRUNK);
            }
        }

        float branch_chance = LSYSTEM_BRANCH_CHANCE_BASE + (float)v->y * LSYSTEM_BRANCH_CHANCE_PER_HEIGHT;
        if (randf() < branch_chance && v->y > 5) {
            int dx = GetRandomValue(-1, 1);
            int dz = GetRandomValue(-1, 1);
            if (dx != 0 || dz != 0) {
                for (int len = 1; len <= GetRandomValue(2, 5); len++) {
                    int bx = v->x + dx * len;
                    int bz = v->z + dz * len;
                    int by = v->y + len / 2;
                    tree_add_voxel(tree, bx, by, bz, VOXEL_BRANCH);

                    if (len >= 2 && randf() < 0.6f) {
                        tree_add_voxel(tree, bx, by + 1, bz, VOXEL_LEAF);
                        tree_add_voxel(tree, bx + 1, by, bz, VOXEL_LEAF);
                        tree_add_voxel(tree, bx - 1, by, bz, VOXEL_LEAF);
                        tree_add_voxel(tree, bx, by, bz + 1, VOXEL_LEAF);
                        tree_add_voxel(tree, bx, by, bz - 1, VOXEL_LEAF);
                    }
                }
            }
        }

        if (v->y > 15 && randf() < 0.3f) {
            tree_add_voxel(tree, v->x, v->y + 1, v->z, VOXEL_LEAF);
        }
    }
}

// ============ SPACE COLONIZATION ============

static void init_space_colonization(Tree *tree) {
    tree->attractor_count = 0;

    // Central column attractors
    for (int i = 0; i < 60; i++) {
        float height = randf_range(SC_CROWN_HEIGHT_MIN, SC_CROWN_HEIGHT_MAX);
        float radius = randf_range(0, 2);
        float angle = randf() * 2.0f * PI;
        tree->attractors[tree->attractor_count].x = cosf(angle) * radius;
        tree->attractors[tree->attractor_count].y = height;
        tree->attractors[tree->attractor_count].z = sinf(angle) * radius;
        tree->attractors[tree->attractor_count].active = true;
        tree->attractor_count++;
    }

    // Oak crown attractors
    for (int i = 0; i < MAX_ATTRACTORS - 60; i++) {
        float height = randf_range(25, 55);
        float min_radius = 6.0f + (height - 25) * 0.3f;
        float max_radius = 15.0f + (height - 25) * 0.8f;
        if (max_radius > SC_CROWN_SPREAD_MAX) max_radius = SC_CROWN_SPREAD_MAX;
        float radius = randf_range(min_radius, max_radius);
        float angle = randf() * 2.0f * PI;

        tree->attractors[tree->attractor_count].x = cosf(angle) * radius;
        tree->attractors[tree->attractor_count].y = height;
        tree->attractors[tree->attractor_count].z = sinf(angle) * radius;
        tree->attractors[tree->attractor_count].active = true;
        tree->attractor_count++;
    }

    // Pre-spawn main branches
    tree->sc_branch_count = 0;
    int num_main_branches = SC_MAIN_BRANCHES_MIN + (rand() % (SC_MAIN_BRANCHES_MAX - SC_MAIN_BRANCHES_MIN + 1));
    for (int i = 0; i < num_main_branches; i++) {
        if (tree->sc_branch_count >= MAX_TIPS_PER_TREE) break;
        GrowthTip *branch = &tree->sc_branches[tree->sc_branch_count++];
        float angle = (2.0f * PI * i / num_main_branches) + randf_range(-0.3f, 0.3f);
        float height = SC_BRANCH_HEIGHT_MIN + i * 5.0f + randf_range(-2, 2);

        branch->x = cosf(angle) * 2.0f;
        branch->y = height;
        branch->z = sinf(angle) * 2.0f;
        branch->dx = cosf(angle) * 1.0f;
        branch->dy = randf_range(-0.1f, 0.15f);
        branch->dz = sinf(angle) * 1.0f;
        branch->generation = 1;
        branch->energy = randf_range(18, 28);
        branch->active = true;
    }

    // Build trunk
    for (int y = 0; y < SC_TRUNK_HEIGHT; y++) {
        int trunk_radius;
        if (y < 4) trunk_radius = 3;
        else if (y < 10) trunk_radius = 2;
        else if (y < 20) trunk_radius = 1;
        else trunk_radius = 0;

        for (int tx = -trunk_radius; tx <= trunk_radius; tx++) {
            for (int tz = -trunk_radius; tz <= trunk_radius; tz++) {
                if (tx*tx + tz*tz <= trunk_radius*trunk_radius + 1) {
                    tree_add_voxel(tree, tx, y, tz, VOXEL_TRUNK);
                }
            }
        }
    }

    // Create attractor octree for fast spatial queries
    OctreeBounds attractor_bounds = {
        -SC_CROWN_SPREAD_MAX - 5, 0, -SC_CROWN_SPREAD_MAX - 5,
        SC_CROWN_SPREAD_MAX + 5, SC_CROWN_HEIGHT_MAX + 20, SC_CROWN_SPREAD_MAX + 5
    };
    tree->attractor_octree = attractor_octree_create(tree->attractors, tree->attractor_count, attractor_bounds);
}

static void grow_space_colonization(Tree *tree) {
    if (tree->voxel_count >= MAX_VOXELS_PER_TREE - 100) return;

    for (int b = 0; b < tree->sc_branch_count; b++) {
        GrowthTip *tip = &tree->sc_branches[b];
        if (!tip->active) continue;

        if (tip->generation > 0 && tip->energy <= 0) {
            for (int lx = -1; lx <= 1; lx++) {
                for (int lz = -1; lz <= 1; lz++) {
                    tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y, (int)tip->z + lz, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y + 1, (int)tip->z + lz, VOXEL_LEAF);
                }
            }
            tip->active = false;
            continue;
        }

        // Use octree for efficient attractor queries
        AttractorOctree *ao = (AttractorOctree *)tree->attractor_octree;
        float closest_dist = 9999.0f;
        int closest_idx = -1;
        float closest_dx = 0, closest_dy = 0, closest_dz = 0;

        // Query attractors within influence radius using octree
        OctreeQueryResult *nearby = octree_result_create(64);
        attractor_octree_query_influence(ao, tip->x, tip->y, tip->z, SC_INFLUENCE_RADIUS, nearby);

        for (int i = 0; i < nearby->count; i++) {
            int a = nearby->indices[i];
            Attractor *attr = &tree->attractors[a];
            if (!attr->active) continue;

            float dx = attr->x - tip->x;
            float dy = attr->y - tip->y;
            float dz = attr->z - tip->z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            if (dist < SC_KILL_RADIUS) {
                attractor_octree_deactivate(ao, a);
                if (tip->generation > 0) {
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    if (randf() < 0.5f) {
                        tree_add_voxel(tree, (int)tip->x + 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x - 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z + 1, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z - 1, VOXEL_LEAF);
                    }
                }
            } else if (dist < closest_dist) {
                closest_dist = dist;
                closest_idx = a;
                closest_dx = dx;
                closest_dy = dy;
                closest_dz = dz;
            }
        }
        octree_result_destroy(nearby);

        if (closest_idx >= 0) {
            float len = sqrtf(closest_dx*closest_dx + closest_dy*closest_dy + closest_dz*closest_dz);
            if (len > 0) {
                float dist_from_center = sqrtf(tip->x*tip->x + tip->z*tip->z);
                bool is_trunk = (dist_from_center < 2.0f && tip->generation == 0);
                bool is_branch = (tip->generation > 0);

                float prev_x = tip->x;
                float prev_y = tip->y;
                float prev_z = tip->z;

                float step_size = is_trunk ? SC_TRUNK_STEP : SC_BRANCH_STEP;
                float move_dx, move_dy, move_dz;

                if (is_trunk) {
                    move_dx = (closest_dx / len) * 0.3f * step_size;
                    move_dy = (closest_dy / len) * 1.0f * step_size;
                    move_dz = (closest_dz / len) * 0.3f * step_size;
                } else {
                    float old_len = sqrtf(tip->dx*tip->dx + tip->dy*tip->dy + tip->dz*tip->dz);
                    if (old_len > 0.01f) {
                        move_dx = (tip->dx / old_len * SC_MOMENTUM + closest_dx / len * (1-SC_MOMENTUM)) * step_size;
                        move_dy = (tip->dy / old_len * SC_MOMENTUM + closest_dy / len * 0.1f) * step_size;
                        move_dz = (tip->dz / old_len * SC_MOMENTUM + closest_dz / len * (1-SC_MOMENTUM)) * step_size;
                    } else {
                        move_dx = (closest_dx / len) * step_size;
                        move_dy = 0.05f * step_size;
                        move_dz = (closest_dz / len) * step_size;
                    }
                    tip->energy -= 1.0f;
                }

                tip->x += move_dx;
                tip->y += move_dy;
                tip->z += move_dz;

                VoxelType type = is_trunk ? VOXEL_TRUNK : VOXEL_BRANCH;

                float dx = tip->x - prev_x;
                float dy = tip->y - prev_y;
                float dz = tip->z - prev_z;
                float move_len = sqrtf(dx*dx + dy*dy + dz*dz);
                int steps = (int)(move_len / 0.4f) + 1;

                for (int s = 0; s <= steps; s++) {
                    float t = (float)s / (float)steps;
                    int vx = (int)(prev_x + dx * t);
                    int vy = (int)(prev_y + dy * t);
                    int vz = (int)(prev_z + dz * t);

                    tree_add_voxel(tree, vx, vy, vz, type);

                    if (is_trunk || dist_from_center < 5) {
                        tree_add_voxel(tree, vx + 1, vy, vz, type);
                        tree_add_voxel(tree, vx - 1, vy, vz, type);
                        tree_add_voxel(tree, vx, vy, vz + 1, type);
                        tree_add_voxel(tree, vx, vy, vz - 1, type);
                    }
                }

                if (is_branch && dist_from_center > SC_LEAF_DISTANCE && randf() < 0.5f) {
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x + 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x - 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z + 1, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z - 1, VOXEL_LEAF);
                }

                tip->dx = move_dx;
                tip->dy = move_dy;
                tip->dz = move_dz;

                bool can_branch = false;
                if (is_trunk && tip->y > 12) {
                    can_branch = (randf() < SC_BRANCH_CHANCE_TRUNK);
                } else if (is_branch) {
                    float sub_chance = SC_BRANCH_CHANCE_BASE + (dist_from_center * 0.01f);
                    can_branch = (randf() < sub_chance);
                }

                if (can_branch && tree->sc_branch_count < MAX_TIPS_PER_TREE) {
                    GrowthTip *new_tip = &tree->sc_branches[tree->sc_branch_count++];
                    new_tip->x = tip->x;
                    new_tip->y = tip->y;
                    new_tip->z = tip->z;

                    float branch_angle = randf() * 2.0f * PI;
                    new_tip->dx = cosf(branch_angle) * 1.0f;
                    new_tip->dy = randf_range(-0.1f, 0.1f);
                    new_tip->dz = sinf(branch_angle) * 1.0f;
                    new_tip->generation = tip->generation + 1;
                    new_tip->active = true;
                    new_tip->energy = (tip->generation == 0) ? randf_range(15, 25) : randf_range(8, 15);
                }
            }
        } else {
            if (tip->generation > 0) {
                for (int lx = -1; lx <= 1; lx++) {
                    for (int lz = -1; lz <= 1; lz++) {
                        tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y, (int)tip->z + lz, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y + 1, (int)tip->z + lz, VOXEL_LEAF);
                    }
                }
            }
            tip->active = false;
        }
    }
}

// ============ AGENT-BASED GROWTH ============

static void init_agent_tips(Tree *tree) {
    for (int y = 0; y < 6; y++) {
        tree_add_voxel(tree, 0, y, 0, VOXEL_TRUNK);
        if (y < 3) {
            tree_add_voxel(tree, 1, y, 0, VOXEL_TRUNK);
            tree_add_voxel(tree, -1, y, 0, VOXEL_TRUNK);
            tree_add_voxel(tree, 0, y, 1, VOXEL_TRUNK);
            tree_add_voxel(tree, 0, y, -1, VOXEL_TRUNK);
        }
    }

    tree->tips[0].x = 0;
    tree->tips[0].y = 6;
    tree->tips[0].z = 0;
    tree->tips[0].dx = 0;
    tree->tips[0].dy = 1;
    tree->tips[0].dz = 0;
    tree->tips[0].energy = AGENT_INITIAL_ENERGY;
    tree->tips[0].generation = 0;
    tree->tips[0].active = true;
    tree->tip_count = 1;

    for (int i = 1; i <= 3; i++) {
        float angle = (float)i * 2.0f * PI / 3.0f;
        tree->tips[i].x = 0;
        tree->tips[i].y = 5;
        tree->tips[i].z = 0;
        tree->tips[i].dx = cosf(angle) * 0.7f;
        tree->tips[i].dy = 0.5f;
        tree->tips[i].dz = sinf(angle) * 0.7f;
        tree->tips[i].energy = AGENT_BRANCH_ENERGY;
        tree->tips[i].generation = 1;
        tree->tips[i].active = true;
        tree->tip_count++;
    }
}

static void grow_agent_tips(Tree *tree) {
    for (int i = 0; i < tree->tip_count; i++) {
        GrowthTip *tip = &tree->tips[i];
        if (!tip->active || tip->energy <= 0) {
            if (tip->active && tip->y > 8) {
                for (int lx = -1; lx <= 1; lx++) {
                    for (int lz = -1; lz <= 1; lz++) {
                        tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y, (int)tip->z + lz, VOXEL_LEAF);
                    }
                }
            }
            tip->active = false;
            continue;
        }

        tip->dx += randf_range(-0.2f, 0.2f);
        tip->dz += randf_range(-0.2f, 0.2f);

        float upward_bias = AGENT_UPWARD_BIAS_BASE - tip->generation * AGENT_UPWARD_BIAS_DECAY;
        if (upward_bias < 0.3f) upward_bias = 0.3f;
        tip->dy = upward_bias + randf_range(-0.1f, 0.1f);

        float len = sqrtf(tip->dx*tip->dx + tip->dy*tip->dy + tip->dz*tip->dz);
        if (len > 0) {
            tip->dx /= len;
            tip->dy /= len;
            tip->dz /= len;
        }

        tip->x += tip->dx;
        tip->y += tip->dy;
        tip->z += tip->dz;
        tip->energy -= 1.0f;

        VoxelType type = VOXEL_BRANCH;
        if (tip->generation == 0 && fabsf(tip->x) < 2 && fabsf(tip->z) < 2) {
            type = VOXEL_TRUNK;
        }

        tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z, type);

        float branch_chance = AGENT_BRANCH_CHANCE_BASE + tip->generation * AGENT_BRANCH_CHANCE_PER_GEN;
        if (tip->energy > 8 && randf() < branch_chance && tree->tip_count < MAX_TIPS_PER_TREE) {
            GrowthTip *new_tip = &tree->tips[tree->tip_count++];
            new_tip->x = tip->x;
            new_tip->y = tip->y;
            new_tip->z = tip->z;

            float angle = randf() * 2.0f * PI;
            new_tip->dx = cosf(angle) * 0.8f;
            new_tip->dy = 0.4f;
            new_tip->dz = sinf(angle) * 0.8f;
            new_tip->energy = tip->energy * 0.5f;
            new_tip->generation = tip->generation + 1;
            new_tip->active = true;

            tip->energy *= 0.75f;
        }

        if (tip->generation > 0 && tip->y > 10 && randf() < 0.25f) {
            tree_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
        }
    }
}

// ============ PUBLIC FUNCTIONS ============

void tree_init(Tree *tree, int base_x, int base_y, int base_z, TreeAlgorithm algorithm) {
    tree->base_x = base_x;
    tree->base_y = base_y;
    tree->base_z = base_z;
    tree->algorithm = algorithm;
    tree->active = true;
    tree->voxel_count = 0;
    tree->lsystem_iteration = 0;
    tree->attractor_count = 0;
    tree->sc_branch_count = 0;
    tree->tip_count = 0;
    tree->attractor_octree = NULL;

    tree_hash_clear(tree);

    tree_add_voxel(tree, 0, 0, 0, VOXEL_TRUNK);
    tree_add_voxel(tree, 0, 1, 0, VOXEL_TRUNK);

    switch (algorithm) {
        case TREE_LSYSTEM:
            break;
        case TREE_SPACE_COLONIZATION:
            init_space_colonization(tree);
            break;
        case TREE_AGENT_TIPS:
            init_agent_tips(tree);
            break;
    }
}

void tree_grow(Tree *tree) {
    if (!tree->active) return;

    switch (tree->algorithm) {
        case TREE_LSYSTEM:
            grow_lsystem(tree);
            break;
        case TREE_SPACE_COLONIZATION:
            grow_space_colonization(tree);
            break;
        case TREE_AGENT_TIPS:
            grow_agent_tips(tree);
            break;
    }
}
