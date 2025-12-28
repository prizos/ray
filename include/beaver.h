#ifndef BEAVER_H
#define BEAVER_H

#include "raylib.h"
#include "tree.h"
#include <stdbool.h>

// ============ BEAVER CONSTANTS ============

#define MAX_BEAVERS 50
#define BEAVER_SPAWN_CHANCE 0.05f      // Chance per frame to spawn when burned trees exist
#define BEAVER_MOVE_SPEED 12.0f        // Units per second
#define BEAVER_EAT_INTERVAL 0.08f      // Time between eating voxels
#define BEAVER_VOXELS_PER_MEAL 20      // Voxels eaten per eating action
#define BEAVER_MAX_MEALS 40            // Max eating actions before leaving
#define BEAVER_SIZE 1.5f               // Render size

// ============ BEAVER TYPES ============

typedef enum {
    BEAVER_SPAWNING,    // Just appeared, picking target
    BEAVER_MOVING,      // Moving toward target tree
    BEAVER_EATING,      // Eating burned voxels
    BEAVER_LEAVING      // Moving away to despawn
} BeaverState;

typedef struct {
    float x, y, z;          // World position
    float target_x, target_z; // Target position
    BeaverState state;
    int target_tree;        // Index of target tree (-1 if none)
    float eat_timer;        // Timer for eating action
    int meals_eaten;        // How many times it has eaten
    bool active;
} Beaver;

// ============ BEAVER FUNCTIONS ============

// Initialize beaver array
void beaver_init_all(Beaver *beavers, int *beaver_count);

// Update all beavers (spawning, moving, eating, leaving)
void beaver_update(Beaver *beavers, int *beaver_count,
                   Tree *trees, int tree_count,
                   const int terrain_height[160][160],
                   float delta);

// Spawn a beaver near burned trees
void beaver_spawn(Beaver *beavers, int *beaver_count,
                  Tree *trees, int tree_count,
                  const int terrain_height[160][160]);

#endif // BEAVER_H
