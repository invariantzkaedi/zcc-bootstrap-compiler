#ifndef ZCC_VOXEL_SHRINKWRAP_H
#define ZCC_VOXEL_SHRINKWRAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    float*    verts;
    uint32_t* indices;
    size_t    vcount;
    size_t    icount;
} ZccGhostMesh;

int zcc_voxel_shrinkwrap(
    const float*    high_verts,   size_t vcount,
    const uint32_t* indices,      size_t icount,
    int             voxel_res,          // 32–64 recommended for <20k tris
    ZccGhostMesh*   out_ghost
);

void zcc_free_ghost_mesh(ZccGhostMesh* m);

#endif
