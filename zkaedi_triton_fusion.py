import ctypes
import torch
import triton
from typing import Dict, Optional
from zkaedi_heat_diffusion import solve_bone_weights  # your previous kernel drop
import sys
import os

# === Load the ZCC shared lib (compile shim + voxelizer into zcc_ghost.dll / .so) ===
lib_path = r"g:\zccMAIN\zcc\zcc_ghost.dll" if sys.platform == "win32" else "/mnt/g/zccMAIN/zcc/zcc_ghost.so"
if not os.path.exists(lib_path):
    lib_path = "./zcc_ghost.dll" if sys.platform == "win32" else "./zcc_ghost.so"
lib = ctypes.CDLL(lib_path)

class ZccWardenHandle(ctypes.Structure):
    _fields_ = [
        ("verts", ctypes.POINTER(ctypes.c_float)),
        ("indices", ctypes.POINTER(ctypes.c_uint32)),
        ("adjacency", ctypes.POINTER(ctypes.c_uint32)),
        ("neigh_count", ctypes.POINTER(ctypes.c_uint8)),
        ("vcount", ctypes.c_size_t),
        ("icount", ctypes.c_size_t),
        ("max_degree", ctypes.c_int),
        ("ready_for_vram", ctypes.c_int),
    ]

lib.zcc_register_ghost_with_warden.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int, ctypes.POINTER(ZccWardenHandle)]
lib.zcc_release_warden_handle.argtypes = [ctypes.POINTER(ZccWardenHandle)]

class ZkaediTritonFusion:
    """Persistent handle. Upload once, run kernels forever."""
    def __init__(self, max_degree: int = 12):
        self.handle = ZccWardenHandle()
        self.max_degree = max_degree
        self.verts_cuda: Optional[torch.Tensor] = None
        self.adjacency_cuda: Optional[torch.Tensor] = None
        self.neigh_count_cuda: Optional[torch.Tensor] = None
        self.loaded = False

    def upload_ghost(self, ghost_mesh_ptr) -> bool:
        """Fuse C-native Ghost buffer directly into VRAM."""
        if lib.zcc_register_ghost_with_warden(ghost_mesh_ptr, self.max_degree, ctypes.byref(self.handle)) == 0:
            return False

        V = self.handle.vcount
        # Zero-copy spirit: create torch tensors from the raw pointers
        self.verts_cuda = torch.from_numpy(
            ctypes.cast(self.handle.verts, ctypes.POINTER(ctypes.c_float * (V * 3))).contents
        ).to("cuda", dtype=torch.float32).view(V, 3)

        self.adjacency_cuda = torch.from_numpy(
            ctypes.cast(self.handle.adjacency, ctypes.POINTER(ctypes.c_uint32 * (V * self.max_degree))).contents
        ).to("cuda", dtype=torch.int32).view(V, self.max_degree)

        self.neigh_count_cuda = torch.from_numpy(
            ctypes.cast(self.handle.neigh_count, ctypes.POINTER(ctypes.c_uint8 * V)).contents
        ).to("cuda", dtype=torch.int32)

        self.loaded = True
        return True

    def solve_weights(self, bone_sources: torch.Tensor, iters: int = 800):
        """Direct call into the Triton diffusion kernel you already have."""
        if not self.loaded:
            raise RuntimeError("Ghost not uploaded to VRAM")
        return solve_bone_weights(
            self.verts_cuda,
            None,  # faces not needed since adjacency is passed directly
            bone_sources,
            num_iters=iters,
            max_neigh=self.max_degree,
            precomputed_adjacency=self.adjacency_cuda,
            precomputed_neigh_count=self.neigh_count_cuda
        )

    def release(self):
        lib.zcc_release_warden_handle(ctypes.byref(self.handle))
        self.loaded = False

# === One-liner close-the-loop usage ===
if __name__ == "__main__":
    fusion = ZkaediTritonFusion()
    # After you called zcc_voxel_shrinkwrap in C and got a GhostMesh*
    # ghost_ptr = ... from your part5.c flow
    # fusion.upload_ghost(ghost_ptr)
    # weights = fusion.solve_weights(bone_sources_cuda)
    print("ZKAEDI loop fused. Ghost → VRAM → Triton. Ready.")
