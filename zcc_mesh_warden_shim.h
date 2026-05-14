#ifndef ZCC_MESH_WARDEN_SHIM_H
#define ZCC_MESH_WARDEN_SHIM_H

#include "zcc_voxel_shrinkwrap.h"
#include <stdint.h>

typedef struct {
    float*    verts;      // device-ready (or host pinned)
    uint32_t* indices;
    uint32_t* adjacency;  // padded [V][MAX_DEGREE]
    uint8_t*  neigh_count;
    size_t    vcount;
    size_t    icount;
    int       max_degree;
    int       ready_for_vram;
} ZccWardenHandle;

int zcc_register_ghost_with_warden(ZccGhostMesh* ghost, int max_degree, ZccWardenHandle* handle);
void zcc_release_warden_handle(ZccWardenHandle* handle);

#endif
