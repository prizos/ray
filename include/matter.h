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

// ============ PHASEABLE SUBSTANCES ============
// Substances that can transition between solid/liquid/gas at simulation temperatures

typedef enum {
    PHASEABLE_H2O = 0,      // 273K melting, 373K boiling
    PHASEABLE_SILICATE,     // 2259K melting, 2776K boiling
    PHASEABLE_N2,           // 63K melting, 77K boiling (cryogenic)
    PHASEABLE_O2,           // 54K melting, 90K boiling (cryogenic)
    PHASEABLE_COUNT         // 4 substances with phase tracking
} PhaseableSubstance;

// Mass tracked per phase for phaseable substances
typedef struct {
    fixed16_t solid;
    fixed16_t liquid;
    fixed16_t gas;
} PhaseMass;

// ============ GEOLOGY TYPES ============
// Layered ground with different properties

typedef enum {
    GEOLOGY_NONE = 0,       // Air/empty
    GEOLOGY_TOPSOIL,        // Organic-rich, lower melting point
    GEOLOGY_ROCK,           // Standard silicate rock
    GEOLOGY_BEDROCK,        // Dense, harder to melt (+10% melting point)
    GEOLOGY_LAVA,           // Molten silicate (derived from liquid silicate presence)
    GEOLOGY_IGNITE,         // Cooled lava - becomes rock
} GeologyType;

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

// ============ PHYSICAL CONSTANTS FOR ALL PHASEABLE SUBSTANCES ============

// --- WATER (H2O) ---
#define WATER_MELTING_POINT     FLOAT_TO_FIXED(273.15f)   // 0°C
#define WATER_BOILING_POINT     FLOAT_TO_FIXED(373.15f)   // 100°C
#define LATENT_HEAT_H2O_FUSION       FLOAT_TO_FIXED(334.0f)    // J/g Ice↔Water
#define LATENT_HEAT_H2O_VAPORIZATION FLOAT_TO_FIXED(2260.0f)   // J/g Water↔Steam
#define SPECIFIC_HEAT_H2O_SOLID      FLOAT_TO_FIXED(2.09f)     // Ice
#define SPECIFIC_HEAT_H2O_LIQUID     FLOAT_TO_FIXED(4.18f)     // Water
#define SPECIFIC_HEAT_H2O_GAS        FLOAT_TO_FIXED(2.01f)     // Steam

// --- SILICATE (SiO2) - Rock/Sand/Glass ---
#define SILICATE_MELTING_POINT  FLOAT_TO_FIXED(2259.0f)   // 1986°C
#define SILICATE_BOILING_POINT  FLOAT_TO_FIXED(2776.0f)   // 2503°C
#define LATENT_HEAT_SILICATE_FUSION       FLOAT_TO_FIXED(400.0f)   // J/g
#define LATENT_HEAT_SILICATE_VAPORIZATION FLOAT_TO_FIXED(12000.0f) // J/g
#define SPECIFIC_HEAT_SILICATE_SOLID      FLOAT_TO_FIXED(0.7f)
#define SPECIFIC_HEAT_SILICATE_LIQUID     FLOAT_TO_FIXED(1.0f)     // Lava
#define SPECIFIC_HEAT_SILICATE_GAS        FLOAT_TO_FIXED(0.8f)

// --- NITROGEN (N2) - Cryogenic ---
#define NITROGEN_MELTING_POINT  FLOAT_TO_FIXED(63.15f)    // -210°C
#define NITROGEN_BOILING_POINT  FLOAT_TO_FIXED(77.36f)    // -196°C
#define LATENT_HEAT_N2_FUSION       FLOAT_TO_FIXED(25.7f)
#define LATENT_HEAT_N2_VAPORIZATION FLOAT_TO_FIXED(199.0f)
#define SPECIFIC_HEAT_N2_SOLID      FLOAT_TO_FIXED(1.0f)
#define SPECIFIC_HEAT_N2_LIQUID     FLOAT_TO_FIXED(2.0f)
#define SPECIFIC_HEAT_N2_GAS        FLOAT_TO_FIXED(1.04f)

// --- OXYGEN (O2) - Cryogenic ---
#define OXYGEN_MELTING_POINT    FLOAT_TO_FIXED(54.36f)    // -219°C
#define OXYGEN_BOILING_POINT    FLOAT_TO_FIXED(90.19f)    // -183°C
#define LATENT_HEAT_O2_FUSION       FLOAT_TO_FIXED(13.9f)
#define LATENT_HEAT_O2_VAPORIZATION FLOAT_TO_FIXED(213.0f)
#define SPECIFIC_HEAT_O2_SOLID      FLOAT_TO_FIXED(0.9f)
#define SPECIFIC_HEAT_O2_LIQUID     FLOAT_TO_FIXED(1.7f)
#define SPECIFIC_HEAT_O2_GAS        FLOAT_TO_FIXED(0.92f)

// --- LEGACY ALIASES (for compatibility) ---
#define LATENT_HEAT_FUSION      LATENT_HEAT_H2O_FUSION
#define LATENT_HEAT_VAPORIZATION LATENT_HEAT_H2O_VAPORIZATION
#define SPECIFIC_HEAT_ICE       SPECIFIC_HEAT_H2O_SOLID
#define SPECIFIC_HEAT_WATER     SPECIFIC_HEAT_H2O_LIQUID
#define SPECIFIC_HEAT_STEAM     SPECIFIC_HEAT_H2O_GAS

// Water-matter sync constant
#define WATER_MASS_PER_DEPTH    FLOAT_TO_FIXED(1.0f)  // g water per unit depth

// Phase transition rate limit (prevents instability)
#define PHASE_TRANSITION_RATE   FLOAT_TO_FIXED(0.1f)  // max mass per tick

// Geology modifiers
#define GEOLOGY_BEDROCK_MELT_MULT   FLOAT_TO_FIXED(1.1f)   // +10% melting point
#define GEOLOGY_TOPSOIL_MELT_MULT   FLOAT_TO_FIXED(0.95f)  // -5% melting point

// ============ CELL STRUCTURE ============
// A single cell in the simulation grid
// Energy is stored IN the matter, temperature is derived

typedef struct {
    // Phase-tracked substances (4 substances * 3 phases each)
    // H2O: solid=ice, liquid=water, gas=steam
    // Silicate: solid=rock, liquid=lava, gas=silicate vapor
    // N2/O2: cryogenic phases
    PhaseMass phase_mass[PHASEABLE_COUNT];

    // Non-phaseable substances (always fixed phase at sim temps)
    fixed16_t co2_gas;              // Always gas (sublimes)
    fixed16_t smoke_gas;            // Always gas (particulates)
    fixed16_t ash_solid;            // Always solid (residue)
    fixed16_t cellulose_solid;      // Decomposes, doesn't melt

    // Thermal state
    fixed16_t energy;               // Total thermal energy (Joules)
    fixed16_t temperature;          // K = energy / thermal_mass (cached)

    // Geology layer info
    uint8_t geology_type;           // GeologyType enum
    uint8_t depth_from_surface;     // 0-255 cells from original surface
    uint16_t _geology_padding;      // Alignment padding

    // Cached values (recomputed each step)
    fixed16_t thermal_mass;         // Sum(mass[i] * specific_heat[i])
    fixed16_t total_mass;           // Sum of all mass

    // Per-phase totals (derived from phase_mass)
    fixed16_t solid_mass;
    fixed16_t liquid_mass;
    fixed16_t gas_mass;

    // Environmental (set externally)
    fixed16_t light_level;          // 0-1: available sunlight
    int terrain_height;             // Ground level at this cell

} MatterCell;

// ============ CONVENIENCE MACROS FOR PHASE ACCESS ============
// Access phase mass for common substances

#define CELL_H2O_ICE(cell)      ((cell)->phase_mass[PHASEABLE_H2O].solid)
#define CELL_H2O_LIQUID(cell)   ((cell)->phase_mass[PHASEABLE_H2O].liquid)
#define CELL_H2O_STEAM(cell)    ((cell)->phase_mass[PHASEABLE_H2O].gas)

#define CELL_SILICATE_SOLID(cell)  ((cell)->phase_mass[PHASEABLE_SILICATE].solid)
#define CELL_SILICATE_LIQUID(cell) ((cell)->phase_mass[PHASEABLE_SILICATE].liquid)
#define CELL_SILICATE_GAS(cell)    ((cell)->phase_mass[PHASEABLE_SILICATE].gas)

#define CELL_N2_SOLID(cell)     ((cell)->phase_mass[PHASEABLE_N2].solid)
#define CELL_N2_LIQUID(cell)    ((cell)->phase_mass[PHASEABLE_N2].liquid)
#define CELL_N2_GAS(cell)       ((cell)->phase_mass[PHASEABLE_N2].gas)

#define CELL_O2_SOLID(cell)     ((cell)->phase_mass[PHASEABLE_O2].solid)
#define CELL_O2_LIQUID(cell)    ((cell)->phase_mass[PHASEABLE_O2].liquid)
#define CELL_O2_GAS(cell)       ((cell)->phase_mass[PHASEABLE_O2].gas)

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

// Add mass of a substance to a cell (routes to correct phase based on temperature)
void cell_add_mass(MatterCell *cell, Substance s, fixed16_t amount);

// Remove mass (returns actual amount removed)
fixed16_t cell_remove_mass(MatterCell *cell, Substance s, fixed16_t amount);

// Add thermal energy to a cell
void cell_add_energy(MatterCell *cell, fixed16_t joules);

// Get mass of a specific substance (all phases combined for phaseable substances)
fixed16_t cell_get_mass(const MatterCell *cell, Substance s);

// Get total mass of a phaseable substance (all 3 phases)
fixed16_t cell_get_phaseable_total(const MatterCell *cell, PhaseableSubstance ps);

// Get total mass of all fuel substances
fixed16_t cell_get_fuel_mass(const MatterCell *cell);

// Check if combustion can occur for a fuel
bool cell_can_combust(const MatterCell *cell, Substance fuel);

// Recompute cached values (temperature, thermal_mass, phase totals)
void cell_update_cache(MatterCell *cell);

// Get temperature (uses cached value)
fixed16_t cell_get_temperature(const MatterCell *cell);

// ============ API: GEOLOGY ============

// Initialize geology type based on terrain height
void matter_init_geology(MatterState *state, const int terrain[MATTER_RES][MATTER_RES]);

// Get effective melting point for silicate based on geology type
fixed16_t cell_get_silicate_melting_point(const MatterCell *cell);

// Update geology type based on phase state (lava detection)
void cell_update_geology(MatterCell *cell);

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

// Process phase transitions for ALL phaseable substances
void matter_process_phase_transitions(MatterState *state);

// Process phase transition for a single substance in a cell
void cell_process_phase_transition(MatterCell *cell, PhaseableSubstance ps);

// Flow liquids (water and lava) based on terrain slope and viscosity
void matter_flow_liquids(MatterState *state);

// ============ API: WATER SYNC ============

// Forward declaration for water state
struct WaterState;

// Sync liquid water from water simulation to matter system
void matter_sync_from_water(MatterState *matter, const struct WaterState *water);

// Sync liquid water from matter system back to water simulation
void matter_sync_to_water(const MatterState *matter, struct WaterState *water);

// Get total H2O mass in a cell (all phases)
fixed16_t cell_total_h2o(const MatterCell *cell);

// ============ API: PHASEABLE SUBSTANCE PROPERTIES ============

// Get melting point for a phaseable substance (with geology modifiers for silicate)
fixed16_t get_phaseable_melting_point(PhaseableSubstance ps, const MatterCell *cell);

// Get boiling point for a phaseable substance
fixed16_t get_phaseable_boiling_point(PhaseableSubstance ps);

// Get latent heat of fusion for a phaseable substance
fixed16_t get_phaseable_latent_fusion(PhaseableSubstance ps);

// Get latent heat of vaporization for a phaseable substance
fixed16_t get_phaseable_latent_vaporization(PhaseableSubstance ps);

// Get specific heat for a phaseable substance in a given phase
fixed16_t get_phaseable_specific_heat(PhaseableSubstance ps, Phase phase);

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
