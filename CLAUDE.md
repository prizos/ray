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

## Testing

### Running Tests

```bash
make test              # Run all tests
make test-svo-unit     # SVO unit tests
make test-svo-physics  # SVO physics tests
make test-3d-physics   # 3D physics integration tests
make test-tools        # Tool integration tests
```

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

The matter system uses molar thermodynamics with proper latent heat handling:

```
Energy Thresholds (for n moles of material):
  E_melt_start = n × Cp × Tm
  E_melt_end   = E_melt_start + n × Hf
  E_boil_start = E_melt_end + n × Cp × (Tb - Tm)
  E_boil_end   = E_boil_start + n × Hv

Temperature as function of Energy:
  Solid:           T = E / (n × Cp)                           [E < E_melt_start]
  Melting plateau: T = Tm                                     [E_melt_start ≤ E < E_melt_end]
  Liquid:          T = Tm + (E - E_melt_end) / (n × Cp)       [E_melt_end ≤ E < E_boil_start]
  Boiling plateau: T = Tb                                     [E_boil_start ≤ E < E_boil_end]
  Gas:             T = Tb + (E - E_boil_end) / (n × Cp)       [E ≥ E_boil_end]

Where:
  Cp = molar heat capacity (J/(mol·K))
  Tm = melting point (K)
  Tb = boiling point (K)
  Hf = enthalpy of fusion (J/mol)
  Hv = enthalpy of vaporization (J/mol)
```

### Writing Tests

#### Energy Calculation Helper

All test files must include this helper to compute energy correctly:

```c
static double calculate_material_energy(MaterialType type, double moles, double temp_k) {
    const MaterialProperties *props = &MATERIAL_PROPS[type];
    double Cp = props->molar_heat_capacity;
    double Tm = props->melting_point;
    double Tb = props->boiling_point;
    double Hf = props->enthalpy_fusion;
    double Hv = props->enthalpy_vaporization;

    if (temp_k <= Tm) {
        return moles * Cp * temp_k;  // Solid
    } else if (temp_k <= Tb) {
        return moles * Cp * Tm + moles * Hf + moles * Cp * (temp_k - Tm);  // Liquid
    } else {
        return moles * Cp * Tm + moles * Hf + moles * Cp * (Tb - Tm)
             + moles * Hv + moles * Cp * (temp_k - Tb);  // Gas
    }
}
```

#### Common Mistakes to Avoid

**WRONG** - Simple energy formula (ignores latent heat):
```c
double energy = moles * heat_capacity * temperature;  // WRONG!
```

**CORRECT** - Use the helper:
```c
double energy = calculate_material_energy(MAT_WATER, moles, temperature);
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
