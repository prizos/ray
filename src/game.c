#include "game.h"
#include <math.h>
#include <stdlib.h>

// Camera movement speed
#define MOVE_SPEED 50.0f
#define LOOK_SPEED 2.0f

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
static inline int voxel_pack_key(int x, int y, int z) {
    return ((x + 100) << 16) | ((z + 100) << 8) | y;
}

// Hash function for spatial hash
static inline int voxel_hash(int key) {
    // Simple hash with good distribution
    unsigned int h = (unsigned int)key;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return (int)(h % VOXEL_HASH_SIZE);
}

// Check if voxel exists in hash (O(1) average)
static bool voxel_exists(Tree *tree, int x, int y, int z) {
    int key = voxel_pack_key(x, y, z);
    int idx = voxel_hash(key);

    // Linear probing
    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        int probe = (idx + i) % VOXEL_HASH_SIZE;
        if (tree->voxel_hash[probe].key == -1) {
            return false;  // Empty slot, not found
        }
        if (tree->voxel_hash[probe].key == key) {
            return true;   // Found
        }
    }
    return false;
}

// Add voxel to hash table
static void voxel_hash_insert(Tree *tree, int x, int y, int z, int voxel_idx) {
    int key = voxel_pack_key(x, y, z);
    int idx = voxel_hash(key);

    // Linear probing to find empty slot
    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        int probe = (idx + i) % VOXEL_HASH_SIZE;
        if (tree->voxel_hash[probe].key == -1) {
            tree->voxel_hash[probe].key = key;
            tree->voxel_hash[probe].voxel_idx = voxel_idx;
            return;
        }
    }
}

// Initialize hash table (call when creating/resetting tree)
static void voxel_hash_clear(Tree *tree) {
    for (int i = 0; i < VOXEL_HASH_SIZE; i++) {
        tree->voxel_hash[i].key = -1;
    }
    tree->trunk_count = 0;
    tree->branch_count_cached = 0;
    tree->leaf_count = 0;
}

// Add a voxel to a tree (returns false if full or duplicate)
// O(1) average time complexity
static bool tree_add_voxel(Tree *tree, int x, int y, int z, VoxelType type) {
    if (tree->voxel_count >= MAX_VOXELS_PER_TREE) return false;
    if (y > MAX_TREE_HEIGHT || y < 0) return false;

    // O(1) duplicate check using spatial hash
    if (voxel_exists(tree, x, y, z)) {
        return false;
    }

    // Add to voxel array
    int idx = tree->voxel_count;
    tree->voxels[idx].x = x;
    tree->voxels[idx].y = y;
    tree->voxels[idx].z = z;
    tree->voxels[idx].type = type;
    tree->voxels[idx].burn_state = VOXEL_NORMAL;
    tree->voxels[idx].burn_timer = 0;
    tree->voxels[idx].active = true;
    tree->voxel_count++;

    // Add to hash table
    voxel_hash_insert(tree, x, y, z, idx);

    // Update cached counts
    switch (type) {
        case VOXEL_TRUNK: tree->trunk_count++; break;
        case VOXEL_BRANCH: tree->branch_count_cached++; break;
        case VOXEL_LEAF: tree->leaf_count++; break;
    }

    return true;
}

// ============ L-SYSTEM GROWTH ============

static void grow_lsystem(Tree *tree) {
    if (tree->lsystem_iteration >= 25) return;  // More iterations

    tree->lsystem_iteration++;

    // Find the highest points
    int max_y = 0;
    for (int i = 0; i < tree->voxel_count; i++) {
        if (tree->voxels[i].active && tree->voxels[i].y > max_y) {
            max_y = tree->voxels[i].y;
        }
    }

    // Grow from top voxels
    int initial_count = tree->voxel_count;
    for (int i = 0; i < initial_count; i++) {
        TreeVoxel *v = &tree->voxels[i];
        if (!v->active || v->y < max_y - 2) continue;

        // Main trunk grows up (thicker at base)
        if (v->x == 0 && v->z == 0 && v->y < 20) {
            tree_add_voxel(tree, 0, v->y + 1, 0, VOXEL_TRUNK);
            // Thicker trunk base
            if (v->y < 5) {
                tree_add_voxel(tree, 1, v->y, 0, VOXEL_TRUNK);
                tree_add_voxel(tree, -1, v->y, 0, VOXEL_TRUNK);
                tree_add_voxel(tree, 0, v->y, 1, VOXEL_TRUNK);
                tree_add_voxel(tree, 0, v->y, -1, VOXEL_TRUNK);
            }
        }

        // Branch probability increases with height
        float branch_chance = 0.15f + (float)v->y * 0.02f;
        if (randf() < branch_chance && v->y > 5) {
            int dx = GetRandomValue(-1, 1);
            int dz = GetRandomValue(-1, 1);
            if (dx != 0 || dz != 0) {
                // Branch extends outward
                for (int len = 1; len <= GetRandomValue(2, 5); len++) {
                    int bx = v->x + dx * len;
                    int bz = v->z + dz * len;
                    int by = v->y + len / 2;
                    tree_add_voxel(tree, bx, by, bz, VOXEL_BRANCH);

                    // Add leaves at branch ends
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

        // Add leaves at top
        if (v->y > 15 && randf() < 0.3f) {
            tree_add_voxel(tree, v->x, v->y + 1, v->z, VOXEL_LEAF);
        }
    }
}

// ============ SPACE COLONIZATION ============

static void init_space_colonization(Tree *tree) {
    // Oak-style: wide spreading crown, shorter trunk
    tree->attractor_count = 0;

    // Central column attractors - shorter trunk for oak
    for (int i = 0; i < 60; i++) {
        float height = randf_range(15, 45);  // Shorter trunk
        float radius = randf_range(0, 2);
        float angle = randf() * 2.0f * PI;
        tree->attractors[tree->attractor_count].x = cosf(angle) * radius;
        tree->attractors[tree->attractor_count].y = height;
        tree->attractors[tree->attractor_count].z = sinf(angle) * radius;
        tree->attractors[tree->attractor_count].active = true;
        tree->attractor_count++;
    }

    // Oak crown - wide spreading, not too tall
    for (int i = 0; i < MAX_ATTRACTORS - 60; i++) {
        float height = randf_range(25, 55);  // Lower, flatter crown
        // Oak: very wide spread, relatively flat top
        float min_radius = 6.0f + (height - 25) * 0.3f;
        float max_radius = 15.0f + (height - 25) * 0.8f;
        if (max_radius > 30) max_radius = 30;  // Cap max spread
        float radius = randf_range(min_radius, max_radius);
        float angle = randf() * 2.0f * PI;

        tree->attractors[tree->attractor_count].x = cosf(angle) * radius;
        tree->attractors[tree->attractor_count].y = height;
        tree->attractors[tree->attractor_count].z = sinf(angle) * radius;
        tree->attractors[tree->attractor_count].active = true;
        tree->attractor_count++;
    }

    // Pre-spawn 4-6 main branches at different heights (oak style)
    tree->branch_count = 0;
    int num_main_branches = 4 + (rand() % 3);
    for (int i = 0; i < num_main_branches; i++) {
        if (tree->branch_count >= MAX_TIPS_PER_TREE) break;
        GrowthTip *branch = &tree->branches[tree->branch_count++];
        float angle = (2.0f * PI * i / num_main_branches) + randf_range(-0.3f, 0.3f);
        float height = 15.0f + i * 5.0f + randf_range(-2, 2);

        branch->x = cosf(angle) * 2.0f;  // Start slightly away from trunk
        branch->y = height;
        branch->z = sinf(angle) * 2.0f;
        branch->dx = cosf(angle) * 1.0f;
        branch->dy = randf_range(-0.1f, 0.15f);
        branch->dz = sinf(angle) * 1.0f;
        branch->generation = 1;
        branch->energy = randf_range(18, 28);  // Long main branches
        branch->active = true;
    }

    // Build thick trunk up to where branches start
    for (int y = 0; y < 40; y++) {
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
}

static void grow_space_colonization(Tree *tree) {
    // Stop growing if near capacity
    if (tree->voxel_count >= MAX_VOXELS_PER_TREE - 100) return;

    float influence_radius = 15.0f;
    float kill_radius = 3.0f;

    for (int b = 0; b < tree->branch_count; b++) {
        GrowthTip *tip = &tree->branches[b];
        if (!tip->active) continue;

        // Branches die when out of energy
        if (tip->generation > 0 && tip->energy <= 0) {
            // Add leaf cluster at end
            for (int lx = -1; lx <= 1; lx++) {
                for (int lz = -1; lz <= 1; lz++) {
                    tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y, (int)tip->z + lz, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x + lx, (int)tip->y + 1, (int)tip->z + lz, VOXEL_LEAF);
                }
            }
            tip->active = false;
            continue;
        }

        // Find the CLOSEST attractor to this tip
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
                // Add leaves only on branches (generation > 0), not trunk
                if (tip->generation > 0) {
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    if (randf() < 0.5f) {
                        tree_add_voxel(tree, (int)tip->x + 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x - 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z + 1, VOXEL_LEAF);
                        tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z - 1, VOXEL_LEAF);
                    }
                }
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
                float dist_from_center = sqrtf(tip->x*tip->x + tip->z*tip->z);
                bool is_trunk = (dist_from_center < 2.0f && tip->generation == 0);
                bool is_branch = (tip->generation > 0);

                // Store previous position for interpolation
                float prev_x = tip->x;
                float prev_y = tip->y;
                float prev_z = tip->z;

                // Movement direction
                float step_size = is_trunk ? 0.6f : 0.8f;
                float move_dx, move_dy, move_dz;
                if (is_trunk) {
                    // Trunk grows mostly upward toward attractors
                    move_dx = (closest_dx / len) * 0.3f * step_size;
                    move_dy = (closest_dy / len) * 1.0f * step_size;
                    move_dz = (closest_dz / len) * 0.3f * step_size;
                } else {
                    // Branches: mostly follow their own direction (momentum)
                    // Only slightly influenced by attractors for straighter growth
                    float old_len = sqrtf(tip->dx*tip->dx + tip->dy*tip->dy + tip->dz*tip->dz);
                    if (old_len > 0.01f) {
                        // 80% own direction, 20% toward attractor
                        float momentum = 0.8f;
                        move_dx = (tip->dx / old_len * momentum + closest_dx / len * (1-momentum)) * step_size;
                        move_dy = (tip->dy / old_len * momentum + closest_dy / len * 0.1f) * step_size; // Very little vertical
                        move_dz = (tip->dz / old_len * momentum + closest_dz / len * (1-momentum)) * step_size;
                    } else {
                        move_dx = (closest_dx / len) * step_size;
                        move_dy = 0.05f * step_size;
                        move_dz = (closest_dz / len) * step_size;
                    }
                    // Consume energy
                    tip->energy -= 1.0f;
                }

                tip->x += move_dx;
                tip->y += move_dy;
                tip->z += move_dz;

                // Determine voxel type
                VoxelType type = is_trunk ? VOXEL_TRUNK : VOXEL_BRANCH;

                // Interpolate between previous and current position for contiguous branches
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

                    // Thicker trunk/main branches
                    if (is_trunk || dist_from_center < 5) {
                        tree_add_voxel(tree, vx + 1, vy, vz, type);
                        tree_add_voxel(tree, vx - 1, vy, vz, type);
                        tree_add_voxel(tree, vx, vy, vz + 1, type);
                        tree_add_voxel(tree, vx, vy, vz - 1, type);
                    }
                }

                // Add leaves only on branches, further from trunk for oak-style
                if (is_branch && dist_from_center > 10 && randf() < 0.5f) {
                    // Larger leaf clusters on oak
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x + 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x - 1, (int)tip->y, (int)tip->z, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z + 1, VOXEL_LEAF);
                    tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z - 1, VOXEL_LEAF);
                }

                // Update tip direction for next iteration
                tip->dx = move_dx;
                tip->dy = move_dy;
                tip->dz = move_dz;

                // Branch spawning logic
                bool can_branch = false;

                if (is_trunk && tip->y > 12) {
                    // Trunk spawns multiple main branches at intervals
                    can_branch = (randf() < 0.25f);  // Regular branching from trunk
                } else if (is_branch) {
                    // Branches can sub-branch, more likely further out
                    float sub_chance = 0.12f + (dist_from_center * 0.01f);
                    can_branch = (randf() < sub_chance);
                }

                if (can_branch && tree->branch_count < MAX_TIPS_PER_TREE) {
                    GrowthTip *new_tip = &tree->branches[tree->branch_count++];
                    new_tip->x = tip->x;
                    new_tip->y = tip->y;
                    new_tip->z = tip->z;

                    // Oak-style: branches go strongly outward
                    float branch_angle = randf() * 2.0f * PI;
                    new_tip->dx = cosf(branch_angle) * 1.0f;
                    new_tip->dy = randf_range(-0.1f, 0.1f);  // Horizontal
                    new_tip->dz = sinf(branch_angle) * 1.0f;
                    new_tip->generation = tip->generation + 1;
                    new_tip->active = true;
                    // Branch length: main branches longer, sub-branches shorter
                    new_tip->energy = (tip->generation == 0) ? randf_range(15, 25) : randf_range(8, 15);
                }
            }
        } else {
            // No attractors - add leaf cluster at tip end (only on branches)
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
    // Build initial trunk
    for (int y = 0; y < 6; y++) {
        tree_add_voxel(tree, 0, y, 0, VOXEL_TRUNK);
        if (y < 3) {
            tree_add_voxel(tree, 1, y, 0, VOXEL_TRUNK);
            tree_add_voxel(tree, -1, y, 0, VOXEL_TRUNK);
            tree_add_voxel(tree, 0, y, 1, VOXEL_TRUNK);
            tree_add_voxel(tree, 0, y, -1, VOXEL_TRUNK);
        }
    }

    // Main growth tip
    tree->tips[0].x = 0;
    tree->tips[0].y = 6;
    tree->tips[0].z = 0;
    tree->tips[0].dx = 0;
    tree->tips[0].dy = 1;
    tree->tips[0].dz = 0;
    tree->tips[0].energy = 35.0f;  // More energy for taller trees
    tree->tips[0].generation = 0;
    tree->tips[0].active = true;
    tree->tip_count = 1;

    // Add some initial branch tips
    for (int i = 1; i <= 3; i++) {
        float angle = (float)i * 2.0f * PI / 3.0f;
        tree->tips[i].x = 0;
        tree->tips[i].y = 5;
        tree->tips[i].z = 0;
        tree->tips[i].dx = cosf(angle) * 0.7f;
        tree->tips[i].dy = 0.5f;
        tree->tips[i].dz = sinf(angle) * 0.7f;
        tree->tips[i].energy = 20.0f;
        tree->tips[i].generation = 1;
        tree->tips[i].active = true;
        tree->tip_count++;
    }
}

static void grow_agent_tips(Tree *tree) {
    for (int i = 0; i < tree->tip_count; i++) {
        GrowthTip *tip = &tree->tips[i];
        if (!tip->active || tip->energy <= 0) {
            // Dead tip - add leaf cluster
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

        // Random wobble
        tip->dx += randf_range(-0.2f, 0.2f);
        tip->dz += randf_range(-0.2f, 0.2f);

        // Upward bias decreases with generation
        float upward_bias = 0.9f - tip->generation * 0.15f;
        if (upward_bias < 0.3f) upward_bias = 0.3f;
        tip->dy = upward_bias + randf_range(-0.1f, 0.1f);

        // Normalize
        float len = sqrtf(tip->dx*tip->dx + tip->dy*tip->dy + tip->dz*tip->dz);
        if (len > 0) {
            tip->dx /= len;
            tip->dy /= len;
            tip->dz /= len;
        }

        // Move
        tip->x += tip->dx;
        tip->y += tip->dy;
        tip->z += tip->dz;
        tip->energy -= 1.0f;

        // Determine voxel type
        VoxelType type = VOXEL_BRANCH;
        if (tip->generation == 0 && fabsf(tip->x) < 2 && fabsf(tip->z) < 2) {
            type = VOXEL_TRUNK;
        }

        tree_add_voxel(tree, (int)tip->x, (int)tip->y, (int)tip->z, type);

        // Branching
        float branch_chance = 0.15f + tip->generation * 0.05f;
        if (tip->energy > 8 && randf() < branch_chance && tree->tip_count < MAX_TIPS_PER_TREE) {
            GrowthTip *new_tip = &tree->tips[tree->tip_count++];
            new_tip->x = tip->x;
            new_tip->y = tip->y;
            new_tip->z = tip->z;

            // Branch at angle
            float angle = randf() * 2.0f * PI;
            new_tip->dx = cosf(angle) * 0.8f;
            new_tip->dy = 0.4f;
            new_tip->dz = sinf(angle) * 0.8f;
            new_tip->energy = tip->energy * 0.5f;
            new_tip->generation = tip->generation + 1;
            new_tip->active = true;

            tip->energy *= 0.75f;
        }

        // Add occasional leaves along branches
        if (tip->generation > 0 && tip->y > 10 && randf() < 0.25f) {
            tree_add_voxel(tree, (int)tip->x, (int)tip->y + 1, (int)tip->z, VOXEL_LEAF);
        }
    }
}

// ============ TREE INITIALIZATION ============

static void init_tree(Tree *tree, int base_x, int base_y, int base_z, TreeAlgorithm algorithm) {
    tree->base_x = base_x;
    tree->base_y = base_y;
    tree->base_z = base_z;
    tree->algorithm = algorithm;
    tree->active = true;
    tree->voxel_count = 0;
    tree->lsystem_iteration = 0;
    tree->attractor_count = 0;
    tree->branch_count = 0;
    tree->tip_count = 0;

    // Initialize spatial hash (must be before adding any voxels)
    voxel_hash_clear(tree);

    // Add base voxels
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

// ============ MAIN GAME FUNCTIONS ============

void game_init(GameState *state)
{
    float grid_center_x = (GRID_WIDTH * CELL_SIZE) / 2.0f;
    float grid_center_z = (GRID_HEIGHT * CELL_SIZE) / 2.0f;

    // Camera starts looking at grid center from an elevated position
    state->camera.position = (Vector3){ grid_center_x - 80.0f, 60.0f, grid_center_z + 80.0f };
    state->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    state->camera.fovy = 60.0f;
    state->camera.projection = CAMERA_PERSPECTIVE;

    // Calculate initial yaw/pitch from position to grid center
    float dx = grid_center_x - state->camera.position.x;
    float dy = 30.0f - state->camera.position.y;  // Look slightly below center
    float dz = grid_center_z - state->camera.position.z;
    state->camera_yaw = atan2f(dx, dz);
    state->camera_pitch = atan2f(dy, sqrtf(dx * dx + dz * dz));

    // Set initial target based on angles
    float cos_pitch = cosf(state->camera_pitch);
    state->camera.target = (Vector3){
        state->camera.position.x + sinf(state->camera_yaw) * cos_pitch,
        state->camera.position.y + sinf(state->camera_pitch),
        state->camera.position.z + cosf(state->camera_yaw) * cos_pitch
    };

    state->running = true;
    state->growth_timer = 0;
    state->burn_timer = 0;
    state->regen_timer = 0;
    state->paused = false;
    state->current_tool = TOOL_TREE;

    // Initialize terrain burn state
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            state->terrain_burn[x][z] = TERRAIN_NORMAL;
            state->terrain_burn_timer[x][z] = 0;
        }
    }

    // Allocate trees dynamically (only once, reuse on reset)
    if (state->trees == NULL) {
        state->trees = (Tree *)malloc(sizeof(Tree) * MAX_TREES);
        if (state->trees == NULL) {
            TraceLog(LOG_ERROR, "Failed to allocate trees!");
            state->running = false;
            return;
        }
        TraceLog(LOG_INFO, "Allocated %zu bytes for %d trees", sizeof(Tree) * MAX_TREES, MAX_TREES);
    }
    state->tree_count = 0;

    // Generate terrain with hills and valleys using layered noise
    for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
        for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
            float fx = (float)x / TERRAIN_RESOLUTION;
            float fz = (float)z / TERRAIN_RESOLUTION;

            // Layered sine waves for natural-looking terrain
            float height = 0;
            height += sinf(fx * 3.0f * PI) * cosf(fz * 2.5f * PI) * 4.0f;
            height += sinf(fx * 7.0f * PI + 1.0f) * sinf(fz * 6.0f * PI) * 2.0f;
            height += cosf(fx * 12.0f * PI) * cosf(fz * 11.0f * PI + 0.5f) * 1.0f;

            // Add base height and ensure minimum
            height += 5.0f;
            if (height < 0) height = 0;

            state->terrain_height[x][z] = (int)height;
        }
    }

    // Initialize trees - all using Space Colonization, spread apart
    state->tree_count = 0;
    int spacing = 10;  // More spacing for larger trees
    for (int x = 5; x < GRID_WIDTH - 5; x += spacing) {
        for (int z = 5; z < GRID_HEIGHT - 5; z += spacing) {
            if (state->tree_count >= MAX_TREES) break;

            // Get terrain height at tree position
            int terrain_x = (int)(x * CELL_SIZE / TERRAIN_SCALE);
            int terrain_z = (int)(z * CELL_SIZE / TERRAIN_SCALE);
            if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
            if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
            int ground_height = state->terrain_height[terrain_x][terrain_z];

            // Don't place trees in water
            if (ground_height < WATER_LEVEL) continue;

            init_tree(&state->trees[state->tree_count], x, ground_height, z, TREE_SPACE_COLONIZATION);
            state->tree_count++;
        }
    }

}

void game_update(GameState *state)
{
    float delta = GetFrameTime();

    if (IsKeyPressed(KEY_ESCAPE)) {
        state->running = false;
    }

    if (IsKeyPressed(KEY_SPACE)) {
        state->paused = !state->paused;
    }

    if (IsKeyPressed(KEY_R)) {
        game_init(state);
        return;
    }

    // Tool switching
    if (IsKeyPressed(KEY_ONE)) {
        state->current_tool = TOOL_BURN;
    }
    if (IsKeyPressed(KEY_TWO)) {
        state->current_tool = TOOL_TREE;
    }

    // ========== CLICK HANDLING ==========
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Cast ray from camera through mouse position
        Ray ray = GetMouseRay(GetMousePosition(), state->camera);

        // Find intersection with ground plane (approximate at average terrain height)
        float avg_height = 5.0f * TERRAIN_SCALE;
        if (ray.direction.y != 0) {
            float t = (avg_height - ray.position.y) / ray.direction.y;
            if (t > 0) {
                // Calculate hit position
                float hit_x = ray.position.x + ray.direction.x * t;
                float hit_z = ray.position.z + ray.direction.z * t;

                // Get terrain coordinates
                int terrain_x = (int)(hit_x / TERRAIN_SCALE);
                int terrain_z = (int)(hit_z / TERRAIN_SCALE);
                if (terrain_x >= TERRAIN_RESOLUTION) terrain_x = TERRAIN_RESOLUTION - 1;
                if (terrain_z >= TERRAIN_RESOLUTION) terrain_z = TERRAIN_RESOLUTION - 1;
                if (terrain_x < 0) terrain_x = 0;
                if (terrain_z < 0) terrain_z = 0;

                if (state->current_tool == TOOL_TREE) {
                    // Convert to grid coordinates
                    int grid_x = (int)(hit_x / CELL_SIZE);
                    int grid_z = (int)(hit_z / CELL_SIZE);

                    // Check bounds
                    if (grid_x >= 0 && grid_x < GRID_WIDTH &&
                        grid_z >= 0 && grid_z < GRID_HEIGHT &&
                        state->tree_count < MAX_TREES) {

                        int ground_height = state->terrain_height[terrain_x][terrain_z];

                        // Don't spawn in water
                        if (ground_height >= WATER_LEVEL) {
                            init_tree(&state->trees[state->tree_count], grid_x, ground_height, grid_z, TREE_SPACE_COLONIZATION);
                            state->tree_count++;
                        }
                    }
                } else if (state->current_tool == TOOL_BURN) {
                    // Start fire at clicked location
                    if (state->terrain_burn[terrain_x][terrain_z] == TERRAIN_NORMAL) {
                        state->terrain_burn[terrain_x][terrain_z] = TERRAIN_BURNING;
                        state->terrain_burn_timer[terrain_x][terrain_z] = BURN_DURATION;
                    }
                }
            }
        }
    }

    // ========== CAMERA CONTROLS ==========
    // Mouse look: Right-click and drag to look around
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 mouse_delta = GetMouseDelta();
        state->camera_yaw += mouse_delta.x * LOOK_SPEED * 0.003f;
        state->camera_pitch -= mouse_delta.y * LOOK_SPEED * 0.003f;

        // Clamp pitch to avoid flipping
        if (state->camera_pitch > 1.4f) state->camera_pitch = 1.4f;
        if (state->camera_pitch < -1.4f) state->camera_pitch = -1.4f;
    }

    // Calculate forward/right vectors from yaw (horizontal plane movement)
    float cos_yaw = cosf(state->camera_yaw);
    float sin_yaw = sinf(state->camera_yaw);
    Vector3 forward = { sin_yaw, 0, cos_yaw };
    Vector3 right = { cos_yaw, 0, -sin_yaw };

    // Movement speed (shift = sprint)
    float speed = MOVE_SPEED * delta;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        speed *= 2.5f;
    }

    // WASD movement
    if (IsKeyDown(KEY_W)) {
        state->camera.position.x += forward.x * speed;
        state->camera.position.z += forward.z * speed;
    }
    if (IsKeyDown(KEY_S)) {
        state->camera.position.x -= forward.x * speed;
        state->camera.position.z -= forward.z * speed;
    }
    if (IsKeyDown(KEY_A)) {
        state->camera.position.x -= right.x * speed;
        state->camera.position.z -= right.z * speed;
    }
    if (IsKeyDown(KEY_D)) {
        state->camera.position.x += right.x * speed;
        state->camera.position.z += right.z * speed;
    }

    // Vertical movement (Q = down, E = up)
    if (IsKeyDown(KEY_Q)) state->camera.position.y -= speed;
    if (IsKeyDown(KEY_E)) state->camera.position.y += speed;

    // Mouse wheel zoom (move forward/backward)
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        float cos_pitch = cosf(state->camera_pitch);
        float zoom_speed = wheel * 10.0f;
        state->camera.position.x += sin_yaw * cos_pitch * zoom_speed;
        state->camera.position.y += sinf(state->camera_pitch) * zoom_speed;
        state->camera.position.z += cos_yaw * cos_pitch * zoom_speed;
    }

    // Update camera target from yaw/pitch angles
    float cos_pitch = cosf(state->camera_pitch);
    state->camera.target = (Vector3){
        state->camera.position.x + sin_yaw * cos_pitch,
        state->camera.position.y + sinf(state->camera_pitch),
        state->camera.position.z + cos_yaw * cos_pitch
    };

    // ========== FIRE SPREAD AND BURN LOGIC ==========
    state->burn_timer += delta;
    if (state->burn_timer >= BURN_SPREAD_INTERVAL) {
        state->burn_timer = 0;

        // Process terrain burning and spread
        for (int x = 0; x < TERRAIN_RESOLUTION; x++) {
            for (int z = 0; z < TERRAIN_RESOLUTION; z++) {
                if (state->terrain_burn[x][z] == TERRAIN_BURNING) {
                    state->terrain_burn_timer[x][z] -= BURN_SPREAD_INTERVAL;

                    // Spread to neighbors
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dz = -1; dz <= 1; dz++) {
                            if (dx == 0 && dz == 0) continue;
                            int nx = x + dx;
                            int nz = z + dz;
                            if (nx >= 0 && nx < TERRAIN_RESOLUTION &&
                                nz >= 0 && nz < TERRAIN_RESOLUTION) {
                                // Only spread to normal terrain above water
                                if (state->terrain_burn[nx][nz] == TERRAIN_NORMAL &&
                                    state->terrain_height[nx][nz] >= WATER_LEVEL) {
                                    // Random spread chance
                                    if (randf() < 0.3f) {
                                        state->terrain_burn[nx][nz] = TERRAIN_BURNING;
                                        state->terrain_burn_timer[nx][nz] = BURN_DURATION;
                                    }
                                }
                            }
                        }
                    }

                    // Transition to burned after timer expires
                    if (state->terrain_burn_timer[x][z] <= 0) {
                        state->terrain_burn[x][z] = TERRAIN_BURNED;
                    }

                    // Burn trees at this terrain location
                    float world_x = x * TERRAIN_SCALE;
                    float world_z = z * TERRAIN_SCALE;

                    for (int t = 0; t < state->tree_count; t++) {
                        Tree *tree = &state->trees[t];
                        if (!tree->active) continue;

                        // Check if tree is near this burning terrain
                        float tree_world_x = tree->base_x * CELL_SIZE;
                        float tree_world_z = tree->base_z * CELL_SIZE;
                        float dist = sqrtf((tree_world_x - world_x) * (tree_world_x - world_x) +
                                          (tree_world_z - world_z) * (tree_world_z - world_z));

                        if (dist < TERRAIN_SCALE * 2) {
                            // Burn voxels from bottom up
                            for (int v = 0; v < tree->voxel_count; v++) {
                                TreeVoxel *voxel = &tree->voxels[v];
                                if (!voxel->active) continue;

                                // Fire spreads upward through tree
                                if (voxel->burn_state == VOXEL_NORMAL) {
                                    // Start burning low voxels first
                                    if (voxel->y < 15 || randf() < 0.1f) {
                                        voxel->burn_state = VOXEL_BURNING;
                                        voxel->burn_timer = BURN_DURATION;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Update tree voxel burn states
        for (int t = 0; t < state->tree_count; t++) {
            Tree *tree = &state->trees[t];
            if (!tree->active) continue;

            for (int v = 0; v < tree->voxel_count; v++) {
                TreeVoxel *voxel = &tree->voxels[v];
                if (!voxel->active) continue;

                if (voxel->burn_state == VOXEL_BURNING) {
                    voxel->burn_timer -= BURN_SPREAD_INTERVAL;

                    // Spread fire upward and sideways within tree
                    for (int v2 = 0; v2 < tree->voxel_count; v2++) {
                        TreeVoxel *other = &tree->voxels[v2];
                        if (!other->active || other->burn_state != VOXEL_NORMAL) continue;

                        int dx = abs(other->x - voxel->x);
                        int dy = other->y - voxel->y;  // Fire spreads UP
                        int dz = abs(other->z - voxel->z);

                        // Spread to adjacent voxels, preferring upward
                        if (dx <= 1 && dz <= 1 && dy >= 0 && dy <= 2) {
                            if (randf() < 0.4f) {
                                other->burn_state = VOXEL_BURNING;
                                other->burn_timer = BURN_DURATION;
                            }
                        }
                    }

                    // Transition based on voxel type
                    if (voxel->burn_timer <= 0) {
                        if (voxel->type == VOXEL_LEAF) {
                            // Leaves disappear after burning
                            voxel->active = false;
                            tree->leaf_count--;
                        } else {
                            // Trunk and branches become charred
                            voxel->burn_state = VOXEL_BURNED;
                        }
                    }
                }
            }
        }
    }

    // ========== TREE REGENERATION OF BURNED TERRAIN ==========
    state->regen_timer += delta;
    if (state->regen_timer >= REGEN_INTERVAL) {
        state->regen_timer = 0;

        // Each healthy tree regenerates nearby burned terrain
        for (int t = 0; t < state->tree_count; t++) {
            Tree *tree = &state->trees[t];
            if (!tree->active) continue;

            // Check if tree has any healthy (non-burned) leaves
            bool has_healthy_leaves = false;
            for (int v = 0; v < tree->voxel_count; v++) {
                if (tree->voxels[v].active &&
                    tree->voxels[v].type == VOXEL_LEAF &&
                    tree->voxels[v].burn_state == VOXEL_NORMAL) {
                    has_healthy_leaves = true;
                    break;
                }
            }

            if (!has_healthy_leaves) continue;

            // Get tree's terrain position
            int tree_terrain_x = (int)(tree->base_x * CELL_SIZE / TERRAIN_SCALE);
            int tree_terrain_z = (int)(tree->base_z * CELL_SIZE / TERRAIN_SCALE);

            // Regenerate burned terrain in radius around tree
            for (int dx = -TREE_REGEN_RADIUS; dx <= TREE_REGEN_RADIUS; dx++) {
                for (int dz = -TREE_REGEN_RADIUS; dz <= TREE_REGEN_RADIUS; dz++) {
                    int tx = tree_terrain_x + dx;
                    int tz = tree_terrain_z + dz;

                    // Bounds check
                    if (tx < 0 || tx >= TERRAIN_RESOLUTION ||
                        tz < 0 || tz >= TERRAIN_RESOLUTION) continue;

                    // Only regenerate burned terrain (not actively burning)
                    if (state->terrain_burn[tx][tz] != TERRAIN_BURNED) continue;

                    // Regeneration chance based on distance (closer = faster)
                    float dist = sqrtf((float)(dx*dx + dz*dz));
                    float regen_chance = 0.3f * (1.0f - dist / (TREE_REGEN_RADIUS + 1));

                    if (randf() < regen_chance) {
                        state->terrain_burn[tx][tz] = TERRAIN_NORMAL;
                    }
                }
            }
        }
    }

    // Tree growth
    if (!state->paused) {
        state->growth_timer += delta;
        if (state->growth_timer >= GROWTH_INTERVAL) {
            state->growth_timer = 0;

            for (int i = 0; i < state->tree_count; i++) {
                Tree *tree = &state->trees[i];
                if (!tree->active) continue;

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
        }
    }
}

void game_cleanup(GameState *state)
{
    if (state->trees != NULL) {
        free(state->trees);
        state->trees = NULL;
    }
}
