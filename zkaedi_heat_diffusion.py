# g:\zccMAIN\zcc\zkaedi_heat_diffusion.py
"""
ZKAEDI PRIME Heat Diffusion Kernel
Iterative Jacobi-style bone heat weighting on GPU via Triton.
"""

import torch
import triton
import triton.language as tl


@triton.jit
def zkaedi_heat_diffusion_step(
    heat_ptr,
    next_heat_ptr,
    neigh_ptr,
    neigh_count_ptr,
    source_mask_ptr,
    V: tl.constexpr,
    MAX_NEIGH: tl.constexpr,
    BLOCK_SIZE: tl.constexpr = 256,
):
    pid = tl.program_id(0)
    offs = pid * BLOCK_SIZE + tl.arange(0, BLOCK_SIZE)
    mask = offs < V

    ncount = tl.load(neigh_count_ptr + offs, mask=mask, other=0)

    sum_heat = tl.zeros([BLOCK_SIZE], dtype=tl.float32)
    for i in range(MAX_NEIGH):
        nid = tl.load(neigh_ptr + offs * MAX_NEIGH + i, mask=mask, other=-1)
        valid = (nid >= 0) & (i < ncount)
        h = tl.load(heat_ptr + nid, mask=valid, other=0.0)
        sum_heat += h

    avg = sum_heat / tl.maximum(ncount.to(tl.float32), 1.0)

    is_source = tl.load(source_mask_ptr + offs, mask=mask, other=0.0) > 0.5
    new_val = tl.where(is_source, 1.0, avg)

    tl.store(next_heat_ptr + offs, new_val, mask=mask)


def solve_bone_weights(
    verts: torch.Tensor,
    faces: torch.Tensor,
    bone_sources: torch.Tensor,
    num_iters: int = 800,
    max_neigh: int = 12,
    precomputed_adjacency: torch.Tensor = None,
    precomputed_neigh_count: torch.Tensor = None,
) -> torch.Tensor:
    """
    ZKAEDI PRIME Convergent Solver (Mode A)
    Solves bone weights via iterative heat diffusion on the Ghost Mesh.
    """
    V = verts.shape[0]
    K = bone_sources.shape[1] if bone_sources.dim() == 2 else 1
    device = verts.device

    if precomputed_adjacency is not None and precomputed_neigh_count is not None:
        neigh = precomputed_adjacency
        neigh_count = precomputed_neigh_count
    else:
        # Build padded adjacency
        from zkaedi_gpu_spatial_hash_full import build_padded_adjacency  # optional helper
        adj = build_padded_adjacency(faces, V, max_neigh) if 'build_padded_adjacency' in globals() else None

        if adj is None:
            raise NotImplementedError("Provide adjacency from C-side or implement build_padded_adjacency()")

        neigh = torch.tensor(adj['neighbors'], device=device, dtype=torch.int32)
        neigh_count = torch.tensor(adj['counts'], device=device, dtype=torch.int32)

    weights = torch.zeros((V, K), device=device, dtype=torch.float32)

    for k in range(K):
        heat = torch.zeros(V, device=device, dtype=torch.float32)
        source_mask = bone_sources[:, k] if bone_sources.dim() == 2 else bone_sources

        heat[source_mask > 0.5] = 1.0
        heat_next = torch.empty_like(heat)

        for _ in range(num_iters):
            grid = (triton.cdiv(V, 256),)
            zkaedi_heat_diffusion_step[grid](
                heat, heat_next,
                neigh, neigh_count,
                source_mask,
                V, max_neigh,
            )
            heat, heat_next = heat_next, heat

        weights[:, k] = heat

    # Normalize to partition of unity
    weights = weights / (weights.sum(dim=1, keepdim=True) + 1e-8)
    return weights
