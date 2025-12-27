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
