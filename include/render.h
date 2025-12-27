#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"
#include "game.h"

// Initialize rendering resources
void render_init(void);

// Draw a single frame
void render_frame(const GameState *state);

// Draw 3D text at a position
void render_text_3d(Font font, const char *text, Vector3 position,
                    float font_size, float spacing, bool backface, Color tint);

// Cleanup rendering resources
void render_cleanup(void);

#endif // RENDER_H
