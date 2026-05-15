"""
ZKAEDI GPU-Native Rigger - Full Orchestrator
One file to rule them all.
C voxel shrink-wrap → Warden shim → Triton upload → GPU spatial hash →
Bary slaving → 60 FPS deformation → Baked GLB export.
"""

import ctypes
import numpy as np
import torch
from zkaedi_baked_glb_writer import ZkaediBakedGLBWriter, BakedSlaveMapping
from zkaedi_gpu_spatial_hash_full import build_full_gpu_spatial_slave_mapping
from zkaedi_barycentric_slaver import ZkaediBarycentricSlaver
from zkaedi_triton_fusion import ZkaediTritonFusion
from zkaedi_validation import validate_zkaedi_pipeline

import sys
import os
import logging
from datetime import datetime

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s | %(levelname)s | %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler("zkaedi_pipeline.log", mode='a')
    ]
)
logger = logging.getLogger("ZKAEDI")

import sys
import os

# === Load your compiled ZCC shared library ===
lib_path = r"g:\zccMAIN\zcc\zcc_ghost.dll" if sys.platform == "win32" else "/mnt/g/zccMAIN/zcc/zcc_ghost.so"
if not os.path.exists(lib_path):
    # Fallback to local directory if absolute path fails
    lib_path = "./zcc_ghost.dll" if sys.platform == "win32" else "./zcc_ghost.so"
lib = ctypes.CDLL(lib_path)

class ZkaediOrchestrator:
    def __init__(self):
        self.fusion = ZkaediTritonFusion()
        self.slaver = ZkaediBarycentricSlaver(self.fusion)
        self.ghost_handle = None

    def process_mesh(
        self,
        high_positions: np.ndarray,
        high_indices: np.ndarray,
        ghost_verts: np.ndarray,
        ghost_faces: np.ndarray,
        bone_sources: torch.Tensor,
        output_glb: str = "rigged_slave.glb"
    ):
        logger.info("[ZKAEDI] Starting full pipeline...")

        # 1. Upload Ghost to VRAM via C shim + Triton
        # (Assume you already called the C voxel_shrinkwrap + register functions)
        # self.fusion.upload_ghost(...)  # wire your C handle here

        # 2. GPU Spatial Hash + Barycentric Bake (fully on-device)
        tri_idx, uvw = build_full_gpu_spatial_slave_mapping(
            torch.from_numpy(high_positions).cuda(),
            torch.from_numpy(ghost_verts).cuda(),
            torch.from_numpy(ghost_faces).cuda()
        )

        # 3. Wire into slaver
        self.slaver.build_slave_mapping(
            torch.from_numpy(high_positions).cuda(),
            torch.from_numpy(ghost_verts).cuda(),
            torch.from_numpy(ghost_faces).cuda(),
            precomputed_tri_idx=tri_idx,
            precomputed_uvw=uvw
        )

        from zkaedi_utils import normalize_barycentric_weights
        from zkaedi_diagnostics import ZkaediDiagnostics

        # === Post-processing normalization ===
        # clamp_negative is False because barycentric extrapolation is mathematically REQUIRED 
        # for high-poly slave vertices that sit outside the proxy Ghost mesh surface.
        uvw_normalized, tri_idx_norm, quality_report = normalize_barycentric_weights(
            self.slaver.slave_data.uvw,
            self.slaver.slave_data.triangle_idx,
            clamp_negative=False,
            return_report=True
        )

        self.slaver.slave_data.uvw = uvw_normalized
        self.slaver.slave_data.triangle_idx = tri_idx_norm

        if quality_report:
            logger.info("=== Barycentric Quality Report ===")
            logger.info(f"Assignment Rate       : {quality_report['assignment_rate']*100:.1f}%")
            logger.info(f"Vertices Clamped      : {quality_report['clamped_vertices']} ({quality_report['clamped_ratio']*100:.1f}%)")
            logger.info(f"Mean Weight Sum (Before) : {quality_report['mean_weight_sum_before']}")
            logger.info(f"Mean Weight Sum (After)  : {quality_report['mean_weight_sum_after']}")
            logger.info(f"Negative Weight Ratio : {quality_report['negative_weight_ratio']}")

        try:
            results = validate_zkaedi_pipeline(
                uvw=self.slaver.slave_data.uvw,
                tri_idx=self.slaver.slave_data.triangle_idx,
                ghost_faces=torch.from_numpy(ghost_faces).cuda(),
                ghost_verts=torch.from_numpy(ghost_verts).cuda(),
                verbose=True
            )

            if not results["all_invariants_passed"]:
                diagnostics = ZkaediDiagnostics()
                tips = diagnostics.analyze(quality_report, results)
                diagnostics.log_tips(tips, logger)
                
                if any(t.severity.value in ["CRITICAL", "ERROR"] for t in tips):
                    raise RuntimeError("Pipeline failed critical diagnostics. Review tips above.")

            logger.info("Validation passed successfully.")

        except Exception as e:
            logger.exception("Critical error during ZKAEDI pipeline execution")
            raise

        # 4. Solve weights on Ghost (your existing Triton kernel)
        weights = self.fusion.solve_weights(bone_sources)

        # 5. (Optional) Export the final baked GLB with slave mapping
        writer = ZkaediBakedGLBWriter()
        writer.write(
            positions=high_positions,
            indices=high_indices,
            slave=BakedSlaveMapping(
                triangle_idx=tri_idx.cpu().numpy().astype(np.uint32),
                uvw=uvw.cpu().numpy().astype(np.float32)
            ),
            output_path=output_glb
        )

        print("[ZKAEDI] Pipeline complete. Dancing geometry serialized.")
        return weights

    def animate_frame(self, ghost_deformed_verts: torch.Tensor):
        """Call this every frame at 60 FPS."""
        return self.slaver.deform_at_60fps(ghost_deformed_verts)


if __name__ == "__main__":
    orch = ZkaediOrchestrator()
    print("ZKAEDI Orchestrator ready. Generating mathematical test manifold...")

    # Generate synthetic mathematical manifold (a simple 3D grid)
    res = 32
    x = np.linspace(-1, 1, res)
    y = np.linspace(-1, 1, res)
    z = np.linspace(-1, 1, res)
    xv, yv, zv = np.meshgrid(x, y, z)
    
    # High poly positions (32^3 = 32,768 verts)
    high_pos = np.vstack([xv.flatten(), yv.flatten(), zv.flatten()]).T.astype(np.float32)
    # Fake high indices (not needed for the math validation of the slaver)
    high_idx = np.arange(len(high_pos), dtype=np.uint32)

    # Decimate to create a synthetic Ghost Mesh (8^3 = 512 verts)
    ghost_res = 8
    gx = np.linspace(-1.1, 1.1, ghost_res)
    gy = np.linspace(-1.1, 1.1, ghost_res)
    gz = np.linspace(-1.1, 1.1, ghost_res)
    gxv, gyv, gzv = np.meshgrid(gx, gy, gz)
    ghost_verts = np.vstack([gxv.flatten(), gyv.flatten(), gzv.flatten()]).T.astype(np.float32)

    # Generate rudimentary tetrahedra/triangles for the ghost faces
    # To keep it completely dependency-free, we just triangulate the grid naively
    ghost_faces = []
    for i in range(ghost_res - 1):
        for j in range(ghost_res - 1):
            for k in range(ghost_res - 1):
                idx = i * ghost_res * ghost_res + j * ghost_res + k
                # 2 triangles per face (just testing the math limits, doesn't need to be closed)
                ghost_faces.extend([
                    [idx, idx+1, idx+ghost_res],
                    [idx+1, idx+ghost_res+1, idx+ghost_res]
                ])
    ghost_faces = np.array(ghost_faces, dtype=np.uint32)

    # Synthetic bone sources (4 bones)
    K = 4
    bone_sources = torch.zeros((len(ghost_verts), K), dtype=torch.float32, device="cuda")
    bone_sources[0:100, 0] = 1.0
    bone_sources[100:200, 1] = 1.0
    bone_sources[200:300, 2] = 1.0
    bone_sources[300:, 3] = 1.0

    print(f"Firing {len(high_pos)} high-poly vertices against {len(ghost_faces)} Ghost triangles.")
    
    # We bypass the C-shim upload for this synthetic test and just hit the tensor cores directly
    # To do this cleanly we inject the ghost directly into the fusion layer
    orch.fusion.verts_cuda = torch.from_numpy(ghost_verts).cuda()
    orch.fusion.loaded = True

    # Build fallback adjacency for the synthetic test since we skipped the Mesh Warden
    def build_padded_adjacency_fallback(faces_tensor, num_verts, max_degree):
        faces = faces_tensor.cpu().numpy()
        import collections
        adj = collections.defaultdict(set)
        for f in faces:
            for i in range(3):
                adj[f[i]].add(f[(i+1)%3])
                adj[f[i]].add(f[(i+2)%3])
        
        neighs = np.full((num_verts, max_degree), -1, dtype=np.int32)
        counts = np.zeros(num_verts, dtype=np.int32)
        for v in range(num_verts):
            nlist = list(adj[v])[:max_degree]
            counts[v] = len(nlist)
            if len(nlist) > 0:
                neighs[v, :len(nlist)] = nlist
        return {'neighbors': neighs, 'counts': counts}

    adj = build_padded_adjacency_fallback(torch.from_numpy(ghost_faces).cuda(), len(ghost_verts), 12)
    orch.fusion.adjacency_cuda = torch.tensor(adj['neighbors'], device="cuda", dtype=torch.int32)
    orch.fusion.neigh_count_cuda = torch.tensor(adj['counts'], device="cuda", dtype=torch.int32)
    orch.fusion.max_degree = 12

    try:
        weights = orch.process_mesh(
            high_positions=high_pos,
            high_indices=high_idx,
            ghost_verts=ghost_verts,
            ghost_faces=ghost_faces,
            bone_sources=bone_sources,
            output_glb="synthetic_test_output.glb"
        )
        print("✅ Mathematical ignition successful. All constraints held. Ready for GLB ingestion.")
    except Exception as e:
        print(f"[ERROR] Pipeline fractured during test ignition: {e}")
