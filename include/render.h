#ifndef RENDER_H
#define RENDER_H

#include "game.h"

// Initialize rendering resources
void render_init(void);

// Render a frame
void render_frame(const GameState *state);

// Cleanup rendering resources
void render_cleanup(void);

#endif // RENDER_H
