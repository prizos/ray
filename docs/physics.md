# Physics Model Reference

Thermodynamic and heat transfer models used in the matter simulation.

## Table of Contents

1. [Energy-Temperature Relationship](#energy-temperature-relationship)
2. [Phase Transitions](#phase-transitions)
3. [Heat Transfer](#heat-transfer)
4. [Discrete Simulation](#discrete-simulation)

---

## Energy-Temperature Relationship

Temperature is **derived from thermal energy**, not stored directly. This ensures energy conservation.

### Single-Phase (No Transitions)

For a material entirely in one phase:

```
T = E / (n * Cp)

Where:
  T  = Temperature (K)
  E  = Thermal energy (J)
  n  = Amount of substance (mol)
  Cp = Molar heat capacity at constant pressure (J/(mol*K))
```

### Multi-Phase (With Transitions)

Energy must account for latent heat at phase boundaries:

```
Energy Thresholds:
  E_melt_start = n * Cp_s * Tm
  E_melt_end   = E_melt_start + n * Hf
  E_boil_start = E_melt_end + n * Cp_l * (Tb - Tm)
  E_boil_end   = E_boil_start + n * Hv

Temperature from Energy:
  E < E_melt_start:                    T = E / (n * Cp_s)           [SOLID]
  E_melt_start <= E < E_melt_end:      T = Tm                       [MELTING]
  E_melt_end <= E < E_boil_start:      T = Tm + (E - E_melt_end) / (n * Cp_l)  [LIQUID]
  E_boil_start <= E < E_boil_end:      T = Tb                       [BOILING]
  E >= E_boil_end:                     T = Tb + (E - E_boil_end) / (n * Cp_g)  [GAS]

Where:
  Cp_s = Molar heat capacity of solid (J/(mol*K))
  Cp_l = Molar heat capacity of liquid (J/(mol*K))
  Cp_g = Molar heat capacity of gas (J/(mol*K))
  Tm   = Melting point (K)
  Tb   = Boiling point (K)
  Hf   = Molar enthalpy of fusion (J/mol)
  Hv   = Molar enthalpy of vaporization (J/mol)
```

### Energy from Temperature

Inverse calculation for initializing materials:

```c
double energy_from_temperature(double n, double T, MaterialProps *p) {
    if (T <= p->Tm) {
        // Solid
        return n * p->Cp_s * T;
    } else if (T <= p->Tb) {
        // Liquid
        return n * p->Cp_s * p->Tm
             + n * p->Hf
             + n * p->Cp_l * (T - p->Tm);
    } else {
        // Gas
        return n * p->Cp_s * p->Tm
             + n * p->Hf
             + n * p->Cp_l * (p->Tb - p->Tm)
             + n * p->Hv
             + n * p->Cp_g * (T - p->Tb);
    }
}
```

---

## Phase Transitions

### Latent Heat

During phase transitions, energy is absorbed/released without temperature change:

| Transition | Energy | Name |
|------------|--------|------|
| Solid -> Liquid | +Hf | Fusion (melting) |
| Liquid -> Solid | -Hf | Solidification (freezing) |
| Liquid -> Gas | +Hv | Vaporization (boiling) |
| Gas -> Liquid | -Hv | Condensation |
| Solid -> Gas | +(Hf + Hv) | Sublimation |

### Phase Determination

Phase is determined by **energy level**, not temperature:

```c
Phase get_phase(double E, double n, MaterialProps *p) {
    double E_melt_start = n * p->Cp_s * p->Tm;
    double E_melt_end   = E_melt_start + n * p->Hf;
    double E_boil_start = E_melt_end + n * p->Cp_l * (p->Tb - p->Tm);
    double E_boil_end   = E_boil_start + n * p->Hv;

    if (E < E_melt_end)   return PHASE_SOLID;
    if (E < E_boil_end)   return PHASE_LIQUID;
    return PHASE_GAS;
}
```

**Note**: During melting (E_melt_start to E_melt_end), the material is technically a solid-liquid mixture at constant T=Tm. For simplicity, we treat it as solid until fully melted.

---

## Heat Transfer

### Fourier's Law of Conduction

Heat flows from hot to cold, proportional to the temperature gradient:

```
q = -k * A * (dT/dx)

Where:
  q    = Heat transfer rate (W = J/s)
  k    = Thermal conductivity (W/(m*K))
  A    = Cross-sectional area (m^2)
  dT   = Temperature difference (K)
  dx   = Distance (m)
```

Source: [Thermal conduction - Wikipedia](https://en.wikipedia.org/wiki/Thermal_conduction)

### Discrete Cell Implementation

For adjacent cells in a voxel grid:

```
Q = k_eff * A * (T_hot - T_cold) * dt / dx

Where:
  Q     = Energy transferred this timestep (J)
  k_eff = Effective conductivity between cells (W/(m*K))
  A     = Contact area (cell_size^2)
  dt    = Timestep (s)
  dx    = Cell center distance (cell_size)
```

### Effective Conductivity

For two materials in series (thermal resistance):

```
k_eff = 2 * k1 * k2 / (k1 + k2)
```

This is the harmonic mean, appropriate for series thermal resistance.

### Energy Conservation

**Critical**: Energy transferred must be capped to prevent:
1. Negative energy in source cell
2. Temperature overshoot (source becoming colder than destination)

```c
// Cap energy transfer
double max_transfer = 0.5 * (E_hot - E_cold);  // At most equalize
Q = fmin(Q, max_transfer);
Q = fmax(Q, 0);  // No negative transfer
```

---

## Discrete Simulation

### Timestep Selection

For stability in explicit heat conduction:

```
dt_max = dx^2 / (2 * alpha * D)

Where:
  dx    = Cell size (m)
  alpha = Thermal diffusivity = k / (rho * Cp)
  D     = Number of dimensions (6 for 3D with all neighbors)
```

### Update Order

To prevent directional bias, use one of:

1. **Checkerboard pattern**: Alternate even/odd cells each substep
2. **Randomized order**: Shuffle cell processing order
3. **Symmetric updates**: Process each pair once, split energy change

### Neighbor Access

For a 3D grid, each cell has 6 face-neighbors:

```
Direction offsets (dx, dy, dz):
  +X: (1, 0, 0)    -X: (-1, 0, 0)
  +Y: (0, 1, 0)    -Y: (0, -1, 0)
  +Z: (0, 0, 1)    -Z: (0, 0, -1)
```

### Fluid Flow

Liquids and gases flow based on:

1. **Gravity**: Liquids flow down, gases rise (buoyancy)
2. **Pressure**: Flow from high to low pressure
3. **Viscosity**: Resistance to flow (higher = slower)

Simplified model for liquids:
```
- If cell below has space: flow down (gravity)
- If cell below is full: spread horizontally
- Transfer rate proportional to 1/viscosity
```

---

## Constants

| Constant | Value | Unit | Description |
|----------|-------|------|-------------|
| INITIAL_TEMP_K | 293.0 | K | Default temperature (20C) |
| TEMP_EPSILON | 0.1 | K | Equilibrium tolerance |
| MOLES_EPSILON | 1e-10 | mol | Empty cell threshold |

### Universal Constants

| Constant | Value | Unit |
|----------|-------|------|
| R (gas constant) | 8.314 | J/(mol*K) |
| Avogadro's number | 6.022e23 | 1/mol |

---

## Sources

- [NIST Chemistry WebBook](https://webbook.nist.gov/) - Thermodynamic data
- [Wikipedia: Thermal conduction](https://en.wikipedia.org/wiki/Thermal_conduction) - Fourier's Law
- [Wikipedia: Enthalpy of fusion](https://en.wikipedia.org/wiki/Enthalpy_of_fusion) - Latent heat values
- [Wikipedia: Molar heat capacity](https://en.wikipedia.org/wiki/Molar_heat_capacity) - Cp values
