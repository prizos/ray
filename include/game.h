#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "tree.h"
#include "terrain.h"
#include "beaver.h"
#include "water.h"
#include <stdint.h>

// ============ DISPLAY CONSTANTS ============

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// ============ GRID CONSTANTS ============

#define GRID_WIDTH 80
#define GRID_HEIGHT 80
#define CELL_SIZE 5.0f
#define BOX_SIZE 0.4f

// ============ CAMERA CONSTANTS ============

#define MOVE_SPEED 50.0f
#define LOOK_SPEED 2.0f

// ============ TOOL TYPES ============

typedef enum {
    TOOL_TREE,
    TOOL_BURN,
    TOOL_WATER
} ToolType;

// ============ GAME STATE ============

typedef struct GameState {
    Camera3D camera;
    float camera_yaw;
    float camera_pitch;
    bool running;

    // Current tool
    ToolType current_tool;

    // Target indicator (for debugging placement)
    bool target_valid;
    int target_grid_x;
    int target_grid_z;
    float target_world_x;
    float target_world_y;
    float target_world_z;

    // Terrain
    int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState terrain_burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float terrain_burn_timer[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // Water simulation
    WaterState water;

    // Trees (dynamically allocated, grows as needed)
    Tree *trees;
    int tree_count;
    int tree_capacity;

    // Beavers
    Beaver beavers[MAX_BEAVERS];
    int beaver_count;

    // Timers
    float growth_timer;
    float burn_timer;
    float regen_timer;

    bool paused;
} GameState;

// ============ GAME FUNCTIONS ============

void game_init(GameState *state);
void game_init_with_seed(GameState *state, uint32_t seed);
void game_init_with_trees(GameState *state, int num_trees);
void game_init_full(GameState *state, int num_trees, uint32_t seed);
void game_update(GameState *state);
void game_cleanup(GameState *state);

#endif // GAME_H
