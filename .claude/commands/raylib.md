Look up raylib API documentation. Search for the function, type, or concept specified by the user.

## Instructions

1. First, search `docs/raylib_cheatsheet_v5.md` for the query - this contains all raylib v5.5 functions organized by module:
   - **rcore**: Window, cursor, drawing, timing, input (keyboard, mouse, gamepad, touch)
   - **rgestures**: Touch/gesture handling
   - **rcamera**: Camera system (UpdateCamera, UpdateCameraPro)
   - **rshapes**: 2D shapes, splines, collision detection
   - **rtextures**: Images, textures, colors
   - **rtext**: Fonts, text drawing, string utilities
   - **rmodels**: 3D shapes, models, meshes, materials, animations
   - **raudio**: Audio device, sounds, music streams

2. If looking for struct definitions, enums, or flags, search `docs/raylib.h` - the full header with:
   - Complete struct definitions (Vector2, Vector3, Color, Rectangle, Image, Texture, etc.)
   - All enums (KeyboardKey, MouseButton, GamepadButton, ConfigFlags, etc.)
   - Macro definitions and constants

3. Return the relevant function signatures with their descriptions.

## Example queries
- "DrawTexture" → Show DrawTexture variants and parameters
- "keyboard input" → Show IsKeyPressed, IsKeyDown, GetKeyPressed, etc.
- "collision" → Show CheckCollision* functions
- "Camera2D struct" → Show Camera2D definition from raylib.h

Search the docs now for: $ARGUMENTS
