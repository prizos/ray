#ifndef DEBUG_METRICS_H
#define DEBUG_METRICS_H

#include <stdbool.h>
#include <stdint.h>

// ============ DEBUG METRICS SYSTEM ============
// Tracks performance and memory metrics for the simulation.
// Enabled via compile-time flag: -DDEBUG_METRICS
// Usage: make run DEBUG=true

// ============ METRIC COUNTERS ============

typedef struct {
    // Memory metrics (snapshot)
    uint64_t svo_node_count;        // Current SVO nodes allocated
    uint64_t svo_node_peak;         // Peak SVO nodes
    uint64_t cell_count;            // Cells with materials
    uint64_t material_entries;      // Total material entries across all cells
    uint64_t estimated_memory_kb;   // Rough memory estimate in KB

    // Per-second counters (reset each second)
    uint64_t nodes_allocated;       // Nodes allocated this second
    uint64_t nodes_freed;           // Nodes freed this second
    uint64_t physics_steps;         // Physics steps executed
    uint64_t active_nodes_processed;// Active nodes processed in physics
    uint64_t cells_expanded;        // Uniform nodes expanded to branches
    uint64_t cells_collapsed;       // Branch nodes collapsed to uniform

    // Phase timing (accumulated per interval, in milliseconds)
    double time_heat_conduction_ms;
    double time_liquid_flow_ms;
    double time_gas_diffusion_ms;
    double time_collapse_check_ms;
    double time_snapshot_ms;        // malloc/memcpy/free overhead

    // Additional counters
    uint64_t dirty_nodes_processed;
    uint64_t neighbor_lookups;

    // Timing
    double last_emit_time;          // Last time metrics were emitted
    double interval;                // Emit interval in seconds

    // State
    bool enabled;                   // Whether metrics are enabled
} DebugMetrics;

// Global metrics instance
extern DebugMetrics g_debug_metrics;

// ============ API FUNCTIONS ============

// Initialize the debug metrics system
void debug_metrics_init(bool enabled, double interval);

// Reset per-second counters (called after emit)
void debug_metrics_reset_counters(void);

// Emit metrics to console if interval has elapsed
// Returns true if metrics were emitted
bool debug_metrics_update(double current_time);

// ============ TRACKING MACROS ============
// These are no-ops when DEBUG_METRICS is not defined

#ifdef DEBUG_METRICS

#define DEBUG_METRICS_NODE_ALLOC() do { \
    g_debug_metrics.nodes_allocated++; \
    g_debug_metrics.svo_node_count++; \
    if (g_debug_metrics.svo_node_count > g_debug_metrics.svo_node_peak) { \
        g_debug_metrics.svo_node_peak = g_debug_metrics.svo_node_count; \
    } \
} while(0)

#define DEBUG_METRICS_NODE_FREE() do { \
    g_debug_metrics.nodes_freed++; \
    if (g_debug_metrics.svo_node_count > 0) g_debug_metrics.svo_node_count--; \
} while(0)

#define DEBUG_METRICS_PHYSICS_STEP() do { \
    g_debug_metrics.physics_steps++; \
} while(0)

#define DEBUG_METRICS_ACTIVE_NODE() do { \
    g_debug_metrics.active_nodes_processed++; \
} while(0)

#define DEBUG_METRICS_CELL_EXPAND() do { \
    g_debug_metrics.cells_expanded++; \
} while(0)

#define DEBUG_METRICS_CELL_COLLAPSE() do { \
    g_debug_metrics.cells_collapsed++; \
} while(0)

// Update cell/material counts (called periodically, not per-operation)
#define DEBUG_METRICS_UPDATE_MEMORY(cells, materials, mem_kb) do { \
    g_debug_metrics.cell_count = (cells); \
    g_debug_metrics.material_entries = (materials); \
    g_debug_metrics.estimated_memory_kb = (mem_kb); \
} while(0)

#else

// No-op versions when debug metrics disabled
#define DEBUG_METRICS_NODE_ALLOC()
#define DEBUG_METRICS_NODE_FREE()
#define DEBUG_METRICS_PHYSICS_STEP()
#define DEBUG_METRICS_ACTIVE_NODE()
#define DEBUG_METRICS_CELL_EXPAND()
#define DEBUG_METRICS_CELL_COLLAPSE()
#define DEBUG_METRICS_UPDATE_MEMORY(cells, materials, mem_kb)

#endif // DEBUG_METRICS

#endif // DEBUG_METRICS_H
