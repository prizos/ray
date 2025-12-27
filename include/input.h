#ifndef INPUT_H
#define INPUT_H

#include "raylib.h"
#include "game.h"

// Look sensitivity
#define LOOK_SPEED 2.0f

// Process player input (thrust, movement, looking)
void input_update_player(Player *player, float delta_time);

// Update camera to follow player
void input_update_camera(Camera3D *camera, const Player *player);

// Check if quit was requested
bool input_quit_requested(void);

// Check if reset was requested
bool input_reset_requested(void);

#endif // INPUT_H
