#ifndef MATTER_H
#define MATTER_H

#include "raylib.h"
#include "water.h"  // For fixed-point types
#include <stdbool.h>
#include <stdint.h>

// ============ CONSTANTS ============

#define MATTER_RES 160
#define MATTER_CELL_SIZE 2.5f

// Update rate
#define MATTER_UPDATE_HZ 30.0f
#define MATTER_UPDATE_DT (1.0f / MATTER_UPDATE_HZ)

// Temperature constants (in Kelvin, stored as fixed16_t)
#define KELVIN_ZERO       FLOAT_TO_FIXED(273.15f)  // 0°C in Kelvin
#define AMBIENT_TEMP      FLOAT_TO_FIXED(293.15f)  // 20°C

// Energy transfer rates
#define CONDUCTION_RATE      FLOAT_TO_FIXED(0.05f)  // Heat transfer coefficient
#define RADIATION_RATE       FLOAT_TO_FIXED(0.002f) // Radiative exchange with environment

// ============ SUBSTANCES ============
// Based on real physical/chemical properties

typedef enum {
    SUBST_NONE = 0,

    // Minerals
    SUBST_SILICATE,     // SiO2 - sand, rock, glass

    // Water (phase varies with temperature)
    SUBST_H2O,

    // Atmosphere
    SUBST_NITROGEN,     // N2 - 78% of air
    SUBST_OXYGEN,       // O2 - 21% of air, oxidizer for combustion

    // Combustion products
    SUBST_CO2,          // Carbon dioxide
    SUBST_SMOKE,        // Particulates
    SUBST_ASH,          // Solid residue

    // Organic (biomass)
    SUBST_CELLULOSE,    // Plant matter (simplified organic compound)

    SUBST_COUNT         // 9 substances
} Substance;

// ============ PHASE ============
// Derived from temperature, not stored

typedef enum {
    PHASE_SOLID,
    PHASE_LIQUID,
    PHASE_GAS
} Phase;

// ============ SUBSTANCE PROPERTIES ============
// Real physical constants for each substance

typedef struct {
    const char *name;
    const char *formula;

    // Molecular properties
    fixed16_t molecular_weight;     // g/mol - affects gas diffusion rate
    bool is_polar;                  // affects miscibility with water

    // Phase transitions (Kelvin at 1 atm)
    fixed16_t melting_point;        // solid → liquid (0 = doesn't melt at sim temps)
    fixed16_t boiling_point;        // liquid → gas (0 = doesn't boil at sim temps)

    // Density by phase (kg/m³)
    fixed16_t density_solid;
    fixed16_t density_liquid;
    fixed16_t density_gas;

    // Thermal properties
    fixed16_t specific_heat;        // J/(g·K) - energy to raise 1g by 1K
    fixed16_t conductivity;         // W/(m·K) - heat transfer rate

    // Solid structure (for future use)
    fixed16_t porosity;             // 0-1 void fraction (0 = non-porous)
    fixed16_t permeability;         // fluid flow rate through pores (0 for now)

    // Chemistry
    bool is_oxidizer;               // supports combustion (O2)
    bool is_fuel;                   // can burn (cellulose)
    fixed16_t ignition_temp;        // K - temperature to ignite (0 = non-flammable)
    fixed16_t heat_of_combustion;   // J/g - energy released when burned

    // Visual
    Color color;                    // Base rendering color

} SubstanceProps;

// Global properties table (defined in matter.c)
extern const SubstanceProps SUBST_PROPS[SUBST_COUNT];

// ============ PHYSICAL CONSTANTS FOR WATER ============

// Phase transition temperatures (Kelvin)
#define WATER_MELTING_POINT     FLOAT_TO_FIXED(273.15f)
#define WATER_BOILING_POINT     FLOAT_TO_FIXED(373.15f)

// Latent heats (J/g) - energy for phase change WITHOUT temperature change
#define LATENT_HEAT_FUSION      FLOAT_TO_FIXED(334.0f)    // Ice ↔ Water
#define LATENT_HEAT_VAPORIZATION FLOAT_TO_FIXED(2260.0f)  // Water ↔ Steam

// Specific heats (J/g·K) - energy to raise 1g by 1K
#define SPECIFIC_HEAT_ICE       FLOAT_TO_FIXED(2.09f)
#define SPECIFIC_HEAT_WATER     FLOAT_TO_FIXED(4.18f)
#define SPECIFIC_HEAT_STEAM     FLOAT_TO_FIXED(2.01f)

// Water-matter sync constant
#define WATER_MASS_PER_DEPTH    FLOAT_TO_FIXED(1.0f)  // g water per unit depth

// ============ CELL STRUCTURE ============
// A single cell in the simulation grid
// Energy is stored IN the matter, temperature is derived

typedef struct {
    // Primary state
    fixed16_t mass[SUBST_COUNT];    // Mass of each substance (g) - excludes H2O
    fixed16_t energy;               // Total thermal energy (Joules)

    // H2O tracked by phase (separate from mass array for proper thermodynamics)
    fixed16_t h2o_ice;              // Solid water (g)
    fixed16_t h2o_liquid;           // Liquid water (g) - synced with water sim
    fixed16_t h2o_steam;            // Steam/vapor (g)

    // Cached values (recomputed each step)
    fixed16_t temperature;          // K = energy / thermal_mass
    fixed16_t thermal_mass;         // Sum(mass[i] * specific_heat[i])
    fixed16_t total_mass;           // Sum of all mass including H2O

    // Per-phase totals (derived from mass + temperature)
    fixed16_t solid_mass;
    fixed16_t liquid_mass;
    fixed16_t gas_mass;

    // Environmental (set externally)
    fixed16_t light_level;          // 0-1: available sunlight
    int terrain_height;             // Ground level at this cell

} MatterCell;

// ============ SIMULATION STATE ============

typedef struct {
    // Grid of cells
    MatterCell cells[MATTER_RES][MATTER_RES];

    // Simulation state
    uint32_t tick;
    float accumulator;
    bool initialized;

    // Conservation tracking
    fixed16_t total_energy;

    // Debug
    uint32_t checksum;

} MatterState;

// ============ API: PHASE ============

// Get the phase of a substance at a given temperature
Phase substance_get_phase(Substance s, fixed16_t temp_kelvin);

// ============ API: INITIALIZATION ============

// Initialize the matter system
void matter_init(MatterState *state, const int terrain[MATTER_RES][MATTER_RES], uint32_t seed);

// Reset to initial conditions
void matter_reset(MatterState *state);

// ============ API: CELL OPERATIONS ============

// Add mass of a substance to a cell
void cell_add_mass(MatterCell *cell, Substance s, fixed16_t amount);

// Remove mass (returns actual amount removed)
fixed16_t cell_remove_mass(MatterCell *cell, Substance s, fixed16_t amount);

// Add thermal energy to a cell
void cell_add_energy(MatterCell *cell, fixed16_t joules);

// Get mass of a specific substance
fixed16_t cell_get_mass(const MatterCell *cell, Substance s);

// Get total mass of all fuel substances
fixed16_t cell_get_fuel_mass(const MatterCell *cell);

// Check if combustion can occur for a fuel
bool cell_can_combust(const MatterCell *cell, Substance fuel);

// Recompute cached values (temperature, thermal_mass, phase totals)
void cell_update_cache(MatterCell *cell);

// Get temperature (uses cached value)
fixed16_t cell_get_temperature(const MatterCell *cell);

// ============ API: SIMULATION ============

// Main update (call every frame with deltaTime)
int matter_update(MatterState *state, float delta_time);

// Single simulation step
void matter_step(MatterState *state);

// Heat conduction between cells
void matter_conduct_heat(MatterState *state);

// Process combustion for all cells
void matter_process_combustion(MatterState *state);

// Gas diffusion (optional, for smoke spread)
void matter_diffuse_gases(MatterState *state);

// Process phase transitions (evaporation, condensation, melting, freezing)
void matter_process_phase_transitions(MatterState *state);

// ============ API: WATER SYNC ============

// Forward declaration for water state
struct WaterState;

// Sync liquid water from water simulation to matter system
void matter_sync_from_water(MatterState *matter, const struct WaterState *water);

// Sync liquid water from matter system back to water simulation
void matter_sync_to_water(const MatterState *matter, struct WaterState *water);

// Get total H2O mass in a cell (all phases)
fixed16_t cell_total_h2o(const MatterCell *cell);

// ============ API: QUERIES ============

// Get cell at position
MatterCell* matter_get_cell(MatterState *state, int x, int z);
const MatterCell* matter_get_cell_const(const MatterState *state, int x, int z);

// World coordinate conversion
void matter_world_to_cell(float world_x, float world_z, int *cell_x, int *cell_z);
void matter_cell_to_world(int cell_x, int cell_z, float *world_x, float *world_z);

// ============ API: EXTERNAL INTERACTIONS ============

// Add heat at world position (e.g., from fire tool)
void matter_add_heat_at(MatterState *state, float world_x, float world_z,
                        fixed16_t energy);

// Add water at world position (from water simulation)
void matter_add_water_at(MatterState *state, float world_x, float world_z,
                         fixed16_t mass);

// Set light level from external source (tree shadows)
void matter_set_light(MatterState *state, int x, int z, fixed16_t level);

// ============ API: CONSERVATION ============

// Calculate total energy in system
fixed16_t matter_total_energy(const MatterState *state);

// Calculate total mass of a substance
fixed16_t matter_total_mass(const MatterState *state, Substance s);

// Calculate checksum for network sync
uint32_t matter_checksum(const MatterState *state);

// ============ UTILITY ============

static inline bool matter_cell_valid(int x, int z) {
    return x >= 0 && x < MATTER_RES && z >= 0 && z < MATTER_RES;
}

// Convert temperature Kelvin to Celsius (for display)
static inline float kelvin_to_celsius(fixed16_t k) {
    return FIXED_TO_FLOAT(k) - 273.15f;
}

// Convert Celsius to Kelvin fixed-point
static inline fixed16_t celsius_to_kelvin(float c) {
    return FLOAT_TO_FIXED(c + 273.15f);
}

#endif // MATTER_H
