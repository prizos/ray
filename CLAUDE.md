# Ray - Distributed Game Client

A raylib-based game client written in pure C. The client handles rendering and input, connecting to a distributed Go backend (separate repository) for physics and game state.

## Architecture

- **Client (this repo)**: Pure C with raylib - rendering, input, network receive
- **Backend (separate)**: Go - distributed physics, game state, coordination

## Build

```bash
make          # Build the game
make run      # Build and run
make clean    # Clean build artifacts
make debug    # Build with debug symbols
```

Requires raylib installed. On Ubuntu/Debian:
```bash
sudo apt install libraylib-dev
```

On macOS:
```bash
brew install raylib
```

## Project Structure

```
src/           # C source files
include/       # Header files
assets/        # Game assets (textures, sounds, etc.)
build/         # Build output (gitignored)
docs/          # Documentation and raylib reference
```

## Code Conventions

- **Naming**: snake_case for functions and variables, PascalCase for types/structs
- **Files**: One module per file pair (module.c + module.h)
- **Memory**: Explicit allocation/deallocation, no hidden mallocs
- **Headers**: Include guards using `#ifndef MODULE_H` / `#define MODULE_H`
- **Raylib**: Use raylib types directly (Vector2, Color, etc.)

## Key Files

- `src/main.c` - Entry point and game loop
- `src/game.c` - Core game state and logic
- `src/render.c` - All rendering code
- `src/input.c` - Input handling
- `src/network.c` - Network protocol (future)

## Network Protocol (Future)

Client will communicate with Go backend via:
- UDP for game state updates (low latency)
- TCP for reliable messages (chat, inventory, etc.)
- Protocol format: TBD (likely protobuf or flat binary)

## Raylib Documentation

Local raylib documentation is available in the `docs/` folder. **Always consult these before implementing raylib features.**

### Quick Reference Files

| File | Content | Use For |
|------|---------|---------|
| `docs/raylib_cheatsheet_v5.md` | Complete API (985 lines) | Function signatures, parameters, descriptions |
| `docs/raylib.h` | Full header (1727 lines) | Struct definitions, enums, flags, constants |
| `docs/FAQ.md` | General FAQ | Understanding raylib concepts |

### Slash Commands

- `/raylib <query>` - Search for functions, types, or concepts
- `/raylib-module <module>` - Show all functions in a module

### API Modules

When looking up raylib APIs, functions are organized by module:

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **rcore** | Window, input, timing | `InitWindow`, `IsKeyPressed`, `GetFrameTime` |
| **rshapes** | 2D drawing, collision | `DrawRectangle`, `DrawCircle`, `CheckCollisionRecs` |
| **rtextures** | Images, textures | `LoadTexture`, `DrawTexture`, `UnloadTexture` |
| **rtext** | Fonts, text | `DrawText`, `LoadFont`, `MeasureText` |
| **rmodels** | 3D models, meshes | `LoadModel`, `DrawModel`, `DrawCube` |
| **raudio** | Sound, music | `LoadSound`, `PlaySound`, `LoadMusicStream` |
| **rcamera** | Camera control | `UpdateCamera`, `UpdateCameraPro` |
| **rgestures** | Touch gestures | `IsGestureDetected`, `GetGestureDetected` |

### How to Look Up APIs

1. **For function signatures**: Search `docs/raylib_cheatsheet_v5.md`
   ```bash
   # Example: find all Draw functions
   grep -n "Draw" docs/raylib_cheatsheet_v5.md
   ```

2. **For struct definitions**: Search `docs/raylib.h`
   ```bash
   # Example: find Camera2D definition
   grep -A 10 "typedef struct Camera2D" docs/raylib.h
   ```

3. **For enums/flags**: Search `docs/raylib.h`
   ```bash
   # Example: find keyboard key codes
   grep -A 50 "Keyboard keys" docs/raylib.h
   ```

### Common Patterns

**Game Loop:**
```c
InitWindow(width, height, title);
SetTargetFPS(60);
while (!WindowShouldClose()) {
    // Update
    float dt = GetFrameTime();

    // Draw
    BeginDrawing();
    ClearBackground(RAYWHITE);
    // ... drawing code
    EndDrawing();
}
CloseWindow();
```

**Loading Resources:**
```c
Texture2D tex = LoadTexture("assets/sprite.png");
// ... use texture
UnloadTexture(tex);  // Always unload when done
```

**Input Handling:**
```c
if (IsKeyPressed(KEY_SPACE)) { }  // Once per press
if (IsKeyDown(KEY_RIGHT)) { }     // Continuous
Vector2 mouse = GetMousePosition();
```
