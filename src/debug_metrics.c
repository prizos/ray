#include "debug_metrics.h"
#include <stdio.h>
#include <string.h>

// Global metrics instance
DebugMetrics g_debug_metrics = {0};

void debug_metrics_init(bool enabled, double interval) {
    memset(&g_debug_metrics, 0, sizeof(DebugMetrics));
    g_debug_metrics.enabled = enabled;
    g_debug_metrics.interval = interval > 0 ? interval : 1.0;
    g_debug_metrics.last_emit_time = 0.0;

    if (enabled) {
        printf("\n");
        printf("================================================================================\n");
        printf("DEBUG METRICS ENABLED (interval: %.1fs)\n", g_debug_metrics.interval);
        printf("================================================================================\n");
        printf("\n");
        fflush(stdout);
    }
}

void debug_metrics_reset_counters(void) {
    g_debug_metrics.nodes_allocated = 0;
    g_debug_metrics.nodes_freed = 0;
    g_debug_metrics.physics_steps = 0;
    g_debug_metrics.active_nodes_processed = 0;
    g_debug_metrics.cells_expanded = 0;
    g_debug_metrics.cells_collapsed = 0;
    g_debug_metrics.time_heat_conduction_ms = 0;
    g_debug_metrics.time_liquid_flow_ms = 0;
    g_debug_metrics.time_gas_diffusion_ms = 0;
    g_debug_metrics.time_collapse_check_ms = 0;
    g_debug_metrics.time_snapshot_ms = 0;
    g_debug_metrics.dirty_nodes_processed = 0;
    g_debug_metrics.neighbor_lookups = 0;
}

bool debug_metrics_update(double current_time) {
    if (!g_debug_metrics.enabled) {
        return false;
    }

    double elapsed = current_time - g_debug_metrics.last_emit_time;
    if (elapsed < g_debug_metrics.interval) {
        return false;
    }

    // Calculate per-second rates
    double rate_multiplier = 1.0 / elapsed;

    printf("\n");
    printf("--- DEBUG METRICS (t=%.1fs) -------------------------------------------\n", current_time);
    printf("  MEMORY:\n");
    printf("    SVO nodes:     %6llu current / %6llu peak\n",
           (unsigned long long)g_debug_metrics.svo_node_count,
           (unsigned long long)g_debug_metrics.svo_node_peak);
    printf("    Cells:         %6llu  |  Materials: %6llu\n",
           (unsigned long long)g_debug_metrics.cell_count,
           (unsigned long long)g_debug_metrics.material_entries);
    printf("    Est. memory:   %6llu KB\n",
           (unsigned long long)g_debug_metrics.estimated_memory_kb);

    printf("  OPERATIONS (per second):\n");
    printf("    Node alloc:    %6.0f  |  Node free:    %6.0f\n",
           g_debug_metrics.nodes_allocated * rate_multiplier,
           g_debug_metrics.nodes_freed * rate_multiplier);
    printf("    Physics steps: %6.0f  |  Active nodes: %6.0f\n",
           g_debug_metrics.physics_steps * rate_multiplier,
           g_debug_metrics.active_nodes_processed * rate_multiplier);
    printf("    Cells expand:  %6.0f  |  Cells collapse: %4.0f\n",
           g_debug_metrics.cells_expanded * rate_multiplier,
           g_debug_metrics.cells_collapsed * rate_multiplier);
    printf("    Dirty nodes:   %6llu  |  Neighbor lookups: %llu\n",
           (unsigned long long)g_debug_metrics.dirty_nodes_processed,
           (unsigned long long)g_debug_metrics.neighbor_lookups);
    printf("  TIMING (total ms in interval):\n");
    printf("    Heat conduct:  %7.1f  |  Liquid flow: %7.1f\n",
           g_debug_metrics.time_heat_conduction_ms,
           g_debug_metrics.time_liquid_flow_ms);
    printf("    Gas diffuse:   %7.1f  |  Collapse:    %7.1f\n",
           g_debug_metrics.time_gas_diffusion_ms,
           g_debug_metrics.time_collapse_check_ms);
    printf("    Snapshot:      %7.1f\n",
           g_debug_metrics.time_snapshot_ms);
    printf("-----------------------------------------------------------------------\n");

    // Reset counters and update timestamp
    debug_metrics_reset_counters();
    g_debug_metrics.last_emit_time = current_time;

    fflush(stdout);

    return true;
}
