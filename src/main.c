#include "raylib.h"
#include "game.h"
#include "render.h"

int main(void)
{
    // Initialize window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Grid Viewer");
    SetTargetFPS(60);

    // Initialize game state (static for zero-init, trees allocated dynamically)
    static GameState state = {0};
    game_init(&state);

    // Initialize rendering
    render_init();

    // Main game loop
    while (!WindowShouldClose() && state.running) {
        game_update(&state);
        render_frame(&state);
    }

    // Cleanup
    render_cleanup();
    game_cleanup(&state);
    CloseWindow();

    return 0;
}
