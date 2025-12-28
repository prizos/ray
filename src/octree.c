#include "octree.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============ INTERNAL HELPERS ============

static OctreeNode *node_create(OctreeBounds bounds, int depth) {
    OctreeNode *node = (OctreeNode *)calloc(1, sizeof(OctreeNode));
    if (!node) return NULL;

    node->bounds = bounds;
    node->depth = depth;
    node->is_leaf = true;
    node->items = NULL;
    node->item_count = 0;
    node->item_capacity = 0;

    for (int i = 0; i < 8; i++) {
        node->children[i] = NULL;
    }

    return node;
}

static void node_destroy(OctreeNode *node) {
    if (!node) return;

    if (node->items) {
        free(node->items);
    }

    for (int i = 0; i < 8; i++) {
        if (node->children[i]) {
            node_destroy(node->children[i]);
        }
    }

    free(node);
}

static int get_child_index(const OctreeBounds *bounds, float x, float y, float z) {
    float mid_x = (bounds->min_x + bounds->max_x) * 0.5f;
    float mid_y = (bounds->min_y + bounds->max_y) * 0.5f;
    float mid_z = (bounds->min_z + bounds->max_z) * 0.5f;

    int idx = 0;
    if (x >= mid_x) idx |= 1;
    if (y >= mid_y) idx |= 2;
    if (z >= mid_z) idx |= 4;
    return idx;
}

static OctreeBounds get_child_bounds(const OctreeBounds *parent, int child_idx) {
    float mid_x = (parent->min_x + parent->max_x) * 0.5f;
    float mid_y = (parent->min_y + parent->max_y) * 0.5f;
    float mid_z = (parent->min_z + parent->max_z) * 0.5f;

    OctreeBounds child;

    if (child_idx & 1) {
        child.min_x = mid_x;
        child.max_x = parent->max_x;
    } else {
        child.min_x = parent->min_x;
        child.max_x = mid_x;
    }

    if (child_idx & 2) {
        child.min_y = mid_y;
        child.max_y = parent->max_y;
    } else {
        child.min_y = parent->min_y;
        child.max_y = mid_y;
    }

    if (child_idx & 4) {
        child.min_z = mid_z;
        child.max_z = parent->max_z;
    } else {
        child.min_z = parent->min_z;
        child.max_z = mid_z;
    }

    return child;
}

static void subdivide_node(OctreeNode *node, int *node_count) {
    for (int i = 0; i < 8; i++) {
        OctreeBounds child_bounds = get_child_bounds(&node->bounds, i);
        node->children[i] = node_create(child_bounds, node->depth + 1);
        (*node_count)++;
    }
    node->is_leaf = false;
}

static bool node_insert(OctreeNode *node, float x, float y, float z, uint32_t data,
                        int max_depth, int *node_count) {
    if (node->is_leaf) {
        // If bucket not full or max depth reached, add here
        if (node->item_count < OCTREE_BUCKET_SIZE || node->depth >= max_depth) {
            // Grow items array if needed
            if (node->item_count >= node->item_capacity) {
                int new_cap = node->item_capacity == 0 ? OCTREE_BUCKET_SIZE : node->item_capacity * 2;
                OctreeItem *new_items = (OctreeItem *)realloc(node->items, sizeof(OctreeItem) * new_cap);
                if (!new_items) return false;
                node->items = new_items;
                node->item_capacity = new_cap;
            }
            node->items[node->item_count++] = (OctreeItem){x, y, z, data};
            return true;
        }

        // Subdivide: create 8 children
        subdivide_node(node, node_count);

        // Re-insert existing items into children
        for (int i = 0; i < node->item_count; i++) {
            int child_idx = get_child_index(&node->bounds,
                                            node->items[i].x, node->items[i].y, node->items[i].z);
            node_insert(node->children[child_idx],
                       node->items[i].x, node->items[i].y, node->items[i].z,
                       node->items[i].data, max_depth, node_count);
        }
        free(node->items);
        node->items = NULL;
        node->item_count = 0;
        node->item_capacity = 0;
    }

    // Insert into appropriate child
    int child_idx = get_child_index(&node->bounds, x, y, z);
    return node_insert(node->children[child_idx], x, y, z, data, max_depth, node_count);
}

static bool node_remove(OctreeNode *node, float x, float y, float z, uint32_t data) {
    if (node->is_leaf) {
        for (int i = 0; i < node->item_count; i++) {
            if (node->items[i].data == data &&
                fabsf(node->items[i].x - x) < 0.001f &&
                fabsf(node->items[i].y - y) < 0.001f &&
                fabsf(node->items[i].z - z) < 0.001f) {
                // Remove by swapping with last
                node->items[i] = node->items[node->item_count - 1];
                node->item_count--;
                return true;
            }
        }
        return false;
    }

    int child_idx = get_child_index(&node->bounds, x, y, z);
    if (node->children[child_idx]) {
        return node_remove(node->children[child_idx], x, y, z, data);
    }
    return false;
}

static void node_query_sphere(OctreeNode *node, float cx, float cy, float cz,
                              float radius, OctreeQueryResult *result) {
    // Early exit if sphere doesn't intersect node bounds
    if (!octree_sphere_intersects_bounds(cx, cy, cz, radius, &node->bounds)) {
        return;
    }

    if (node->is_leaf) {
        float radius_sq = radius * radius;
        for (int i = 0; i < node->item_count; i++) {
            float dx = node->items[i].x - cx;
            float dy = node->items[i].y - cy;
            float dz = node->items[i].z - cz;
            float dist_sq = dx*dx + dy*dy + dz*dz;

            if (dist_sq <= radius_sq) {
                octree_result_add(result, node->items[i].data);
            }
        }
    } else {
        for (int i = 0; i < 8; i++) {
            if (node->children[i]) {
                node_query_sphere(node->children[i], cx, cy, cz, radius, result);
            }
        }
    }
}

static void node_query_range(OctreeNode *node, const OctreeBounds *range, OctreeQueryResult *result) {
    if (!octree_bounds_intersects(&node->bounds, range)) {
        return;
    }

    if (node->is_leaf) {
        for (int i = 0; i < node->item_count; i++) {
            if (octree_bounds_contains_point(range,
                                             node->items[i].x, node->items[i].y, node->items[i].z)) {
                octree_result_add(result, node->items[i].data);
            }
        }
    } else {
        for (int i = 0; i < 8; i++) {
            if (node->children[i]) {
                node_query_range(node->children[i], range, result);
            }
        }
    }
}

static void node_query_nearest(OctreeNode *node, float x, float y, float z,
                               float *best_dist_sq, uint32_t *best_data, bool *found) {
    // Prune if node's closest point is farther than best
    float closest_dist_sq = octree_bounds_point_dist_sq(&node->bounds, x, y, z);
    if (closest_dist_sq > *best_dist_sq) {
        return;
    }

    if (node->is_leaf) {
        for (int i = 0; i < node->item_count; i++) {
            float dx = node->items[i].x - x;
            float dy = node->items[i].y - y;
            float dz = node->items[i].z - z;
            float dist_sq = dx*dx + dy*dy + dz*dz;

            if (dist_sq < *best_dist_sq) {
                *best_dist_sq = dist_sq;
                *best_data = node->items[i].data;
                *found = true;
            }
        }
    } else {
        // Sort children by distance for better pruning
        int order[8];
        float child_dists[8];
        for (int i = 0; i < 8; i++) {
            order[i] = i;
            if (node->children[i]) {
                child_dists[i] = octree_bounds_point_dist_sq(&node->children[i]->bounds, x, y, z);
            } else {
                child_dists[i] = 1e30f;
            }
        }

        // Simple insertion sort (8 elements)
        for (int i = 1; i < 8; i++) {
            int j = i;
            while (j > 0 && child_dists[order[j-1]] > child_dists[order[j]]) {
                int tmp = order[j];
                order[j] = order[j-1];
                order[j-1] = tmp;
                j--;
            }
        }

        for (int i = 0; i < 8; i++) {
            int child_idx = order[i];
            if (node->children[child_idx]) {
                node_query_nearest(node->children[child_idx], x, y, z,
                                  best_dist_sq, best_data, found);
            }
        }
    }
}

static int frustum_aabb_intersect(const OctreeFrustum *f, const OctreeBounds *b) {
    bool all_inside = true;

    for (int p = 0; p < 6; p++) {
        const Vector4 *plane = &f->planes[p];

        // Find the corner most in the direction of the plane normal (p-vertex)
        float px = (plane->x > 0) ? b->max_x : b->min_x;
        float py = (plane->y > 0) ? b->max_y : b->min_y;
        float pz = (plane->z > 0) ? b->max_z : b->min_z;

        // If p-vertex is outside, box is completely outside
        if (plane->x * px + plane->y * py + plane->z * pz + plane->w < 0) {
            return FRUSTUM_OUTSIDE;
        }

        // Find the corner most opposite to plane normal (n-vertex)
        float nx = (plane->x > 0) ? b->min_x : b->max_x;
        float ny = (plane->y > 0) ? b->min_y : b->max_y;
        float nz = (plane->z > 0) ? b->min_z : b->max_z;

        // If n-vertex is outside, box intersects this plane
        if (plane->x * nx + plane->y * ny + plane->z * nz + plane->w < 0) {
            all_inside = false;
        }
    }

    return all_inside ? FRUSTUM_INSIDE : FRUSTUM_INTERSECT;
}

static void node_add_all(OctreeNode *node, OctreeQueryResult *result) {
    if (node->is_leaf) {
        for (int i = 0; i < node->item_count; i++) {
            octree_result_add(result, node->items[i].data);
        }
    } else {
        for (int i = 0; i < 8; i++) {
            if (node->children[i]) {
                node_add_all(node->children[i], result);
            }
        }
    }
}

static void node_query_frustum(OctreeNode *node, const OctreeFrustum *frustum,
                               OctreeQueryResult *result) {
    int intersect = frustum_aabb_intersect(frustum, &node->bounds);

    if (intersect == FRUSTUM_OUTSIDE) {
        return;
    }

    if (intersect == FRUSTUM_INSIDE) {
        // Completely inside, add all items
        node_add_all(node, result);
    } else if (node->is_leaf) {
        // Intersecting leaf, add all (could test individual points but usually not worth it)
        for (int i = 0; i < node->item_count; i++) {
            octree_result_add(result, node->items[i].data);
        }
    } else {
        // Intersecting internal node, recurse
        for (int i = 0; i < 8; i++) {
            if (node->children[i]) {
                node_query_frustum(node->children[i], frustum, result);
            }
        }
    }
}

// ============ PUBLIC API ============

Octree *octree_create(OctreeBounds bounds, int max_depth) {
    Octree *tree = (Octree *)calloc(1, sizeof(Octree));
    if (!tree) return NULL;

    tree->world_bounds = bounds;
    tree->max_depth = max_depth > OCTREE_MAX_DEPTH ? OCTREE_MAX_DEPTH : max_depth;
    tree->root = node_create(bounds, 0);
    tree->node_count = 1;
    tree->total_items = 0;

    if (!tree->root) {
        free(tree);
        return NULL;
    }

    return tree;
}

void octree_destroy(Octree *tree) {
    if (!tree) return;
    node_destroy(tree->root);
    free(tree);
}

void octree_clear(Octree *tree) {
    if (!tree) return;
    node_destroy(tree->root);
    tree->root = node_create(tree->world_bounds, 0);
    tree->node_count = 1;
    tree->total_items = 0;
}

bool octree_insert(Octree *tree, float x, float y, float z, uint32_t data) {
    if (!tree || !tree->root) return false;

    if (!octree_bounds_contains_point(&tree->world_bounds, x, y, z)) {
        return false;
    }

    if (node_insert(tree->root, x, y, z, data, tree->max_depth, &tree->node_count)) {
        tree->total_items++;
        return true;
    }
    return false;
}

bool octree_remove(Octree *tree, float x, float y, float z, uint32_t data) {
    if (!tree || !tree->root) return false;

    if (node_remove(tree->root, x, y, z, data)) {
        tree->total_items--;
        return true;
    }
    return false;
}

bool octree_update(Octree *tree, float old_x, float old_y, float old_z,
                   float new_x, float new_y, float new_z, uint32_t data) {
    if (!tree) return false;

    if (octree_remove(tree, old_x, old_y, old_z, data)) {
        return octree_insert(tree, new_x, new_y, new_z, data);
    }
    return false;
}

void octree_query_range(Octree *tree, OctreeBounds range, OctreeQueryResult *result) {
    if (!tree || !tree->root || !result) return;
    node_query_range(tree->root, &range, result);
}

void octree_query_sphere(Octree *tree, float cx, float cy, float cz, float radius,
                         OctreeQueryResult *result) {
    if (!tree || !tree->root || !result) return;
    node_query_sphere(tree->root, cx, cy, cz, radius, result);
}

bool octree_query_nearest(Octree *tree, float x, float y, float z,
                          float max_dist, uint32_t *out_data, float *out_dist) {
    if (!tree || !tree->root) return false;

    float best_dist_sq = max_dist * max_dist;
    uint32_t best_data = 0;
    bool found = false;

    node_query_nearest(tree->root, x, y, z, &best_dist_sq, &best_data, &found);

    if (found) {
        if (out_data) *out_data = best_data;
        if (out_dist) *out_dist = sqrtf(best_dist_sq);
    }
    return found;
}

void octree_query_frustum(Octree *tree, OctreeFrustum *frustum, OctreeQueryResult *result) {
    if (!tree || !tree->root || !frustum || !result) return;
    node_query_frustum(tree->root, frustum, result);
}

void octree_query_all(Octree *tree, OctreeQueryResult *result) {
    if (!tree || !tree->root || !result) return;
    node_add_all(tree->root, result);
}

// ============ QUERY RESULT MANAGEMENT ============

OctreeQueryResult *octree_result_create(int initial_capacity) {
    OctreeQueryResult *result = (OctreeQueryResult *)calloc(1, sizeof(OctreeQueryResult));
    if (!result) return NULL;

    if (initial_capacity > 0) {
        result->indices = (uint32_t *)malloc(sizeof(uint32_t) * initial_capacity);
        if (!result->indices) {
            free(result);
            return NULL;
        }
        result->capacity = initial_capacity;
    }
    result->count = 0;

    return result;
}

void octree_result_clear(OctreeQueryResult *result) {
    if (result) {
        result->count = 0;
    }
}

void octree_result_destroy(OctreeQueryResult *result) {
    if (!result) return;
    if (result->indices) {
        free(result->indices);
    }
    free(result);
}

void octree_result_add(OctreeQueryResult *result, uint32_t data) {
    if (!result) return;

    if (result->count >= result->capacity) {
        int new_cap = result->capacity == 0 ? 64 : result->capacity * 2;
        uint32_t *new_indices = (uint32_t *)realloc(result->indices, sizeof(uint32_t) * new_cap);
        if (!new_indices) return;
        result->indices = new_indices;
        result->capacity = new_cap;
    }

    result->indices[result->count++] = data;
}

// ============ BOUNDS HELPERS ============

bool octree_bounds_contains_point(const OctreeBounds *bounds, float x, float y, float z) {
    return x >= bounds->min_x && x <= bounds->max_x &&
           y >= bounds->min_y && y <= bounds->max_y &&
           z >= bounds->min_z && z <= bounds->max_z;
}

bool octree_bounds_intersects(const OctreeBounds *a, const OctreeBounds *b) {
    return a->min_x <= b->max_x && a->max_x >= b->min_x &&
           a->min_y <= b->max_y && a->max_y >= b->min_y &&
           a->min_z <= b->max_z && a->max_z >= b->min_z;
}

bool octree_sphere_intersects_bounds(float cx, float cy, float cz, float radius,
                                      const OctreeBounds *bounds) {
    float dist_sq = octree_bounds_point_dist_sq(bounds, cx, cy, cz);
    return dist_sq <= radius * radius;
}

float octree_bounds_point_dist_sq(const OctreeBounds *bounds, float x, float y, float z) {
    float dx = 0, dy = 0, dz = 0;

    if (x < bounds->min_x) dx = bounds->min_x - x;
    else if (x > bounds->max_x) dx = x - bounds->max_x;

    if (y < bounds->min_y) dy = bounds->min_y - y;
    else if (y > bounds->max_y) dy = y - bounds->max_y;

    if (z < bounds->min_z) dz = bounds->min_z - z;
    else if (z > bounds->max_z) dz = z - bounds->max_z;

    return dx*dx + dy*dy + dz*dz;
}

// ============ FRUSTUM HELPERS ============

void octree_frustum_from_camera(Camera3D *camera, float aspect, float near, float far,
                                 OctreeFrustum *frustum) {
    if (!camera || !frustum) return;

    // Get view and projection matrices
    Matrix view = MatrixLookAt(camera->position, camera->target, camera->up);
    Matrix proj = MatrixPerspective(camera->fovy * DEG2RAD, aspect, near, far);
    Matrix vp = MatrixMultiply(view, proj);

    // Extract frustum planes from view-projection matrix
    // Left plane
    frustum->planes[0] = (Vector4){
        vp.m3 + vp.m0,
        vp.m7 + vp.m4,
        vp.m11 + vp.m8,
        vp.m15 + vp.m12
    };

    // Right plane
    frustum->planes[1] = (Vector4){
        vp.m3 - vp.m0,
        vp.m7 - vp.m4,
        vp.m11 - vp.m8,
        vp.m15 - vp.m12
    };

    // Bottom plane
    frustum->planes[2] = (Vector4){
        vp.m3 + vp.m1,
        vp.m7 + vp.m5,
        vp.m11 + vp.m9,
        vp.m15 + vp.m13
    };

    // Top plane
    frustum->planes[3] = (Vector4){
        vp.m3 - vp.m1,
        vp.m7 - vp.m5,
        vp.m11 - vp.m9,
        vp.m15 - vp.m13
    };

    // Near plane
    frustum->planes[4] = (Vector4){
        vp.m3 + vp.m2,
        vp.m7 + vp.m6,
        vp.m11 + vp.m10,
        vp.m15 + vp.m14
    };

    // Far plane
    frustum->planes[5] = (Vector4){
        vp.m3 - vp.m2,
        vp.m7 - vp.m6,
        vp.m11 - vp.m10,
        vp.m15 - vp.m14
    };

    // Normalize all planes
    for (int i = 0; i < 6; i++) {
        float len = sqrtf(frustum->planes[i].x * frustum->planes[i].x +
                         frustum->planes[i].y * frustum->planes[i].y +
                         frustum->planes[i].z * frustum->planes[i].z);
        if (len > 0.0001f) {
            frustum->planes[i].x /= len;
            frustum->planes[i].y /= len;
            frustum->planes[i].z /= len;
            frustum->planes[i].w /= len;
        }
    }
}

// ============ DEBUG/STATS ============

static void count_nodes(OctreeNode *node, int *internal, int *leaf, int *items) {
    if (!node) return;

    if (node->is_leaf) {
        (*leaf)++;
        (*items) += node->item_count;
    } else {
        (*internal)++;
        for (int i = 0; i < 8; i++) {
            count_nodes(node->children[i], internal, leaf, items);
        }
    }
}

void octree_print_stats(Octree *tree) {
    if (!tree) {
        printf("Octree: NULL\n");
        return;
    }

    int internal = 0, leaf = 0, items = 0;
    count_nodes(tree->root, &internal, &leaf, &items);

    printf("Octree Stats:\n");
    printf("  World bounds: (%.1f,%.1f,%.1f) - (%.1f,%.1f,%.1f)\n",
           tree->world_bounds.min_x, tree->world_bounds.min_y, tree->world_bounds.min_z,
           tree->world_bounds.max_x, tree->world_bounds.max_y, tree->world_bounds.max_z);
    printf("  Max depth: %d\n", tree->max_depth);
    printf("  Internal nodes: %d\n", internal);
    printf("  Leaf nodes: %d\n", leaf);
    printf("  Total nodes: %d\n", tree->node_count);
    printf("  Total items: %d (counted: %d)\n", tree->total_items, items);
    printf("  Avg items/leaf: %.2f\n", leaf > 0 ? (float)items / leaf : 0.0f);
}

int octree_get_node_count(Octree *tree) {
    return tree ? tree->node_count : 0;
}

int octree_get_item_count(Octree *tree) {
    return tree ? tree->total_items : 0;
}
