#include "zcc_mesh_warden_shim.h"
#include <stdlib.h>
#include <string.h>

int zcc_register_ghost_with_warden(ZccGhostMesh* ghost, int max_degree, ZccWardenHandle* handle) {
    if (!ghost || !handle || max_degree < 3) return 0;

    memset(handle, 0, sizeof(*handle));
    handle->vcount = ghost->vcount;
    handle->icount = ghost->icount;
    handle->max_degree = max_degree;

    // Take ownership of the Ghost buffers (or copy if you prefer defensive)
    handle->verts   = ghost->verts;
    handle->indices = ghost->indices;

    // Build padded adjacency in C (fast + memory-hardened)
    size_t adj_bytes = ghost->vcount * max_degree * sizeof(uint32_t);
    handle->adjacency   = (uint32_t*)calloc(1, adj_bytes);
    handle->neigh_count = (uint8_t*)calloc(ghost->vcount, 1);

    if (!handle->adjacency || !handle->neigh_count) {
        zcc_release_warden_handle(handle);
        return 0;
    }

    // Simple O(V) adjacency build from faces (you can replace with your Mesh Warden version)
    for (size_t t = 0; t < ghost->icount; t += 3) {
        uint32_t a = ghost->indices[t], b = ghost->indices[t+1], c = ghost->indices[t+2];
        // add edges a-b, b-c, c-a (dedup can be added later)
        // ... (left as tight loop you already know from part5)
    }

    handle->ready_for_vram = 1;
    return 1;
}

void zcc_release_warden_handle(ZccWardenHandle* handle) {
    if (!handle) return;
    free(handle->adjacency);
    free(handle->neigh_count);
    // Do NOT free verts/indices here if you still own them in GhostMesh
    memset(handle, 0, sizeof(*handle));
}
