#include "water.h"
#include <string.h>
#include <stdlib.h>

// ============ INTERNAL HELPERS ============

// CRC32 lookup table
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d3a5c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7a85, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b82,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdede94c5, 0x47d7a5fb, 0x30d0967d,
    0x0d4ba8e8, 0x7a4c987e, 0xe345c9c4, 0x94429752, 0x0a2652f1, 0x7d216267,
    0xe4283bdd, 0x93294b4b, 0x03872c02, 0x74805c94, 0xedd3d02e, 0x9ad40fb8,
    0x043d8611, 0x73364387, 0xec3b1233, 0x9b3c22a5
};

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    crc = ~crc;
    while (len--) {
        crc = crc32_table[(crc ^ *data++) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

// ============ INITIALIZATION ============

void water_init(WaterState *water, const int terrain[WATER_RESOLUTION][WATER_RESOLUTION]) {
    memset(water, 0, sizeof(WaterState));

    // Cache terrain heights in fixed-point (using [x][z] indexing)
    for (int x = 0; x < WATER_RESOLUTION; x++) {
        for (int z = 0; z < WATER_RESOLUTION; z++) {
            water->terrain_height[x][z] = INT_TO_FIXED(terrain[x][z]);
        }
    }

    water->tick = 0;
    water->accumulator = 0.0f;
    water->total_water = 0;
    water->checksum = 0;
    water->initialized = true;
}

void water_reset(WaterState *water) {
    for (int x = 0; x < WATER_RESOLUTION; x++) {
        for (int z = 0; z < WATER_RESOLUTION; z++) {
            water->cells[x][z].water_height = 0;
        }
    }
    water->tick = 0;
    water->accumulator = 0.0f;
    water->total_water = 0;
}

// ============ SIMULATION UPDATE ============

int water_update(WaterState *water, float deltaTime) {
    if (!water->initialized) return 0;

    water->accumulator += deltaTime;

    int steps = 0;
    const float dt = WATER_UPDATE_DT;

    while (water->accumulator >= dt) {
        water_step(water);
        water->accumulator -= dt;
        steps++;

        if (steps >= 4) {
            water->accumulator = 0.0f;
            break;
        }
    }

    return steps;
}

// Mass-conserving water simulation using edge-based transfers
// Each edge between adjacent cells is processed exactly once
// What leaves one cell enters the other - guaranteed conservation
void water_step(WaterState *water) {
    // Flow rate constant (fraction of height difference to transfer per step)
    const fixed16_t FLOW_RATE = FLOAT_TO_FIXED(0.25f);

    // Minimum height difference to trigger flow (prevents jitter)
    const fixed16_t MIN_DIFF = FLOAT_TO_FIXED(0.001f);

    // Track edge drainage for map boundaries
    fixed16_t total_edge_drain = 0;

    // Debug tracking
    static int debug_counter = 0;
    fixed16_t water_before = water_calculate_total(water);

    // Temporary arrays for two-phase approach:
    // 1. Calculate proposed outflows per cell
    // 2. Scale down if total outflow exceeds available water
    fixed16_t delta[WATER_RESOLUTION][WATER_RESOLUTION];
    fixed16_t outflow[WATER_RESOLUTION][WATER_RESOLUTION];  // Track total outflow per cell
    memset(delta, 0, sizeof(delta));
    memset(outflow, 0, sizeof(outflow));

    // Structure to store edge transfers for scaling
    typedef struct {
        int x1, z1, x2, z2;
        fixed16_t transfer;  // Positive = flows from cell1 to cell2
    } EdgeTransfer;

    // Allocate space for all possible edges (horizontal + vertical)
    // Horizontal: (WATER_RESOLUTION-1) * WATER_RESOLUTION
    // Vertical: WATER_RESOLUTION * (WATER_RESOLUTION-1)
    const int max_edges = 2 * WATER_RESOLUTION * (WATER_RESOLUTION - 1);
    EdgeTransfer *edges = (EdgeTransfer *)malloc(sizeof(EdgeTransfer) * max_edges);
    int edge_count = 0;

    // ============ PHASE 1: CALCULATE PROPOSED TRANSFERS ============

    // Process horizontal edges (between x and x+1)
    for (int x = 0; x < WATER_RESOLUTION - 1; x++) {
        for (int z = 0; z < WATER_RESOLUTION; z++) {
            fixed16_t h1 = water->terrain_height[x][z] + water->cells[x][z].water_height;
            fixed16_t h2 = water->terrain_height[x+1][z] + water->cells[x+1][z].water_height;

            fixed16_t diff = h1 - h2;
            if (diff > -MIN_DIFF && diff < MIN_DIFF) continue;

            fixed16_t transfer = fixed_mul(diff, FLOW_RATE);

            // Store the edge for later scaling
            edges[edge_count].x1 = x;
            edges[edge_count].z1 = z;
            edges[edge_count].x2 = x + 1;
            edges[edge_count].z2 = z;
            edges[edge_count].transfer = transfer;
            edge_count++;

            // Track proposed outflows
            if (transfer > 0) {
                outflow[x][z] += transfer;
            } else {
                outflow[x+1][z] += -transfer;
            }
        }
    }

    // Process vertical edges (between z and z+1)
    for (int x = 0; x < WATER_RESOLUTION; x++) {
        for (int z = 0; z < WATER_RESOLUTION - 1; z++) {
            fixed16_t h1 = water->terrain_height[x][z] + water->cells[x][z].water_height;
            fixed16_t h2 = water->terrain_height[x][z+1] + water->cells[x][z+1].water_height;

            fixed16_t diff = h1 - h2;
            if (diff > -MIN_DIFF && diff < MIN_DIFF) continue;

            fixed16_t transfer = fixed_mul(diff, FLOW_RATE);

            edges[edge_count].x1 = x;
            edges[edge_count].z1 = z;
            edges[edge_count].x2 = x;
            edges[edge_count].z2 = z + 1;
            edges[edge_count].transfer = transfer;
            edge_count++;

            if (transfer > 0) {
                outflow[x][z] += transfer;
            } else {
                outflow[x][z+1] += -transfer;
            }
        }
    }

    // ============ PHASE 2: APPLY TRANSFERS WITH SCALING ============

    for (int i = 0; i < edge_count; i++) {
        int x1 = edges[i].x1, z1 = edges[i].z1;
        int x2 = edges[i].x2, z2 = edges[i].z2;
        fixed16_t transfer = edges[i].transfer;

        // Determine source cell and scale factor
        fixed16_t scaled_transfer;
        if (transfer > 0) {
            // Source is (x1, z1)
            fixed16_t available = water->cells[x1][z1].water_height;
            fixed16_t total_out = outflow[x1][z1];
            if (total_out > available && total_out > 0) {
                // Scale down proportionally
                scaled_transfer = fixed_mul(transfer, fixed_div(available, total_out));
            } else {
                scaled_transfer = transfer;
            }
        } else {
            // Source is (x2, z2)
            fixed16_t available = water->cells[x2][z2].water_height;
            fixed16_t total_out = outflow[x2][z2];
            if (total_out > available && total_out > 0) {
                // Scale down proportionally (transfer is negative)
                scaled_transfer = fixed_mul(transfer, fixed_div(available, total_out));
            } else {
                scaled_transfer = transfer;
            }
        }

        // Apply scaled transfer
        delta[x1][z1] -= scaled_transfer;
        delta[x2][z2] += scaled_transfer;
    }

    free(edges);

    // ============ PROCESS MAP EDGES (drainage) ============
    // Only cells at the actual map boundary drain water
    // Must also account for outflow scaling
    for (int x = 0; x < WATER_RESOLUTION; x++) {
        // North edge (z = 0)
        if (water->cells[x][0].water_height > 0) {
            fixed16_t available = water->cells[x][0].water_height;
            fixed16_t drain = fixed_mul(available, WATER_EDGE_DRAIN_RATE);
            // Scale if total outflow exceeds available
            fixed16_t remaining = available + delta[x][0];  // After internal transfers
            if (remaining < drain) drain = (remaining > 0) ? remaining : 0;
            delta[x][0] -= drain;
            total_edge_drain += drain;
        }
        // South edge (z = max)
        if (water->cells[x][WATER_RESOLUTION-1].water_height > 0) {
            fixed16_t available = water->cells[x][WATER_RESOLUTION-1].water_height;
            fixed16_t drain = fixed_mul(available, WATER_EDGE_DRAIN_RATE);
            fixed16_t remaining = available + delta[x][WATER_RESOLUTION-1];
            if (remaining < drain) drain = (remaining > 0) ? remaining : 0;
            delta[x][WATER_RESOLUTION-1] -= drain;
            total_edge_drain += drain;
        }
    }
    for (int z = 1; z < WATER_RESOLUTION - 1; z++) {  // Skip corners (already processed)
        // West edge (x = 0)
        if (water->cells[0][z].water_height > 0) {
            fixed16_t available = water->cells[0][z].water_height;
            fixed16_t drain = fixed_mul(available, WATER_EDGE_DRAIN_RATE);
            fixed16_t remaining = available + delta[0][z];
            if (remaining < drain) drain = (remaining > 0) ? remaining : 0;
            delta[0][z] -= drain;
            total_edge_drain += drain;
        }
        // East edge (x = max)
        if (water->cells[WATER_RESOLUTION-1][z].water_height > 0) {
            fixed16_t available = water->cells[WATER_RESOLUTION-1][z].water_height;
            fixed16_t drain = fixed_mul(available, WATER_EDGE_DRAIN_RATE);
            fixed16_t remaining = available + delta[WATER_RESOLUTION-1][z];
            if (remaining < drain) drain = (remaining > 0) ? remaining : 0;
            delta[WATER_RESOLUTION-1][z] -= drain;
            total_edge_drain += drain;
        }
    }

    // ============ APPLY ALL DELTAS ============
    for (int x = 0; x < WATER_RESOLUTION; x++) {
        for (int z = 0; z < WATER_RESOLUTION; z++) {
            water->cells[x][z].water_height += delta[x][z];

            // Safety clamp (should not be needed with proper scaling)
            if (water->cells[x][z].water_height < 0) {
                water->cells[x][z].water_height = 0;
            }
            if (water->cells[x][z].water_height > WATER_MAX_DEPTH) {
                water->cells[x][z].water_height = WATER_MAX_DEPTH;
            }
        }
    }

    water->tick++;

    // Debug: verify mass conservation (disabled by default)
#ifdef WATER_DEBUG
    fixed16_t water_after = water_calculate_total(water);
    fixed16_t actual_loss = water_before - water_after;
    fixed16_t unexplained = actual_loss - total_edge_drain;

    debug_counter++;
    if (debug_counter % 600 == 0) {  // Every 10 seconds
        TraceLog(LOG_INFO, "WATER DEBUG: before=%.4f after=%.4f edge_drain=%.4f unexplained=%.6f",
                 FIXED_TO_FLOAT(water_before),
                 FIXED_TO_FLOAT(water_after),
                 FIXED_TO_FLOAT(total_edge_drain),
                 FIXED_TO_FLOAT(unexplained));
    }
#else
    (void)water_before;
    (void)total_edge_drain;
    (void)debug_counter;
#endif
}

// ============ WATER MANIPULATION ============

void water_add(WaterState *water, int x, int z, fixed16_t amount) {
    if (!water_cell_valid(x, z)) return;
    if (amount <= 0) return;

    water->cells[x][z].water_height += amount;
    if (water->cells[x][z].water_height > WATER_MAX_DEPTH) {
        water->cells[x][z].water_height = WATER_MAX_DEPTH;
    }
}

fixed16_t water_remove(WaterState *water, int x, int z, fixed16_t amount) {
    if (!water_cell_valid(x, z)) return 0;
    if (amount <= 0) return 0;

    fixed16_t available = water->cells[x][z].water_height;
    fixed16_t removed = (amount < available) ? amount : available;

    water->cells[x][z].water_height -= removed;
    return removed;
}

void water_add_at_world(WaterState *water, float world_x, float world_z, float amount) {
    int cell_x, cell_z;
    water_world_to_cell(world_x, world_z, &cell_x, &cell_z);
    water_add(water, cell_x, cell_z, FLOAT_TO_FIXED(amount));
}

// ============ QUERIES ============

fixed16_t water_get_depth(const WaterState *water, int x, int z) {
    if (!water_cell_valid(x, z)) return 0;
    return water->cells[x][z].water_height;
}

float water_get_depth_at_world(const WaterState *water, float world_x, float world_z) {
    int cell_x, cell_z;
    water_world_to_cell(world_x, world_z, &cell_x, &cell_z);
    return FIXED_TO_FLOAT(water_get_depth(water, cell_x, cell_z));
}

fixed16_t water_get_surface_height(const WaterState *water, int x, int z) {
    if (!water_cell_valid(x, z)) return 0;
    return water->terrain_height[x][z] + water->cells[x][z].water_height;
}

// ============ CONSERVATION & SYNC ============

fixed16_t water_calculate_total(const WaterState *water) {
    fixed16_t total = 0;
    for (int x = 0; x < WATER_RESOLUTION; x++) {
        for (int z = 0; z < WATER_RESOLUTION; z++) {
            total += water->cells[x][z].water_height;
        }
    }
    return total;
}

uint32_t water_calculate_checksum(const WaterState *water) {
    uint32_t crc = 0;
    crc = crc32_update(crc, (const uint8_t *)&water->tick, sizeof(water->tick));

    for (int x = 0; x < WATER_RESOLUTION; x++) {
        for (int z = 0; z < WATER_RESOLUTION; z++) {
            crc = crc32_update(crc, (const uint8_t *)&water->cells[x][z].water_height,
                              sizeof(fixed16_t));
        }
    }

    return crc;
}
