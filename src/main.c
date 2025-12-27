#include "raylib.h"
#include "game.h"
#include "render.h"
#include "audio.h"

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

    // Initialize audio
    audio_init();

    // Main game loop
    while (!WindowShouldClose() && state.running) {
        game_update(&state);
        audio_update();
        render_frame(&state);
    }

    // Cleanup
    audio_cleanup();
    render_cleanup();
    game_cleanup(&state);
    CloseWindow();

    return 0;
}
