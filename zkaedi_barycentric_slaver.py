import torch
import triton
import triton.language as tl
from dataclasses import dataclass
from typing import Optional

@dataclass
class BarySlaveData:
    """Persistent mapping. Stored once. Used every frame."""
    triangle_idx: torch.Tensor   # [N_high] int32
    uvw: torch.Tensor            # [N_high, 3] float32  (u, v, w)
    N: int

@triton.jit
def barycentric_deform_kernel(
    ghost_verts_ptr,      # [V_ghost, 3] deformed Ghost vertices (current frame)
    tri_idx_ptr,          # [N_high]
    uvw_ptr,              # [N_high, 3]
    out_verts_ptr,        # [N_high, 3] output high-poly positions
    N_high: tl.constexpr,
    BLOCK: tl.constexpr = 256,
):
    pid = tl.program_id(0)
    offs = pid * BLOCK + tl.arange(0, BLOCK)
    mask = offs < N_high

    t_idx = tl.load(tri_idx_ptr + offs, mask=mask, other=0)

    # Load barycentric weights
    u = tl.load(uvw_ptr + offs * 3 + 0, mask=mask, other=0.0)
    v = tl.load(uvw_ptr + offs * 3 + 1, mask=mask, other=0.0)
    w = tl.load(uvw_ptr + offs * 3 + 2, mask=mask, other=0.0)

    # Load the three deformed Ghost vertices of the triangle
    # (assumes contiguous Ghost verts; adjust stride if needed)
    ax = tl.load(ghost_verts_ptr + t_idx * 3 + 0, mask=mask, other=0.0)
    ay = tl.load(ghost_verts_ptr + t_idx * 3 + 1, mask=mask, other=0.0)
    az = tl.load(ghost_verts_ptr + t_idx * 3 + 2, mask=mask, other=0.0)

    bx = tl.load(ghost_verts_ptr + (t_idx + 1) * 3 + 0, mask=mask, other=0.0)
    by = tl.load(ghost_verts_ptr + (t_idx + 1) * 3 + 1, mask=mask, other=0.0)
    bz = tl.load(ghost_verts_ptr + (t_idx + 1) * 3 + 2, mask=mask, other=0.0)

    cx = tl.load(ghost_verts_ptr + (t_idx + 2) * 3 + 0, mask=mask, other=0.0)
    cy = tl.load(ghost_verts_ptr + (t_idx + 2) * 3 + 1, mask=mask, other=0.0)
    cz = tl.load(ghost_verts_ptr + (t_idx + 2) * 3 + 2, mask=mask, other=0.0)

    # Barycentric interpolation — this is the magic
    px = u * ax + v * bx + w * cx
    py = u * ay + v * by + w * cy
    pz = u * az + v * bz + w * cz

    tl.store(out_verts_ptr + offs * 3 + 0, px, mask=mask)
    tl.store(out_verts_ptr + offs * 3 + 1, py, mask=mask)
    tl.store(out_verts_ptr + offs * 3 + 2, pz, mask=mask)


class ZkaediBarycentricSlaver:
    """
    One-time slaving + 60 FPS deformation.
    The 500 MB mesh becomes a pure geometric slave.
    """
    def __init__(self, fusion):
        self.fusion = fusion
        self.slave_data: Optional[BarySlaveData] = None
        self.deformed_high: Optional[torch.Tensor] = None

    def build_slave_mapping(self, high_verts, ghost_verts, ghost_faces, precomputed_tri_idx=None, precomputed_uvw=None):
        if precomputed_tri_idx is None:
            from zkaedi_gpu_spatial_hash_full import build_full_gpu_spatial_slave_mapping
            tri_idx, uvw = build_full_gpu_spatial_slave_mapping(
                high_verts, ghost_verts, ghost_faces
            )
        else:
            tri_idx = precomputed_tri_idx
            if precomputed_uvw is not None:
                uvw = precomputed_uvw
            else:
                uvw = torch.zeros((high_verts.shape[0], 3), device=high_verts.device, dtype=torch.float32)

        N = high_verts.shape[0]
        self.slave_data = BarySlaveData(triangle_idx=tri_idx, uvw=uvw, N=N)
        self.deformed_high = torch.empty((N, 3), device=high_verts.device, dtype=torch.float32)

    def deform_at_60fps(self, ghost_deformed_verts: torch.Tensor) -> torch.Tensor:
        """
        Call every frame. Extremely fast.
        Ghost moves → high-poly instantly follows with perfect interpolation.
        """
        if self.slave_data is None:
            raise RuntimeError("Call build_slave_mapping first")

        grid = (triton.cdiv(self.slave_data.N, 256),)
        barycentric_deform_kernel[grid](
            ghost_deformed_verts,
            self.slave_data.triangle_idx,
            self.slave_data.uvw,
            self.deformed_high,
            self.slave_data.N,
        )
        return self.deformed_high
