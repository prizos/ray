#include "raylib.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Ray");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // Update

        // Draw
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Ray Client", 10, 10, 20, DARKGRAY);
            DrawFPS(SCREEN_WIDTH - 100, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
