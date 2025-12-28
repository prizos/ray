#ifndef OCTREE_H
#define OCTREE_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

// ============ OCTREE CONSTANTS ============

#define OCTREE_MAX_DEPTH 8           // 2^8 = 256 subdivisions per axis
#define OCTREE_BUCKET_SIZE 8         // Items per leaf before subdivision
#define OCTREE_CHILD_COUNT 8         // Octree has 8 children

// Child index encoding: xyz bits (0=neg, 1=pos)
// 0: ---  1: +--  2: -+-  3: ++-
// 4: --+  5: +-+  6: -++  7: +++

// ============ BOUNDING BOX ============

typedef struct {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
} OctreeBounds;

// ============ OCTREE ITEM ============

typedef struct {
    float x, y, z;              // Position
    uint32_t data;              // Generic payload (index into external array)
} OctreeItem;

// ============ OCTREE NODE ============

typedef struct OctreeNode OctreeNode;

struct OctreeNode {
    OctreeBounds bounds;
    OctreeNode *children[8];    // NULL if leaf
    OctreeItem *items;          // Only for leaf nodes
    uint16_t item_count;
    uint16_t item_capacity;
    uint8_t depth;
    bool is_leaf;
};

// ============ OCTREE ROOT ============

typedef struct {
    OctreeNode *root;
    OctreeBounds world_bounds;
    int total_items;
    int node_count;
    int max_depth;
} Octree;

// ============ QUERY RESULTS ============

typedef struct {
    uint32_t *indices;          // Array of data indices from OctreeItem.data
    int count;
    int capacity;
} OctreeQueryResult;

// ============ FRUSTUM FOR CULLING ============

typedef struct {
    Vector4 planes[6];          // 6 frustum planes (left, right, top, bottom, near, far)
} OctreeFrustum;

// Frustum intersection results
#define FRUSTUM_OUTSIDE 0
#define FRUSTUM_INTERSECT 1
#define FRUSTUM_INSIDE 2

// ============ OCTREE API ============

// Creation/destruction
Octree *octree_create(OctreeBounds bounds, int max_depth);
void octree_destroy(Octree *tree);
void octree_clear(Octree *tree);

// Insertion/removal
bool octree_insert(Octree *tree, float x, float y, float z, uint32_t data);
bool octree_remove(Octree *tree, float x, float y, float z, uint32_t data);
bool octree_update(Octree *tree, float old_x, float old_y, float old_z,
                   float new_x, float new_y, float new_z, uint32_t data);

// Queries
void octree_query_range(Octree *tree, OctreeBounds range, OctreeQueryResult *result);
void octree_query_sphere(Octree *tree, float cx, float cy, float cz, float radius,
                         OctreeQueryResult *result);
bool octree_query_nearest(Octree *tree, float x, float y, float z,
                          float max_dist, uint32_t *out_data, float *out_dist);
void octree_query_frustum(Octree *tree, OctreeFrustum *frustum, OctreeQueryResult *result);

// Query all items (for iteration)
void octree_query_all(Octree *tree, OctreeQueryResult *result);

// Query result management
OctreeQueryResult *octree_result_create(int initial_capacity);
void octree_result_clear(OctreeQueryResult *result);
void octree_result_destroy(OctreeQueryResult *result);
void octree_result_add(OctreeQueryResult *result, uint32_t data);

// Frustum extraction from camera
void octree_frustum_from_camera(Camera3D *camera, float aspect, float near, float far,
                                 OctreeFrustum *frustum);

// Bounds helpers
bool octree_bounds_contains_point(const OctreeBounds *bounds, float x, float y, float z);
bool octree_bounds_intersects(const OctreeBounds *a, const OctreeBounds *b);
bool octree_sphere_intersects_bounds(float cx, float cy, float cz, float radius,
                                      const OctreeBounds *bounds);
float octree_bounds_point_dist_sq(const OctreeBounds *bounds, float x, float y, float z);

// Debug/stats
void octree_print_stats(Octree *tree);
int octree_get_node_count(Octree *tree);
int octree_get_item_count(Octree *tree);

#endif // OCTREE_H
