#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "tree.h"
#include "terrain.h"

// ============ DISPLAY CONSTANTS ============

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// ============ GRID CONSTANTS ============

#define GRID_WIDTH 40
#define GRID_HEIGHT 40
#define CELL_SIZE 5.0f
#define BOX_SIZE 0.4f

// ============ CAMERA CONSTANTS ============

#define MOVE_SPEED 50.0f
#define LOOK_SPEED 2.0f

// ============ TOOL TYPES ============

typedef enum {
    TOOL_TREE,
    TOOL_BURN
} ToolType;

// ============ GAME STATE ============

typedef struct GameState {
    Camera3D camera;
    float camera_yaw;
    float camera_pitch;
    bool running;

    // Current tool
    ToolType current_tool;

    // Terrain
    int terrain_height[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    TerrainBurnState terrain_burn[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];
    float terrain_burn_timer[TERRAIN_RESOLUTION][TERRAIN_RESOLUTION];

    // Trees (dynamically allocated)
    Tree *trees;
    int tree_count;

    // Timers
    float growth_timer;
    float burn_timer;
    float regen_timer;

    bool paused;
} GameState;

// ============ GAME FUNCTIONS ============

void game_init(GameState *state);
void game_init_with_trees(GameState *state, int num_trees);
void game_update(GameState *state);
void game_cleanup(GameState *state);

#endif // GAME_H
