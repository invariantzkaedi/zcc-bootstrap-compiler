# g:\zccMAIN\zcc\zkaedi_gpu_spatial_hash_full.py
"""
ZKAEDI Fixed-Buffer Spatial Hash (Option A)
- Fixed capacity per voxel (64 triangles max)
- Single-pass build with bounded atomics
- No prefix sum / compacted lists
- Adaptive grid resolution + distance-to-center prefilter
- Predictable memory & execution
"""

import torch
import triton
import triton.language as tl
from typing import Tuple

MAX_TRIS_PER_VOXEL: int = 64


# ====================== ADAPTIVE GRID ======================

def compute_adaptive_grid_resolution(
    ghost_verts: torch.Tensor,
    target_voxel_size: float = 0.05,
    min_res: int = 24,
    max_res: int = 64,
) -> int:
    mins = ghost_verts.min(dim=0).values
    maxs = ghost_verts.max(dim=0).values
    diagonal = torch.norm(maxs - mins).item()
    if diagonal < 1e-6:
        return min_res
    res = int(diagonal / target_voxel_size)
    return max(min_res, min(max_res, res))


# ====================== KERNEL: Build Fixed Buffer ======================

@triton.jit
def build_fixed_voxel_buffer_kernel(
    faces_ptr,
    ghost_verts_ptr,
    grid_min_ptr,
    grid_inv_size_ptr,
    grid_res: tl.constexpr,
    voxel_triangle_buffer_ptr,   # [grid_size, MAX_TRIS_PER_VOXEL]
    voxel_counts_ptr,            # [grid_size]
    F: tl.constexpr,
    MAX_TRIS: tl.constexpr,
):
    pid = tl.program_id(0)
    if pid >= F:
        return

    # Load triangle
    i0 = tl.load(faces_ptr + pid * 3 + 0)
    i1 = tl.load(faces_ptr + pid * 3 + 1)
    i2 = tl.load(faces_ptr + pid * 3 + 2)

    ax = tl.load(ghost_verts_ptr + i0 * 3 + 0)
    ay = tl.load(ghost_verts_ptr + i0 * 3 + 1)
    az = tl.load(ghost_verts_ptr + i0 * 3 + 2)

    bx = tl.load(ghost_verts_ptr + i1 * 3 + 0)
    by = tl.load(ghost_verts_ptr + i1 * 3 + 1)
    bz = tl.load(ghost_verts_ptr + i1 * 3 + 2)

    cx = tl.load(ghost_verts_ptr + i2 * 3 + 0)
    cy = tl.load(ghost_verts_ptr + i2 * 3 + 1)
    cz = tl.load(ghost_verts_ptr + i2 * 3 + 2)

    # Compute voxel AABB
    minx = tl.minimum(tl.minimum(ax, bx), cx)
    maxx = tl.maximum(tl.maximum(ax, bx), cx)
    miny = tl.minimum(tl.minimum(ay, by), cy)
    maxy = tl.maximum(tl.maximum(ay, by), cy)
    minz = tl.minimum(tl.minimum(az, bz), cz)
    maxz = tl.maximum(tl.maximum(az, bz), cz)

    vx0 = tl.maximum(0, tl.cast((minx - tl.load(grid_min_ptr + 0)) * tl.load(grid_inv_size_ptr + 0), tl.int32))
    vx1 = tl.minimum(grid_res - 1, tl.cast((maxx - tl.load(grid_min_ptr + 0)) * tl.load(grid_inv_size_ptr + 0) + 1, tl.int32))

    vy0 = tl.maximum(0, tl.cast((miny - tl.load(grid_min_ptr + 1)) * tl.load(grid_inv_size_ptr + 1), tl.int32))
    vy1 = tl.minimum(grid_res - 1, tl.cast((maxy - tl.load(grid_min_ptr + 1)) * tl.load(grid_inv_size_ptr + 1) + 1, tl.int32))

    vz0 = tl.maximum(0, tl.cast((minz - tl.load(grid_min_ptr + 2)) * tl.load(grid_inv_size_ptr + 2), tl.int32))
    vz1 = tl.minimum(grid_res - 1, tl.cast((maxz - tl.load(grid_min_ptr + 2)) * tl.load(grid_inv_size_ptr + 2) + 1, tl.int32))

    for z in range(vz0, vz1 + 1):
        for y in range(vy0, vy1 + 1):
            for x in range(vx0, vx1 + 1):
                vidx = (z * grid_res + y) * grid_res + x

                # Bounded atomic write
                current_count = tl.atomic_add(voxel_counts_ptr + vidx, 1)
                if current_count < MAX_TRIS:
                    tl.store(voxel_triangle_buffer_ptr + vidx * MAX_TRIS + current_count, pid)


# ====================== KERNEL: Query ======================

@triton.jit
def query_fixed_buffer_kernel(
    high_verts_ptr,
    ghost_verts_ptr,
    triangle_centers_ptr,
    faces_ptr,
    grid_min_ptr,
    grid_inv_size_ptr,
    grid_res: tl.constexpr,
    voxel_triangle_buffer_ptr,
    voxel_counts_ptr,
    out_tri_idx_ptr,
    out_uvw_ptr,
    N: tl.constexpr,
    voxel_size: tl.constexpr,
    MAX_TRIS: tl.constexpr,
):
    pid = tl.program_id(0)
    if pid >= N:
        return

    px = tl.load(high_verts_ptr + pid * 3 + 0)
    py = tl.load(high_verts_ptr + pid * 3 + 1)
    pz = tl.load(high_verts_ptr + pid * 3 + 2)

    vx = tl.cast((px - tl.load(grid_min_ptr + 0)) * tl.load(grid_inv_size_ptr + 0), tl.int32)
    vy = tl.cast((py - tl.load(grid_min_ptr + 1)) * tl.load(grid_inv_size_ptr + 1), tl.int32)
    vz = tl.cast((pz - tl.load(grid_min_ptr + 2)) * tl.load(grid_inv_size_ptr + 2), tl.int32)

    best_dist = 1e30
    best_tri = -1
    best_u = 0.0
    best_v = 0.0
    best_w = 0.0

    culling_threshold_sq = (voxel_size * 1.8) ** 2

    for dz in range(-1, 2):
        for dy in range(-1, 2):
            for dx in range(-1, 2):
                nx = vx + dx
                ny = vy + dy
                nz = vz + dz
                if nx >= 0 and ny >= 0 and nz >= 0 and nx < grid_res and ny < grid_res and nz < grid_res:
                    vidx = (nz * grid_res + ny) * grid_res + nx
                    count = tl.load(voxel_counts_ptr + vidx)

                    for i in range(tl.minimum(count, MAX_TRIS)):
                        tri_id = tl.load(voxel_triangle_buffer_ptr + vidx * MAX_TRIS + i)

                        # Distance-to-Center Prefilter
                        cx = tl.load(triangle_centers_ptr + tri_id * 3 + 0)
                        cy = tl.load(triangle_centers_ptr + tri_id * 3 + 1)
                        cz = tl.load(triangle_centers_ptr + tri_id * 3 + 2)

                        dx_ = px - cx
                        dy_ = py - cy
                        dz_ = pz - cz
                        if (dx_ * dx_ + dy_ * dy_ + dz_ * dz_) <= culling_threshold_sq:
                            # Load triangle and compute barycentric (simplified but functional)
                            i0 = tl.load(faces_ptr + tri_id * 3 + 0)
                            i1 = tl.load(faces_ptr + tri_id * 3 + 1)
                            i2 = tl.load(faces_ptr + tri_id * 3 + 2)

                            ax = tl.load(ghost_verts_ptr + i0 * 3 + 0)
                            ay = tl.load(ghost_verts_ptr + i0 * 3 + 1)
                            az = tl.load(ghost_verts_ptr + i0 * 3 + 2)

                            bx = tl.load(ghost_verts_ptr + i1 * 3 + 0)
                            by = tl.load(ghost_verts_ptr + i1 * 3 + 1)
                            bz = tl.load(ghost_verts_ptr + i1 * 3 + 2)

                            cx_ = tl.load(ghost_verts_ptr + i2 * 3 + 0)
                            cy_ = tl.load(ghost_verts_ptr + i2 * 3 + 1)
                            cz_ = tl.load(ghost_verts_ptr + i2 * 3 + 2)

                            # Barycentric calculation
                            v0x, v0y, v0z = bx - ax, by - ay, bz - az
                            v1x, v1y, v1z = cx_ - ax, cy_ - ay, cz_ - az
                            v2x, v2y, v2z = px - ax, py - ay, pz - az

                            dot00 = v0x*v0x + v0y*v0y + v0z*v0z
                            dot01 = v0x*v1x + v0y*v1y + v0z*v1z
                            dot02 = v0x*v2x + v0y*v2y + v0z*v2z
                            dot11 = v1x*v1x + v1y*v1y + v1z*v1z
                            dot12 = v1x*v2x + v1y*v2y + v1z*v2z

                            inv_denom = 1.0 / (dot00 * dot11 - dot01 * dot01 + 1e-12)
                            u = (dot11 * dot02 - dot01 * dot12) * inv_denom
                            v = (dot00 * dot12 - dot01 * dot02) * inv_denom
                            w = 1.0 - u - v

                            # Approximate distance
                            dx_dist = u*ax + v*bx + w*cx_ - px
                            dy_dist = u*ay + v*by + w*cy_ - py
                            dz_dist = u*az + v*bz + w*cz_ - pz
                            dist = dx_dist*dx_dist + dy_dist*dy_dist + dz_dist*dz_dist

                            if dist < best_dist:
                                best_dist = dist
                                best_tri = tri_id
                                best_u = u
                                best_v = v
                                best_w = w

    tl.store(out_tri_idx_ptr + pid, best_tri)
    tl.store(out_uvw_ptr + pid * 3 + 0, best_u)
    tl.store(out_uvw_ptr + pid * 3 + 1, best_v)
    tl.store(out_uvw_ptr + pid * 3 + 2, best_w)


# ====================== MAIN FUNCTION ======================

def build_full_gpu_spatial_slave_mapping(
    high_verts: torch.Tensor,
    ghost_verts: torch.Tensor,
    ghost_faces: torch.Tensor,
    target_voxel_size: float = 0.05,
) -> Tuple[torch.Tensor, torch.Tensor]:
    device = high_verts.device
    F = ghost_faces.shape[0]
    N = high_verts.shape[0]

    grid_res = compute_adaptive_grid_resolution(ghost_verts, target_voxel_size)
    grid_size = grid_res ** 3

    # Fixed buffer allocation
    voxel_triangle_buffer = torch.full(
        (grid_size, MAX_TRIS_PER_VOXEL), -1, device=device, dtype=torch.int32
    )
    voxel_counts = torch.zeros(grid_size, device=device, dtype=torch.int32)

    grid_min = ghost_verts.min(dim=0).values
    grid_max = ghost_verts.max(dim=0).values
    grid_size_vec = grid_max - grid_min + 1e-6
    grid_inv_size = grid_res / grid_size_vec
    voxel_size = (grid_size_vec / grid_res).mean().item()

    triangle_centers = ghost_verts[ghost_faces.to(torch.long)].mean(dim=1)

    # Build phase (single bounded atomic pass)
    build_fixed_voxel_buffer_kernel[(F,)](
        ghost_faces, ghost_verts,
        grid_min, grid_inv_size, grid_res,
        voxel_triangle_buffer, voxel_counts,
        F, MAX_TRIS_PER_VOXEL
    )

    # Query phase
    tri_idx_out = torch.empty(N, device=device, dtype=torch.int32)
    uvw_out = torch.empty((N, 3), device=device, dtype=torch.float32)

    query_fixed_buffer_kernel[(triton.cdiv(N, 256),)](
        high_verts, ghost_verts, triangle_centers, ghost_faces,
        grid_min, grid_inv_size, grid_res,
        voxel_triangle_buffer, voxel_counts,
        tri_idx_out, uvw_out,
        N, voxel_size, MAX_TRIS_PER_VOXEL
    )

    return tri_idx_out, uvw_out
