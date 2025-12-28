#include "beaver.h"
#include "terrain.h"
#include <math.h>
#include <stdlib.h>

// Grid settings (from game.h)
#define CELL_SIZE 5.0f

static float randf(void) {
    return (float)GetRandomValue(0, 10000) / 10000.0f;
}

// Find a tree with burned voxels, returns -1 if none
static int find_burned_tree(Tree *trees, int tree_count) {
    // Collect indices of trees with burned voxels
    int candidates[100];
    int candidate_count = 0;

    for (int t = 0; t < tree_count && candidate_count < 100; t++) {
        Tree *tree = &trees[t];
        if (!tree->active) continue;

        for (int v = 0; v < tree->voxel_count; v++) {
            if (tree->voxels[v].active && tree->voxels[v].burn_state == VOXEL_BURNED) {
                candidates[candidate_count++] = t;
                break;
            }
        }
    }

    if (candidate_count == 0) return -1;
    return candidates[GetRandomValue(0, candidate_count - 1)];
}

// Count burned voxels in a tree
static int count_burned_voxels(Tree *tree) {
    int count = 0;
    for (int v = 0; v < tree->voxel_count; v++) {
        if (tree->voxels[v].active && tree->voxels[v].burn_state == VOXEL_BURNED) {
            count++;
        }
    }
    return count;
}

// Eat some burned voxels from tree
static int eat_burned_voxels(Tree *tree, int max_eat) {
    int eaten = 0;
    for (int v = 0; v < tree->voxel_count && eaten < max_eat; v++) {
        TreeVoxel *voxel = &tree->voxels[v];
        if (voxel->active && voxel->burn_state == VOXEL_BURNED) {
            voxel->active = false;
            if (voxel->type == VOXEL_TRUNK) tree->trunk_count--;
            else if (voxel->type == VOXEL_BRANCH) tree->branch_count--;
            else if (voxel->type == VOXEL_LEAF) tree->leaf_count--;
            eaten++;
        }
    }
    return eaten;
}

void beaver_init_all(Beaver *beavers, int *beaver_count) {
    *beaver_count = 0;
    for (int i = 0; i < MAX_BEAVERS; i++) {
        beavers[i].active = false;
    }
}

void beaver_spawn(Beaver *beavers, int *beaver_count,
                  Tree *trees, int tree_count,
                  const int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION]) {

    if (*beaver_count >= MAX_BEAVERS) return;

    // Find a tree with burned voxels
    int target = find_burned_tree(trees, tree_count);
    if (target < 0) return;

    Tree *tree = &trees[target];

    // Find an empty slot
    int slot = -1;
    for (int i = 0; i < MAX_BEAVERS; i++) {
        if (!beavers[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;

    // Spawn at edge of map, moving toward target tree
    float tree_world_x = tree->base_x * CELL_SIZE;
    float tree_world_z = tree->base_z * CELL_SIZE;

    // Pick a random edge to spawn from
    float spawn_x, spawn_z;
    float map_size = TERRAIN_RESOLUTION * TERRAIN_SCALE;
    int edge = GetRandomValue(0, 3);

    switch (edge) {
        case 0: // North edge
            spawn_x = randf() * map_size;
            spawn_z = 0;
            break;
        case 1: // South edge
            spawn_x = randf() * map_size;
            spawn_z = map_size;
            break;
        case 2: // East edge
            spawn_x = map_size;
            spawn_z = randf() * map_size;
            break;
        default: // West edge
            spawn_x = 0;
            spawn_z = randf() * map_size;
            break;
    }

    // Get terrain height at spawn position
    int tx = (int)(spawn_x / TERRAIN_SCALE);
    int tz = (int)(spawn_z / TERRAIN_SCALE);
    if (tx < 0) tx = 0;
    if (tx >= TERRAIN_RESOLUTION) tx = TERRAIN_RESOLUTION - 1;
    if (tz < 0) tz = 0;
    if (tz >= TERRAIN_RESOLUTION) tz = TERRAIN_RESOLUTION - 1;
    float spawn_y = terrain_height[tx][tz] * TERRAIN_SCALE;

    Beaver *beaver = &beavers[slot];
    beaver->x = spawn_x;
    beaver->y = spawn_y;
    beaver->z = spawn_z;
    beaver->target_x = tree_world_x;
    beaver->target_z = tree_world_z;
    beaver->state = BEAVER_MOVING;
    beaver->target_tree = target;
    beaver->eat_timer = 0;
    beaver->meals_eaten = 0;
    beaver->active = true;

    (*beaver_count)++;
}

void beaver_update(Beaver *beavers, int *beaver_count,
                   Tree *trees, int tree_count,
                   const int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION],
                   float delta) {

    // Try to spawn new beavers if there are burned trees
    if (randf() < BEAVER_SPAWN_CHANCE) {
        beaver_spawn(beavers, beaver_count, trees, tree_count, terrain_height);
    }

    // Update each active beaver
    for (int i = 0; i < MAX_BEAVERS; i++) {
        Beaver *beaver = &beavers[i];
        if (!beaver->active) continue;

        // Get terrain height at current position
        int tx = (int)(beaver->x / TERRAIN_SCALE);
        int tz = (int)(beaver->z / TERRAIN_SCALE);
        if (tx < 0) tx = 0;
        if (tx >= TERRAIN_RESOLUTION) tx = TERRAIN_RESOLUTION - 1;
        if (tz < 0) tz = 0;
        if (tz >= TERRAIN_RESOLUTION) tz = TERRAIN_RESOLUTION - 1;
        beaver->y = terrain_height[tx][tz] * TERRAIN_SCALE + BEAVER_SIZE * 0.5f;

        switch (beaver->state) {
            case BEAVER_SPAWNING:
                // Transition to moving
                beaver->state = BEAVER_MOVING;
                break;

            case BEAVER_MOVING: {
                // Move toward target
                float dx = beaver->target_x - beaver->x;
                float dz = beaver->target_z - beaver->z;
                float dist = sqrtf(dx * dx + dz * dz);

                if (dist < 3.0f) {
                    // Arrived at target
                    beaver->state = BEAVER_EATING;
                    beaver->eat_timer = 0;
                } else {
                    // Move toward target
                    float speed = BEAVER_MOVE_SPEED * delta;
                    beaver->x += (dx / dist) * speed;
                    beaver->z += (dz / dist) * speed;
                }
                break;
            }

            case BEAVER_EATING: {
                beaver->eat_timer += delta;

                if (beaver->eat_timer >= BEAVER_EAT_INTERVAL) {
                    beaver->eat_timer = 0;

                    // Check if target tree still has burned voxels
                    if (beaver->target_tree >= 0 && beaver->target_tree < tree_count) {
                        Tree *tree = &trees[beaver->target_tree];
                        if (tree->active) {
                            int eaten = eat_burned_voxels(tree, BEAVER_VOXELS_PER_MEAL);
                            if (eaten > 0) {
                                beaver->meals_eaten++;
                            }

                            // Check if done eating
                            if (beaver->meals_eaten >= BEAVER_MAX_MEALS ||
                                count_burned_voxels(tree) == 0) {
                                // Look for another burned tree or leave
                                int new_target = find_burned_tree(trees, tree_count);
                                if (new_target >= 0 && beaver->meals_eaten < BEAVER_MAX_MEALS) {
                                    beaver->target_tree = new_target;
                                    Tree *new_tree = &trees[new_target];
                                    beaver->target_x = new_tree->base_x * CELL_SIZE;
                                    beaver->target_z = new_tree->base_z * CELL_SIZE;
                                    beaver->state = BEAVER_MOVING;
                                } else {
                                    beaver->state = BEAVER_LEAVING;
                                    // Set target to nearest edge
                                    float map_size = TERRAIN_RESOLUTION * TERRAIN_SCALE;
                                    float dist_north = beaver->z;
                                    float dist_south = map_size - beaver->z;
                                    float dist_west = beaver->x;
                                    float dist_east = map_size - beaver->x;

                                    float min_dist = dist_north;
                                    beaver->target_x = beaver->x;
                                    beaver->target_z = -20.0f;

                                    if (dist_south < min_dist) {
                                        min_dist = dist_south;
                                        beaver->target_x = beaver->x;
                                        beaver->target_z = map_size + 20.0f;
                                    }
                                    if (dist_west < min_dist) {
                                        min_dist = dist_west;
                                        beaver->target_x = -20.0f;
                                        beaver->target_z = beaver->z;
                                    }
                                    if (dist_east < min_dist) {
                                        beaver->target_x = map_size + 20.0f;
                                        beaver->target_z = beaver->z;
                                    }
                                }
                            }
                        } else {
                            beaver->state = BEAVER_LEAVING;
                        }
                    } else {
                        beaver->state = BEAVER_LEAVING;
                    }
                }
                break;
            }

            case BEAVER_LEAVING: {
                // Move toward edge
                float dx = beaver->target_x - beaver->x;
                float dz = beaver->target_z - beaver->z;
                float dist = sqrtf(dx * dx + dz * dz);

                if (dist < 5.0f) {
                    // Despawn
                    beaver->active = false;
                    (*beaver_count)--;
                } else {
                    float speed = BEAVER_MOVE_SPEED * delta;
                    beaver->x += (dx / dist) * speed;
                    beaver->z += (dz / dist) * speed;
                }
                break;
            }
        }
    }
}
