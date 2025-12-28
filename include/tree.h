#ifndef TREE_H
#define TREE_H

#include "raylib.h"
#include <stdbool.h>

// ============ TREE CONSTANTS ============

// Tree structure limits
#define MAX_TREES 100
#define MAX_TREE_HEIGHT 120
#define MAX_VOXELS_PER_TREE 12000
#define MAX_TIPS_PER_TREE 200
#define MAX_ATTRACTORS 800

// Spatial hash size (prime number > MAX_VOXELS_PER_TREE * 1.3 for good distribution)
#define VOXEL_HASH_SIZE 16007

// Growth timing
#define GROWTH_INTERVAL 0.05f

// Space colonization algorithm parameters
#define SC_INFLUENCE_RADIUS 15.0f    // How far attractors can influence tips
#define SC_KILL_RADIUS 3.0f          // Distance at which attractors are consumed
#define SC_TRUNK_STEP 0.6f           // Growth step size for trunk
#define SC_BRANCH_STEP 0.8f          // Growth step size for branches
#define SC_MOMENTUM 0.8f             // How much branches follow their own direction
#define SC_BRANCH_CHANCE_TRUNK 0.25f // Chance of spawning branch from trunk
#define SC_BRANCH_CHANCE_BASE 0.12f  // Base chance of sub-branching
#define SC_LEAF_DISTANCE 10.0f       // Min distance from trunk for leaves
#define SC_MAIN_BRANCHES_MIN 4       // Minimum initial main branches
#define SC_MAIN_BRANCHES_MAX 6       // Maximum initial main branches
#define SC_TRUNK_HEIGHT 40           // Height of pre-built trunk
#define SC_BRANCH_HEIGHT_MIN 15.0f   // Min height for branch spawning
#define SC_CROWN_HEIGHT_MIN 15.0f    // Min height for crown attractors
#define SC_CROWN_HEIGHT_MAX 45.0f    // Max height for crown attractors
#define SC_CROWN_SPREAD_MAX 30.0f    // Max horizontal spread of crown

// Agent-based algorithm parameters
#define AGENT_INITIAL_ENERGY 35.0f
#define AGENT_BRANCH_ENERGY 20.0f
#define AGENT_UPWARD_BIAS_BASE 0.9f
#define AGENT_UPWARD_BIAS_DECAY 0.15f
#define AGENT_BRANCH_CHANCE_BASE 0.15f
#define AGENT_BRANCH_CHANCE_PER_GEN 0.05f

// L-System parameters
#define LSYSTEM_MAX_ITERATIONS 25
#define LSYSTEM_BRANCH_CHANCE_BASE 0.15f
#define LSYSTEM_BRANCH_CHANCE_PER_HEIGHT 0.02f

// ============ TREE TYPES ============

// Tree algorithm types
typedef enum {
    TREE_LSYSTEM,
    TREE_SPACE_COLONIZATION,
    TREE_AGENT_TIPS
} TreeAlgorithm;

// Voxel types for coloring
typedef enum {
    VOXEL_TRUNK,
    VOXEL_BRANCH,
    VOXEL_LEAF
} VoxelType;

// Voxel burn state
typedef enum {
    VOXEL_NORMAL,
    VOXEL_BURNING,
    VOXEL_BURNED
} VoxelBurnState;

// A single voxel in a tree
typedef struct {
    int x, y, z;
    VoxelType type;
    VoxelBurnState burn_state;
    float burn_timer;
    bool active;
} TreeVoxel;

// Growth tip for agent-based trees and space colonization
typedef struct {
    float x, y, z;
    float dx, dy, dz;
    float energy;
    int generation;
    bool active;
} GrowthTip;

// Attraction point for space colonization
typedef struct {
    float x, y, z;
    bool active;
} Attractor;

// Spatial hash entry
typedef struct {
    int key;
    int voxel_idx;
} VoxelHashEntry;

// Tree structure
typedef struct {
    int base_x, base_y, base_z;
    TreeAlgorithm algorithm;
    bool active;

    // Voxel storage
    TreeVoxel voxels[MAX_VOXELS_PER_TREE];
    int voxel_count;

    // Spatial hash for O(1) duplicate checking
    VoxelHashEntry voxel_hash[VOXEL_HASH_SIZE];

    // Cached voxel counts
    int trunk_count;
    int branch_count;
    int leaf_count;

    // L-System state
    int lsystem_iteration;

    // Space Colonization state
    Attractor attractors[MAX_ATTRACTORS];
    int attractor_count;
    GrowthTip sc_branches[MAX_TIPS_PER_TREE];
    int sc_branch_count;

    // Agent-based state
    GrowthTip tips[MAX_TIPS_PER_TREE];
    int tip_count;
} Tree;

// ============ TREE FUNCTIONS ============

// Initialize a tree at the given position
void tree_init(Tree *tree, int base_x, int base_y, int base_z, TreeAlgorithm algorithm);

// Grow a tree by one step
void tree_grow(Tree *tree);

// Add a voxel to a tree (returns false if full or duplicate)
bool tree_add_voxel(Tree *tree, int x, int y, int z, VoxelType type);

// Check if a voxel exists at position
bool tree_voxel_exists(Tree *tree, int x, int y, int z);

// Get voxel at position (returns NULL if not found)
TreeVoxel *tree_get_voxel_at(Tree *tree, int x, int y, int z);

// Clear the spatial hash table
void tree_hash_clear(Tree *tree);

// Pack position into hash key
int tree_pack_key(int x, int y, int z);

// Compute hash index from key
int tree_hash_index(int key);

#endif // TREE_H
