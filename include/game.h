#ifndef GAME_H
#define GAME_H

#include "raylib.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Grid settings
#define GRID_WIDTH 40
#define GRID_HEIGHT 40
#define CELL_SIZE 5.0f
#define BOX_SIZE 0.4f

// Terrain settings
#define TERRAIN_RESOLUTION 80   // Terrain grid points
#define TERRAIN_SCALE 2.5f      // Size of each terrain voxel
#define WATER_LEVEL 3           // Height below which water appears

// Tree settings
#define MAX_TREES 12
#define MAX_TREE_HEIGHT 120
#define MAX_VOXELS_PER_TREE 12000
#define MAX_TIPS_PER_TREE 200
#define MAX_ATTRACTORS 800

// Spatial hash settings (prime number > MAX_VOXELS_PER_TREE * 1.3)
#define VOXEL_HASH_SIZE 16007

#define GROWTH_INTERVAL 0.05f  // Very fast growth

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

// A single voxel in a tree
typedef struct {
    int x, y, z;       // Position relative to tree base
    VoxelType type;    // Trunk, branch, or leaf
    bool active;
} TreeVoxel;

// Growth tip for agent-based trees
typedef struct {
    float x, y, z;      // Current position
    float dx, dy, dz;   // Direction
    float energy;       // Remaining growth energy
    int generation;     // How many times this tip has branched
    bool active;
} GrowthTip;

// Attraction point for space colonization
typedef struct {
    float x, y, z;
    bool active;
} Attractor;

// Spatial hash entry (packed position -> voxel index)
typedef struct {
    int key;        // Packed (x,y,z) or -1 if empty
    int voxel_idx;  // Index into voxels array
} VoxelHashEntry;

// Tree structure
typedef struct {
    int base_x, base_y, base_z;  // Grid position (base_y = terrain height)
    TreeAlgorithm algorithm;
    bool active;

    // Voxel storage
    TreeVoxel voxels[MAX_VOXELS_PER_TREE];
    int voxel_count;

    // Spatial hash for O(1) duplicate checking
    VoxelHashEntry voxel_hash[VOXEL_HASH_SIZE];

    // Cached voxel counts (updated incrementally)
    int trunk_count;
    int branch_count_cached;
    int leaf_count;

    // L-System state
    int lsystem_iteration;

    // Space Colonization state
    Attractor attractors[MAX_ATTRACTORS];
    int attractor_count;
    GrowthTip branches[MAX_TIPS_PER_TREE];
    int branch_count;

    // Agent-based state
    GrowthTip tips[MAX_TIPS_PER_TREE];
    int tip_count;
} Tree;

// Game state structure
typedef struct GameState {
    Camera3D camera;
    float camera_yaw;      // Horizontal angle (radians)
    float camera_pitch;    // Vertical angle (radians)
    bool running;

    // Terrain height map
    int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // Trees
    Tree trees[MAX_TREES];
    int tree_count;

    // Growth timing
    float growth_timer;
    bool paused;
} GameState;

// Initialize game state
void game_init(GameState *state);

// Update game state
void game_update(GameState *state);

// Cleanup
void game_cleanup(GameState *state);

#endif // GAME_H
