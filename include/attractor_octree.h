#ifndef ATTRACTOR_OCTREE_H
#define ATTRACTOR_OCTREE_H

#include "octree.h"
#include "tree.h"

// ============ ATTRACTOR OCTREE ============

// Wrapper around generic octree for attractor-specific operations
typedef struct {
    Octree *octree;
    Attractor *attractors;      // External attractor array (owned by Tree)
    int attractor_count;
} AttractorOctree;

// ============ API ============

// Create attractor octree from existing attractor array
AttractorOctree *attractor_octree_create(Attractor *attractors, int count, OctreeBounds bounds);

// Destroy attractor octree (does NOT free the attractors array)
void attractor_octree_destroy(AttractorOctree *ao);

// Rebuild octree from scratch (call after bulk attractor changes)
void attractor_octree_rebuild(AttractorOctree *ao);

// Query attractors within influence radius
// Returns indices into the attractors array
void attractor_octree_query_influence(AttractorOctree *ao, float x, float y, float z,
                                       float influence_radius, OctreeQueryResult *result);

// Find nearest ACTIVE attractor within max_dist
// Returns true if found, with index and distance
bool attractor_octree_find_nearest_active(AttractorOctree *ao, float x, float y, float z,
                                           float max_dist, int *out_idx, float *out_dist);

// Remove attractor from octree (marks as inactive and removes from tree)
void attractor_octree_deactivate(AttractorOctree *ao, int attractor_idx);

// Get count of active attractors (for stats)
int attractor_octree_count_active(AttractorOctree *ao);

#endif // ATTRACTOR_OCTREE_H
