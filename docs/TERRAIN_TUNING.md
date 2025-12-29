# Terrain Parameter Tuning Guide

This guide explains how to use the terrain tuning tool to find the best noise parameters for your terrain generation.

## Quick Start

```bash
# 1. Create the config file
make tune-init

# 2. Generate preview images
make tune-terrain

# 3. View the images
open terrain_output/    # macOS
# or: xdg-open terrain_output/  # Linux

# 4. Edit terrain_params.cfg based on what you like
# 5. Repeat steps 2-4 until satisfied
```

## Commands

| Command | Description |
|---------|-------------|
| `make tune-init` | Create `terrain_params.cfg` template |
| `make tune-terrain` | Generate all preview images |
| `make tune-terrain SEED=12345` | Generate with specific seed |
| `make tune-single` | Generate only center values (no variations) |
| `make tune-dry-run` | Show what would be generated |
| `make tune-clean` | Remove all generated images |

## Understanding the Parameters

### Octaves (1-8, default: 6)

Number of noise layers stacked together. Each octave adds finer detail.

| Value | Effect |
|-------|--------|
| 1-2 | Very smooth, gentle rolling hills |
| 3-4 | Moderate detail, natural hills |
| 5-6 | Good detail, realistic terrain |
| 7-8 | High detail, rough/rocky appearance |

**Tip:** Start with 4-6 for natural-looking terrain.

### Lacunarity (1.5-3.0, default: 2.0)

Frequency multiplier between octaves. Controls how quickly detail increases at smaller scales.

| Value | Effect |
|-------|--------|
| 1.5-1.8 | Gradual detail increase, smoother blending |
| 2.0 | Standard (doubles frequency each octave) |
| 2.2-3.0 | Sharp distinction between feature sizes |

**Tip:** 2.0 is a good default. Lower values for more organic terrain.

### Persistence (0.2-0.8, default: 0.5)

Amplitude multiplier between octaves. Controls how much fine detail shows through.

| Value | Effect |
|-------|--------|
| 0.2-0.4 | Smooth terrain dominated by large features |
| 0.5 | Balanced detail (standard) |
| 0.6-0.8 | Rough terrain with prominent fine detail |

**Tip:** Lower values for plains/gentle hills, higher for mountains.

### Scale (0.01-0.1, default: 0.025)

Base noise frequency. Controls the overall size of terrain features.

| Value | Effect |
|-------|--------|
| 0.01-0.015 | Very large features (big mountains/valleys) |
| 0.02-0.03 | Medium features (good for most terrain) |
| 0.04-0.06 | Smaller features (more hills) |
| 0.07-0.1 | Many small features (hilly/bumpy) |

**Tip:** This is often the most impactful parameter. Start here when tuning.

## Workflow

### Step 1: Find the Right Scale

Start by adjusting `scale` to get the right feature size:

```ini
# Disable other splays to focus on scale
octaves_splay = false
lacunarity_splay = false
persistence_splay = false
scale_splay = true
```

Run `make tune-terrain` and compare the scale variations.

### Step 2: Tune Detail Level

Once scale is set, adjust `octaves` and `persistence`:

```ini
octaves_splay = true
persistence_splay = true
scale_splay = false
```

### Step 3: Fine-tune

Finally, adjust `lacunarity` for the blending between detail levels.

### Step 4: Apply to Game

Once satisfied, update the defaults in `include/terrain.h`:

```c
#define TERRAIN_DEFAULT_OCTAVES 5
#define TERRAIN_DEFAULT_LACUNARITY 1.9f
#define TERRAIN_DEFAULT_PERSISTENCE 0.45f
#define TERRAIN_DEFAULT_SCALE 0.02f
```

## Output Files

Images are named with all parameters encoded:

```
terrain_s{seed}_o{octaves}_l{lacunarity}_p{persistence}_sc{scale}_{label}_{type}.png
```

- `seed`: Random seed used
- `octaves`: Number of noise layers
- `lacunarity`: Frequency multiplier
- `persistence`: Amplitude multiplier
- `scale`: Base frequency
- `label`: Variation name (CENTER, oct+50, per-20, etc.)
- `type`: `gray` (heightmap) or `color` (visualization)

## Color Legend

The colored images use the game's terrain colors:

| Color | Meaning |
|-------|---------|
| Blue | Water (height < 3) |
| Brown-green | Low grass (height 3-4) |
| Green | High grass (height 5-7) |
| Gray | Rock (height 8+) |

## Tips

1. **Use a fixed seed** when comparing parameters: `make tune-terrain SEED=12345`

2. **Disable splays** you're not currently tuning to reduce image count

3. **Compare grayscale images** to see actual height distribution

4. **Look at the INDEX.txt** file for a summary of all generated variations

5. **Water coverage** is controlled by `height_min`/`height_max` and `WATER_LEVEL` (3)
   - To increase water: lower `height_max` or raise `height_min`
   - To decrease water: raise `height_max`

## Advanced: Direct Tool Usage

The `terrain_tune` tool can be run directly with more options:

```bash
./terrain_tune --help

# Examples:
./terrain_tune -c my_config.cfg      # Use custom config
./terrain_tune -o my_output/         # Custom output directory
./terrain_tune --single              # Only center values
./terrain_tune --dry-run             # Preview without generating
./terrain_tune -v                    # Verbose output
```
