# Ray - Distributed Game Client

A raylib-based game client written in pure C. The client handles rendering and input, connecting to a distributed Go backend (separate repository) for physics and game state.

## Architecture

- **Client (this repo)**: Pure C with raylib - rendering, input, network receive
- **Backend (separate)**: Go - distributed physics, game state, coordination

## Build

Uses [Meson](https://mesonbuild.com/) build system.

### Prerequisites

Requires raylib and meson installed:

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

## Project Structure

```
src/           # C source files
include/       # Header files
assets/        # Game assets (textures, sounds, etc.)
build/         # Build output (gitignored)
docs/          # Documentation and raylib reference
```

### Matter System (Chunks)

The 3D matter simulation uses a chunk-based system for efficient storage and physics:

- `chunk.h` - Core structures, cell operations, material properties
- `chunk.c` - Cell operations, material state management
- `chunk_world.c` - World management, chunk hash table, tool APIs
- `chunk_physics.c` - Heat conduction, phase transitions, fluid flow

Key types:
- `ChunkWorld` - Hash table of chunks containing all matter
- `Chunk` - 32×32×32 cell block with O(1) neighbor access
- `Cell3D` - A cell containing multiple materials (bitmask-based)
- `MaterialType` - Enum of material types (MAT_WATER, MAT_ROCK, etc.)
- `MaterialProperties` - Physical constants per material (in `MATERIAL_PROPS` table)
- `MaterialState` - Per-material state: `moles` + `thermal_energy`

Legacy SVO API compatibility macros are provided in `chunk.h` for migration.

## Code Conventions

- **Naming**: snake_case for functions and variables, PascalCase for types/structs
- **Files**: One module per file pair (module.c + module.h)
- **Memory**: Explicit allocation/deallocation, no hidden mallocs
- **Headers**: Include guards using `#ifndef MODULE_H` / `#define MODULE_H`
- **Raylib**: Use raylib types directly (Vector2, Color, etc.)

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

Available tests:
- `test_svo_unit` - Chunk/SVO unit tests
- `test_svo_physics` - Physics simulation tests
- `test_3d_physics` - 3D physics integration tests
- `test_energy_conservation` - Energy conservation verification
- `test_tool_integration` - Tool API tests
- `test_e2e_simulation` - End-to-end simulation tests
- `test_phase_specific_cp` - Phase-specific heat capacity tests
- `test_noise` - Noise generation tests
- `test_octree` - Octree data structure tests

### Testing Philosophy

Tests must be **scientifically rigorous** they should propose a theory, document it in comments, and then prove it. The simulation is a physics system, and tests must verify that the physics is correct.

#### Core Principles

1. **Theory First**: Every test should be traceable to a physical law or principle or computational expectation
   - Conservation of mass
   - Conservation of energy (First Law of Thermodynamics)
   - Fourier's Law of heat conduction
   - Phase transition thermodynamics
   - Mathematical or algorithmic expectations

2. **Derived, Not Assumed**: Properties are derived from fundamental quantities, never hardcoded
   - Temperature is derived from thermal energy: `T = f(E)` where f accounts for latent heat
   - Phase is derived from energy thresholds, not temperature alone
   - Never use "ambient temperature" as a fallback - vacuum has no temperature

3. **Hermetic System**: Energy and mass only enter through explicit tools/actions
   - No spontaneous energy generation
   - No implicit temperature assumptions
   - If a cell is empty, it has no temperature (return 0.0 as sentinel)

### Thermodynamic Model

The matter system uses molar thermodynamics with proper latent heat handling and phase-specific heat capacities.

```
Energy Thresholds (for n moles of material):
  E_melt_start = n × Cp_s × Tm
  E_melt_end   = E_melt_start + n × Hf
  E_boil_start = E_melt_end + n × Cp_l × (Tb - Tm)
  E_boil_end   = E_boil_start + n × Hv

Temperature as function of Energy:
  Solid:           T = E / (n × Cp_s)                           [E < E_melt_start]
  Melting plateau: T = Tm                                       [E_melt_start ≤ E < E_melt_end]
  Liquid:          T = Tm + (E - E_melt_end) / (n × Cp_l)       [E_melt_end ≤ E < E_boil_start]
  Boiling plateau: T = Tb                                       [E_boil_start ≤ E < E_boil_end]
  Gas:             T = Tb + (E - E_boil_end) / (n × Cp_g)       [E ≥ E_boil_end]

Where:
  Cp_s = molar heat capacity of solid (J/(mol·K))
  Cp_l = molar heat capacity of liquid (J/(mol·K))
  Cp_g = molar heat capacity of gas (J/(mol·K))
  Tm = melting point (K)
  Tb = boiling point (K)
  Hf = molar enthalpy of fusion (J/mol)
  Hv = molar enthalpy of vaporization (J/mol)
```

#### Reference Values

| Substance | Cp_s | Cp_l | Cp_g | Tm (K) | Tb (K) | Hf (J/mol) | Hv (J/mol) |
|-----------|------|------|------|--------|--------|------------|------------|
| H₂O       | 38.0 | 75.3 | 33.6 | 273.15 | 373.15 | 6,010      | 40,660     |
| SiO₂      | 44.4 | 82.6 | 52.0 | 1,986  | 2,503  | 9,600      | 600,000    |
| N₂        | 29.1 | 29.1 | 29.1 | 63.15  | 77.36  | 720        | 5,560      |
| O₂        | 29.4 | 29.4 | 29.4 | 54.36  | 90.19  | 444        | 6,820      |

### Writing Tests

#### Energy Calculation Helper

All test files must include this helper to compute energy correctly:

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
        return moles * Cp_s * temp_k;  // Solid
    } else if (temp_k <= Tb) {
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (temp_k - Tm);  // Liquid
    } else {
        return moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (Tb - Tm)
             + moles * Hv + moles * Cp_g * (temp_k - Tb);  // Gas
    }
}
```

#### Common Mistakes to Avoid

**WRONG** - Simple energy formula (ignores latent heat and phase-specific Cp):
```c
double energy = moles * heat_capacity * temperature;  // WRONG for T > Tm!
```
Note: `E = n × Cp_s × T` is correct ONLY for solids below their melting point.

**CORRECT** - Use the helper:
```c
double energy = calculate_material_energy(MAT_WATER, moles, temperature);
```

**WRONG** - Using single heat capacity for all phases:
```c
double energy = moles * Cp * Tm + moles * Hf + moles * Cp * (temp - Tm);  // WRONG!
```
This incorrectly uses solid Cp for the liquid phase.

**CORRECT** - Use phase-specific heat capacities:
```c
double energy = moles * Cp_s * Tm + moles * Hf + moles * Cp_l * (temp - Tm);
```

**WRONG** - Assuming all water above 273K is liquid:
```c
if (temp > 273.15) phase = PHASE_LIQUID;  // WRONG - doesn't account for latent heat
```

**CORRECT** - Use energy-based phase determination:
```c
Phase phase = material_get_phase_from_energy(&state, type);
```

### Test Categories

| Category | Purpose | Example |
|----------|---------|---------|
| Unit | Single function correctness | `material_get_temperature()` returns correct T |
| Conservation | Physical laws hold | Total energy unchanged during conduction |
| Flow | Fluid dynamics | Water flows down, water spreads, steam rises |
| Phase | State transitions | Water freezes below 273K with latent heat |
| Integration | Tools work correctly | Heat tool adds energy to materials |

### Constants

- `INITIAL_TEMP_K = 293.0` (20°C) - Used ONLY for initializing new matter from tools
- Never use as a fallback temperature
- Vacuum (empty cells) return 0.0 for temperature queries
