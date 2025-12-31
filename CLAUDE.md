# Ray - Physics-Based Game

A raylib-based game written in pure C with accurate physics simulation of matter and energy.

## Table of Contents

1. [Build System](#build-system)
2. [Testing](#testing)
3. [Architecture](#architecture)
4. [Code Conventions](#code-conventions)
5. [Physics Documentation](#physics-documentation)

---

## Build System

Uses [Meson](https://mesonbuild.com/) build system. **All builds must use these commands.**

### Build Commands

```bash
# Setup (one time)
meson setup build

# Build
meson compile -C build

# Run the game
./build/game

# Clean and rebuild
rm -rf build && meson setup build && meson compile -C build
```

### Debug Build

```bash
# Debug build with symbols
meson setup build-debug -Dbuildtype=debug
meson compile -C build-debug

# Debug build with metrics overlay
meson setup build-debug -Ddebug_metrics=true -Dbuildtype=debug
meson compile -C build-debug
./build-debug/game -D
```

---

## Testing

### Running Tests

```bash
# Run all tests
meson test -C build

# Run specific test
meson test -C build test_svo_unit

# Run with verbose output
meson test -C build -v

# List all tests
meson test -C build --list
```

### Test Index

All tests are defined in `meson.build`. The canonical test list:

| Test | File | Purpose |
|------|------|---------|
| `test_svo_unit` | `tests/test_svo_unit.c` | Chunk/cell unit tests |
| `test_svo_physics` | `tests/test_svo_physics.c` | Physics simulation tests |
| `test_3d_physics` | `tests/test_3d_physics.c` | 3D physics integration |
| `test_energy_conservation` | `tests/test_energy_conservation.c` | Energy conservation verification |
| `test_tool_integration` | `tests/test_tool_integration.c` | Tool API tests |
| `test_e2e_simulation` | `tests/test_e2e_simulation.c` | End-to-end simulation |
| `test_phase_specific_cp` | `tests/test_phase_specific_cp.c` | Phase-specific heat capacity |
| `test_noise` | `tests/test_noise.c` | Noise generation |
| `test_octree` | `tests/test_octree.c` | Octree data structure |

### Testing Rules

**CRITICAL: Follow these rules strictly.**

1. **Use existing tests only**
   - Run tests using `meson test -C build`
   - Never run test binaries directly or use ad-hoc shell scripts
   - Never create one-off test programs outside `tests/`

2. **Extend existing test files**
   - Add new test cases to the appropriate existing test file
   - If a new test category is needed, add it to `meson.build` properly
   - Follow the existing test file structure and macros

3. **Fix failures properly**
   - If a test fails, fix the code or the test - do not work around it
   - Do not disable, skip, or comment out failing tests
   - Do not add special cases or hacks to make tests pass artificially
   - Understand WHY a test fails before changing anything

4. **No hacks or workarounds**
   - If something doesn't work, diagnose and fix the root cause
   - Do not add "temporary" fixes that mask problems
   - Do not hardcode values to make tests pass
   - If a test expectation is wrong, fix it with proper justification

5. **Build verification**
   - After any change, run `meson compile -C build` to verify compilation
   - Run `meson test -C build` to verify all tests pass
   - Do not commit code that breaks the build or tests

---

## Architecture

### Project Structure

```
src/           # C source files
include/       # Header files
tests/         # Test files (all tests here)
build/         # Build output (gitignored)
docs/          # Documentation (physics, materials, raylib)
```

### Matter System (Chunks)

The 3D matter simulation uses a chunk-based system:

| File | Purpose |
|------|---------|
| `chunk.h` | Core structures, cell operations, material properties |
| `chunk.c` | Cell operations, material state management |
| `chunk_world.c` | World management, chunk hash table, tool APIs |
| `chunk_physics.c` | Heat conduction, phase transitions, fluid flow |

Key types:
- `ChunkWorld` - Hash table of chunks containing all matter
- `Chunk` - 32x32x32 cell block with O(1) neighbor access
- `Cell3D` - A cell containing multiple materials (bitmask-based)
- `MaterialType` - Enum: `MAT_WATER`, `MAT_ROCK`, `MAT_DIRT`, etc.
- `MaterialProperties` - Physical constants (in `MATERIAL_PROPS` table)
- `MaterialState` - Per-material: `moles` + `thermal_energy`

---

## Code Conventions

- **Naming**: snake_case for functions/variables, PascalCase for types/structs
- **Files**: One module per file pair (module.c + module.h)
- **Memory**: Explicit allocation/deallocation, no hidden mallocs
- **Headers**: Include guards using `#ifndef MODULE_H` / `#define MODULE_H`
- **Raylib**: Use raylib types directly (Vector2, Color, etc.)

---

## Physics Documentation

Detailed physics and material references are in `docs/`:

| Document | Contents |
|----------|----------|
| [docs/physics.md](docs/physics.md) | Thermodynamic model, heat transfer, phase transitions, Fourier's Law |
| [docs/materials.md](docs/materials.md) | Material properties index (H2O, SiO2, N2, O2, CO2) with verified values |

### Quick Reference

**Core principle**: Temperature is derived from thermal energy, not stored directly.

```
T = f(E, n, material_properties)
```

Where energy accounts for latent heat at phase transitions.

**Test philosophy**: Every test must trace to a physical law (conservation of energy, Fourier's Law, etc.). Properties are derived from fundamentals, never hardcoded.

**Constants**:
- `INITIAL_TEMP_K = 293.0` (20C) - Only for initializing new matter
- Empty cells return 0.0 for temperature (sentinel value)

**Energy calculation**: Use `calculate_material_energy()` from `tests/test_common.h`. See `docs/physics.md` for derivation.
