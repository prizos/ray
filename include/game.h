#ifndef GAME_H
#define GAME_H

#include "raylib.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Physics constants
#define GRAVITY 9.8f
#define THRUST_POWER 20.0f
#define HORIZONTAL_SPEED 8.0f
#define MAX_FALL_SPEED 30.0f
#define GROUND_LEVEL 0.5f
#define PLAYER_RADIUS 0.3f

// Fuel constants
#define MAX_FUEL 100.0f
#define STARTING_FUEL 60.0f
#define FUEL_CONSUMPTION_RATE 15.0f  // Fuel units per second when thrusting
#define FUEL_GAIN_PER_LETTER 25.0f   // Fuel gained per safe letter
#define FUEL_COST_PER_LETTER 15.0f   // Fuel lost per dangerous letter
#define DANGEROUS_LETTER_CHANCE 40   // Percent chance a letter is dangerous

// Letter entity settings
#define MAX_LETTERS 10
#define LETTER_SPEED_MIN 3.0f
#define LETTER_SPEED_MAX 8.0f
#define LETTER_SIZE 2.0f
#define LETTER_COLLISION_RADIUS 1.5f

// Spawn bounds (letters spawn on the left, move right)
#define SPAWN_X_MIN -30.0f
#define SPAWN_X_MAX -25.0f
#define SPAWN_Y_MIN 1.5f
#define SPAWN_Y_MAX 8.0f
#define SPAWN_Z_MIN -10.0f
#define SPAWN_Z_MAX 10.0f
#define DESPAWN_X 30.0f

// Available letters to spawn
#define LETTER_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

// Adversary settings
#define ADVERSARY_SPEED 3.0f
#define ADVERSARY_RADIUS 1.8f
#define ADVERSARY_SCORE_PENALTY 5

// Player state
typedef struct Player {
    Vector3 position;
    Vector3 velocity;
    float yaw;           // Horizontal look angle
    float pitch;         // Vertical look angle
    float fuel;          // Current fuel level
    bool is_thrusting;
    bool is_grounded;
} Player;

// Letter entity
typedef struct Letter {
    Vector3 position;
    Vector3 velocity;
    char character;
    bool active;
    bool was_hit;
    bool is_dangerous;  // true = costs fuel (red), false = gives fuel (green)
    Color color;
} Letter;

// Adversary (chasing red X)
typedef struct Adversary {
    Vector3 position;
    bool active;
    float hit_cooldown;  // Prevents rapid repeated hits
} Adversary;

// Game state structure
typedef struct GameState {
    Player player;
    Camera3D camera;
    bool running;
    int score;
    Letter letters[MAX_LETTERS];
    float spawn_timer;
    float spawn_interval;
    Adversary adversary;
} GameState;

// Initialize game state
void game_init(GameState *state);

// Update game state (called each frame)
void game_update(GameState *state);

// Cleanup game resources
void game_cleanup(GameState *state);

// Spawn a new letter
void game_spawn_letter(GameState *state);

// Check collision between player and a letter
bool game_check_letter_collision(Vector3 player_pos, Letter *letter);

#endif // GAME_H
