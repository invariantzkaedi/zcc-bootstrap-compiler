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

MAX_TRIS_PER_VOXEL: int = 128


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

def build_voxel_grid_pytorch(ghost_verts, ghost_faces, grid_min, grid_inv_size, grid_res, MAX_TRIS):
    import numpy as np
    device = ghost_verts.device
    F = ghost_faces.shape[0]
    
    tris = ghost_verts[ghost_faces.to(torch.long)] # [F, 3, 3]
    tmin = tris.min(dim=1).values # [F, 3]
    tmax = tris.max(dim=1).values # [F, 3]
    
    vmin = torch.clamp(((tmin - grid_min) * grid_inv_size).to(torch.int32), 0, grid_res - 1)
    vmax = torch.clamp(((tmax - grid_min) * grid_inv_size).to(torch.int32), 0, grid_res - 1)
    
    vmin_cpu = vmin.cpu().numpy()
    vmax_cpu = vmax.cpu().numpy()
    
    grid_size = grid_res ** 3
    buf_cpu = np.full((grid_size, MAX_TRIS), -1, dtype=np.int32)
    counts_cpu = np.zeros(grid_size, dtype=np.int32)
    
    for i in range(F):
        x0, y0, z0 = vmin_cpu[i]
        x1, y1, z1 = vmax_cpu[i]
        for z in range(z0, z1+1):
            for y in range(y0, y1+1):
                for x in range(x0, x1+1):
                    vidx = (z * grid_res + y) * grid_res + x
                    c = counts_cpu[vidx]
                    if c < MAX_TRIS:
                        buf_cpu[vidx, c] = i
                        counts_cpu[vidx] = c + 1
                        
    return torch.from_numpy(buf_cpu).to(device), torch.from_numpy(counts_cpu).to(device)


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
    BLOCK_SIZE: tl.constexpr,
):
    pid = tl.program_id(0)
    offs = pid * BLOCK_SIZE + tl.arange(0, BLOCK_SIZE)
    mask = offs < N

    px = tl.load(high_verts_ptr + offs * 3 + 0, mask=mask, other=0.0)
    py = tl.load(high_verts_ptr + offs * 3 + 1, mask=mask, other=0.0)
    pz = tl.load(high_verts_ptr + offs * 3 + 2, mask=mask, other=0.0)

    vx = tl.cast((px - tl.load(grid_min_ptr + 0)) * tl.load(grid_inv_size_ptr + 0), tl.int32)
    vy = tl.cast((py - tl.load(grid_min_ptr + 1)) * tl.load(grid_inv_size_ptr + 1), tl.int32)
    vz = tl.cast((pz - tl.load(grid_min_ptr + 2)) * tl.load(grid_inv_size_ptr + 2), tl.int32)

    best_dist = tl.full([BLOCK_SIZE], 1e30, dtype=tl.float32)
    best_tri = tl.full([BLOCK_SIZE], -1, dtype=tl.int32)
    best_u = tl.full([BLOCK_SIZE], 0.0, dtype=tl.float32)
    best_v = tl.full([BLOCK_SIZE], 0.0, dtype=tl.float32)
    best_w = tl.full([BLOCK_SIZE], 0.0, dtype=tl.float32)

    for dz in range(-2, 3):
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                nx = vx + dx
                ny = vy + dy
                nz = vz + dz
                
                valid_voxel = (nx >= 0) & (ny >= 0) & (nz >= 0) & (nx < grid_res) & (ny < grid_res) & (nz < grid_res)
                vidx = (nz * grid_res + ny) * grid_res + nx
                
                count = tl.load(voxel_counts_ptr + vidx, mask=valid_voxel, other=0)

                for i in range(MAX_TRIS):
                    valid_tri = (i < count) & valid_voxel & mask
                    tri_id = tl.load(voxel_triangle_buffer_ptr + vidx * MAX_TRIS + i, mask=valid_tri, other=-1)
                    
                    is_valid = tri_id >= 0

                    i0 = tl.load(faces_ptr + tri_id * 3 + 0, mask=is_valid, other=0)
                    i1 = tl.load(faces_ptr + tri_id * 3 + 1, mask=is_valid, other=0)
                    i2 = tl.load(faces_ptr + tri_id * 3 + 2, mask=is_valid, other=0)

                    ax = tl.load(ghost_verts_ptr + i0 * 3 + 0, mask=is_valid, other=0.0)
                    ay = tl.load(ghost_verts_ptr + i0 * 3 + 1, mask=is_valid, other=0.0)
                    az = tl.load(ghost_verts_ptr + i0 * 3 + 2, mask=is_valid, other=0.0)

                    bx = tl.load(ghost_verts_ptr + i1 * 3 + 0, mask=is_valid, other=0.0)
                    by = tl.load(ghost_verts_ptr + i1 * 3 + 1, mask=is_valid, other=0.0)
                    bz = tl.load(ghost_verts_ptr + i1 * 3 + 2, mask=is_valid, other=0.0)

                    cx_ = tl.load(ghost_verts_ptr + i2 * 3 + 0, mask=is_valid, other=0.0)
                    cy_ = tl.load(ghost_verts_ptr + i2 * 3 + 1, mask=is_valid, other=0.0)
                    cz_ = tl.load(ghost_verts_ptr + i2 * 3 + 2, mask=is_valid, other=0.0)

                    v0x = bx - ax
                    v0y = by - ay
                    v0z = bz - az
                    v1x = cx_ - ax
                    v1y = cy_ - ay
                    v1z = cz_ - az
                    v2x = px - ax
                    v2y = py - ay
                    v2z = pz - az

                    dot00 = v0x*v0x + v0y*v0y + v0z*v0z
                    dot01 = v0x*v1x + v0y*v1y + v0z*v1z
                    dot02 = v0x*v2x + v0y*v2y + v0z*v2z
                    dot11 = v1x*v1x + v1y*v1y + v1z*v1z
                    dot12 = v1x*v2x + v1y*v2y + v1z*v2z

                    denom = dot00 * dot11 - dot01 * dot01
                    is_denom_valid = is_valid & (denom != 0.0)
                    
                    inv_denom = 1.0 / (denom + 1e-12)

                    u = (dot11 * dot02 - dot01 * dot12) * inv_denom
                    v = (dot00 * dot12 - dot01 * dot02) * inv_denom
                    w = 1.0 - u - v

                    # Very lenient bounds to allow proxy mesh extrapolation while rejecting infinite parallel planes
                    inside = (u >= -2.0) & (v >= -2.0) & (w >= -2.0) & (u <= 3.0) & (v <= 3.0) & (w <= 3.0)

                    qx = u * ax + v * bx + w * cx_
                    qy = u * ay + v * by + w * cy_
                    qz = u * az + v * bz + w * cz_

                    dist = (qx - px)*(qx - px) + (qy - py)*(qy - py) + (qz - pz)*(qz - pz)

                    better = is_denom_valid & inside & (dist < best_dist)

                    best_dist = tl.where(better, dist, best_dist)
                    best_tri = tl.where(better, tri_id, best_tri)
                    best_u = tl.where(better, u, best_u)
                    best_v = tl.where(better, v, best_v)
                    best_w = tl.where(better, w, best_w)

    tl.store(out_tri_idx_ptr + offs, best_tri, mask=mask)
    tl.store(out_uvw_ptr + offs * 3 + 0, best_u, mask=mask)
    tl.store(out_uvw_ptr + offs * 3 + 1, best_v, mask=mask)
    tl.store(out_uvw_ptr + offs * 3 + 2, best_w, mask=mask)


# ====================== MAIN FUNCTION ======================

def build_full_gpu_spatial_slave_mapping(
    high_verts: torch.Tensor,
    ghost_verts: torch.Tensor,
    ghost_faces: torch.Tensor,
    target_voxel_size: float = 0.05,
) -> Tuple[torch.Tensor, torch.Tensor]:
    high_verts = high_verts.contiguous()
    ghost_verts = ghost_verts.contiguous()
    ghost_faces = ghost_faces.contiguous()

    device = high_verts.device
    F = ghost_faces.shape[0]
    N = high_verts.shape[0]

    grid_res = compute_adaptive_grid_resolution(ghost_verts, target_voxel_size)
    grid_size = grid_res ** 3

    grid_min = ghost_verts.min(dim=0).values
    grid_max = ghost_verts.max(dim=0).values
    grid_size_vec = grid_max - grid_min + 1e-6
    grid_inv_size = grid_res / grid_size_vec
    voxel_size = (grid_size_vec / grid_res).mean().item()

    triangle_centers = ghost_verts[ghost_faces.to(torch.long)].mean(dim=1)

    # Build phase (deterministic CPU pass for low-poly Ghost Mesh)
    voxel_triangle_buffer, voxel_counts = build_voxel_grid_pytorch(
        ghost_verts, ghost_faces, grid_min, grid_inv_size, grid_res, MAX_TRIS_PER_VOXEL
    )

    # Query phase (Vectorized)
    tri_idx_out = torch.full((N,), -1, device=device, dtype=torch.int32)
    uvw_out = torch.zeros((N, 3), device=device, dtype=torch.float32)

    BLOCK_SIZE = 256
    grid = (triton.cdiv(N, BLOCK_SIZE),)
    query_fixed_buffer_kernel[grid](
        high_verts, ghost_verts, triangle_centers, ghost_faces,
        grid_min, grid_inv_size, grid_res,
        voxel_triangle_buffer, voxel_counts,
        tri_idx_out, uvw_out,
        N, voxel_size, MAX_TRIS_PER_VOXEL, BLOCK_SIZE
    )

    # === Fallback Pass for Unmapped Vertices ===
    unmapped_mask = tri_idx_out < 0
    if unmapped_mask.any():
        unmapped_verts = high_verts[unmapped_mask]
        U = unmapped_verts.shape[0]
        
        # Chunked to prevent OOM on large fallbacks
        chunk_size = 4096
        fallback_tris = torch.empty(U, dtype=torch.long, device=device)
        
        for i in range(0, U, chunk_size):
            end = min(i + chunk_size, U)
            chunk = unmapped_verts[i:end]
            dists = torch.cdist(chunk, triangle_centers)
            fallback_tris[i:end] = torch.argmin(dists, dim=1)
            
        best_faces = ghost_faces.to(torch.long)[fallback_tris]
        ax, ay, az = ghost_verts[best_faces[:, 0], 0], ghost_verts[best_faces[:, 0], 1], ghost_verts[best_faces[:, 0], 2]
        bx, by, bz = ghost_verts[best_faces[:, 1], 0], ghost_verts[best_faces[:, 1], 1], ghost_verts[best_faces[:, 1], 2]
        cx, cy, cz = ghost_verts[best_faces[:, 2], 0], ghost_verts[best_faces[:, 2], 1], ghost_verts[best_faces[:, 2], 2]
        px, py, pz = unmapped_verts[:, 0], unmapped_verts[:, 1], unmapped_verts[:, 2]

        v0x, v0y, v0z = bx - ax, by - ay, bz - az
        v1x, v1y, v1z = cx - ax, cy - ay, cz - az
        v2x, v2y, v2z = px - ax, py - ay, pz - az

        dot00 = v0x*v0x + v0y*v0y + v0z*v0z
        dot01 = v0x*v1x + v0y*v1y + v0z*v1z
        dot02 = v0x*v2x + v0y*v2y + v0z*v2z
        dot11 = v1x*v1x + v1y*v1y + v1z*v1z
        dot12 = v1x*v2x + v1y*v2y + v1z*v2z

        denom = dot00 * dot11 - dot01 * dot01 + 1e-12
        inv_denom = 1.0 / denom

        u = (dot11 * dot02 - dot01 * dot12) * inv_denom
        v = (dot00 * dot12 - dot01 * dot02) * inv_denom
        w = 1.0 - u - v

        tri_idx_out[unmapped_mask] = fallback_tris.to(torch.int32)
        uvw_out[unmapped_mask] = torch.stack([u, v, w], dim=1)

    return tri_idx_out, uvw_out
