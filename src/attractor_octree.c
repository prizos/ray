#include "attractor_octree.h"
#include <stdlib.h>
#include <math.h>

AttractorOctree *attractor_octree_create(Attractor *attractors, int count, OctreeBounds bounds) {
    AttractorOctree *ao = (AttractorOctree *)calloc(1, sizeof(AttractorOctree));
    if (!ao) return NULL;

    ao->attractors = attractors;
    ao->attractor_count = count;
    ao->octree = octree_create(bounds, 6);  // Max depth 6 for attractors

    if (!ao->octree) {
        free(ao);
        return NULL;
    }

    // Insert all active attractors
    for (int i = 0; i < count; i++) {
        if (attractors[i].active) {
            octree_insert(ao->octree, attractors[i].x, attractors[i].y, attractors[i].z, i);
        }
    }

    return ao;
}

void attractor_octree_destroy(AttractorOctree *ao) {
    if (!ao) return;
    if (ao->octree) {
        octree_destroy(ao->octree);
    }
    free(ao);
}

void attractor_octree_rebuild(AttractorOctree *ao) {
    if (!ao || !ao->octree) return;

    octree_clear(ao->octree);

    for (int i = 0; i < ao->attractor_count; i++) {
        if (ao->attractors[i].active) {
            octree_insert(ao->octree, ao->attractors[i].x, ao->attractors[i].y,
                         ao->attractors[i].z, i);
        }
    }
}

void attractor_octree_query_influence(AttractorOctree *ao, float x, float y, float z,
                                       float influence_radius, OctreeQueryResult *result) {
    if (!ao || !result) return;
    octree_query_sphere(ao->octree, x, y, z, influence_radius, result);
}

bool attractor_octree_find_nearest_active(AttractorOctree *ao, float x, float y, float z,
                                           float max_dist, int *out_idx, float *out_dist) {
    if (!ao || !ao->octree) return false;

    // Query all attractors within max_dist
    OctreeQueryResult *result = octree_result_create(64);
    octree_query_sphere(ao->octree, x, y, z, max_dist, result);

    float best_dist_sq = max_dist * max_dist;
    int best_idx = -1;
    bool found = false;

    for (int i = 0; i < result->count; i++) {
        int idx = result->indices[i];
        Attractor *attr = &ao->attractors[idx];

        // Skip inactive attractors
        if (!attr->active) continue;

        float dx = attr->x - x;
        float dy = attr->y - y;
        float dz = attr->z - z;
        float dist_sq = dx*dx + dy*dy + dz*dz;

        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_idx = idx;
            found = true;
        }
    }

    octree_result_destroy(result);

    if (found) {
        if (out_idx) *out_idx = best_idx;
        if (out_dist) *out_dist = sqrtf(best_dist_sq);
    }

    return found;
}

void attractor_octree_deactivate(AttractorOctree *ao, int attractor_idx) {
    if (!ao || attractor_idx < 0 || attractor_idx >= ao->attractor_count) return;

    Attractor *attr = &ao->attractors[attractor_idx];
    if (!attr->active) return;

    // Remove from octree
    octree_remove(ao->octree, attr->x, attr->y, attr->z, attractor_idx);

    // Mark as inactive
    attr->active = false;
}

int attractor_octree_count_active(AttractorOctree *ao) {
    if (!ao) return 0;

    int count = 0;
    for (int i = 0; i < ao->attractor_count; i++) {
        if (ao->attractors[i].active) count++;
    }
    return count;
}
