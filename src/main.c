#include "raylib.h"
#include "game.h"
#include "render.h"

int main(void)
{
    // Initialize window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Ray - 3D Demo");
    SetTargetFPS(60);

    // Initialize game state
    GameState state;
    game_init(&state);

    // Initialize rendering
    render_init();

    // Main game loop
    while (state.running) {
        game_update(&state);
        render_frame(&state);
    }

    // Cleanup
    render_cleanup();
    game_cleanup(&state);
    CloseWindow();

    return 0;
}
