# Ray - Distributed Game Client

A raylib-based game client written in pure C. The client handles rendering and input, connecting to a distributed Go backend (separate repository) for physics and game state.

## Table of Contents

1. [Build System](#build-system)
   - [Prerequisites](#prerequisites)
   - [Build Commands](#build-commands)
   - [Debug Build](#debug-build)
2. [Testing](#testing)
   - [Running Tests](#running-tests)
   - [Test Index](#test-index)
   - [Testing Rules](#testing-rules)
3. [Architecture](#architecture)
   - [Project Structure](#project-structure)
   - [Matter System](#matter-system-chunks)
4. [Code Conventions](#code-conventions)
5. [Physics Reference](#physics-reference)
   - [Thermodynamic Model](#thermodynamic-model)
   - [Reference Values](#reference-values)

---

## Build System

Uses [Meson](https://mesonbuild.com/) build system. **All builds must use these commands.**

### Prerequisites

On Ubuntu/Debian:
```bash
sudo apt install libraylib-dev meson ninja-build
```

On macOS:
```bash
brew install raylib meson ninja
```

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
docs/          # Documentation
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
- `MaterialType` - Enum: `MAT_WATER`, `MAT_ROCK`, etc.
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

## Physics Reference

### Test Philosophy

Tests must be **scientifically rigorous**:

1. **Theory First**: Every test traces to a physical law
   - Conservation of mass/energy
   - Fourier's Law of heat conduction
   - Phase transition thermodynamics

2. **Derived, Not Assumed**: Properties derived from fundamentals
   - Temperature derived from thermal energy: `T = f(E)`
   - Phase derived from energy thresholds, not temperature alone
   - No "ambient temperature" fallback - vacuum has no temperature

3. **Hermetic System**: Energy/mass only enter through explicit tools
   - No spontaneous energy generation
   - Empty cells return 0.0 for temperature (sentinel value)

### Thermodynamic Model

```
Energy Thresholds (for n moles):
  E_melt_start = n * Cp_s * Tm
  E_melt_end   = E_melt_start + n * Hf
  E_boil_start = E_melt_end + n * Cp_l * (Tb - Tm)
  E_boil_end   = E_boil_start + n * Hv

Temperature from Energy:
  Solid:           T = E / (n * Cp_s)                      [E < E_melt_start]
  Melting plateau: T = Tm                                  [E_melt_start <= E < E_melt_end]
  Liquid:          T = Tm + (E - E_melt_end) / (n * Cp_l)  [E_melt_end <= E < E_boil_start]
  Boiling plateau: T = Tb                                  [E_boil_start <= E < E_boil_end]
  Gas:             T = Tb + (E - E_boil_end) / (n * Cp_g)  [E >= E_boil_end]
```

### Reference Values

| Substance | Cp_s | Cp_l | Cp_g | Tm (K) | Tb (K) | Hf (J/mol) | Hv (J/mol) |
|-----------|------|------|------|--------|--------|------------|------------|
| H2O       | 38.0 | 75.3 | 33.6 | 273.15 | 373.15 | 6,010      | 40,660     |
| SiO2      | 44.4 | 82.6 | 47.4 | 1,986  | 2,503  | 9,600      | 600,000    |
| N2        | 29.1 | 29.1 | 29.1 | 63.15  | 77.36  | 720        | 5,560      |
| O2        | 29.4 | 29.4 | 29.4 | 54.36  | 90.19  | 444        | 6,820      |

### Energy Calculation Helper

Test files should use this helper:

```c
static double calculate_material_energy(MaterialType type, double moles, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double Cp_s = props->molar_heat_capacity_solid;
    double Cp_l = props->molar_heat_capacity_liquid;
    double Cp_g = props->molar_heat_capacity_gas;
    double Tm = props->melting_point;
    double Tb = props->boiling_point;
    double Hf = props->enthalpy_fusion;
    double Hv = props->enthalpy_vaporization;

    if (temp_k <= Tm) {
        return moles * Cp_s * temp_k;
    } else if (temp_k <= Tb) {
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (temp_k - Tm);
    } else {
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (Tb - Tm)
             + moles * Hv + moles * Cp_g * (temp_k - Tb);
    }
}
```

### Constants

- `INITIAL_TEMP_K = 293.0` (20C) - Only for initializing new matter from tools
- Vacuum (empty cells) return 0.0 for temperature queries
