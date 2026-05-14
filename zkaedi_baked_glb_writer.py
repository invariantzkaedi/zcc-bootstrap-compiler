import json
import struct
from dataclasses import dataclass
import numpy as np
from typing import Optional

@dataclass
class BakedSlaveMapping:
    triangle_idx: np.ndarray   # uint32 [N]
    uvw: np.ndarray            # float32 [N, 3]

class ZkaediBakedGLBWriter:
    """
    Minimal, tight GLB writer.
    Adds two custom vertex attributes:
        _SLAVE_TRIANGLE_IDX  (SCALAR, uint32)
        _SLAVE_BARY_UVW      (VEC3, float32)
    The main geometry (POSITION + indices) remains 100% readable by your C parser.
    No bloat. 4-byte aligned. Clean chunks.
    """

    def __init__(self):
        self.buffer = bytearray()
        self.buffer_views = []
        self.accessors = []
        self.meshes = []

    def _align(self, size: int) -> int:
        return (size + 3) & ~3

    def _add_buffer_view(self, data: bytes, target: Optional[int] = None) -> int:
        byte_offset = len(self.buffer)
        padded = self._align(len(data))
        self.buffer.extend(data)
        self.buffer.extend(b'\x00' * (padded - len(data)))

        view = {
            "buffer": 0,
            "byteOffset": byte_offset,
            "byteLength": len(data),
        }
        if target:
            view["target"] = target
        self.buffer_views.append(view)
        return len(self.buffer_views) - 1

    def _add_accessor(self, buffer_view: int, component_type: int, count: int,
                      type_: str, byte_offset: int = 0, min_val=None, max_val=None) -> int:
        acc = {
            "bufferView": buffer_view,
            "byteOffset": byte_offset,
            "componentType": component_type,
            "count": count,
            "type": type_,
        }
        if min_val: acc["min"] = min_val
        if max_val: acc["max"] = max_val
        self.accessors.append(acc)
        return len(self.accessors) - 1

    def write(self,
              positions: np.ndarray,      # float32 [N, 3]
              indices: np.ndarray,        # uint32 [M]
              slave: BakedSlaveMapping,
              output_path: str):
        """
        Writes a valid .glb with baked slave mapping.
        positions + indices = original high-poly geometry
        slave = the GPU-computed mapping
        """
        assert positions.dtype == np.float32
        assert indices.dtype == np.uint32
        assert slave.triangle_idx.dtype == np.uint32
        assert slave.uvw.dtype == np.float32

        N = len(positions)

        # === 1. POSITION (existing geometry) ===
        pos_bytes = positions.tobytes()
        pos_bv = self._add_buffer_view(pos_bytes, target=34962)  # ARRAY_BUFFER
        pos_acc = self._add_accessor(
            pos_bv, 5126, N, "VEC3",
            min_val=positions.min(axis=0).tolist(),
            max_val=positions.max(axis=0).tolist()
        )

        # === 2. INDICES ===
        idx_bytes = indices.tobytes()
        idx_bv = self._add_buffer_view(idx_bytes, target=34963)  # ELEMENT_ARRAY_BUFFER
        idx_acc = self._add_accessor(idx_bv, 5125, len(indices), "SCALAR")

        # === 3. Custom Slave Attributes (the bonus) ===
        tri_idx_bytes = slave.triangle_idx.tobytes()
        tri_bv = self._add_buffer_view(tri_idx_bytes)
        tri_acc = self._add_accessor(tri_bv, 5125, N, "SCALAR")   # uint32

        uvw_bytes = slave.uvw.tobytes()
        uvw_bv = self._add_buffer_view(uvw_bytes)
        uvw_acc = self._add_accessor(uvw_bv, 5126, N, "VEC3")

        # === Mesh primitive with custom attributes ===
        primitive = {
            "attributes": {
                "POSITION": pos_acc,
                "_SLAVE_TRIANGLE_IDX": tri_acc,
                "_SLAVE_BARY_UVW": uvw_acc,
            },
            "indices": idx_acc,
            "mode": 4  # TRIANGLES
        }

        mesh = {"primitives": [primitive]}
        self.meshes.append(mesh)

        # === Build glTF JSON ===
        gltf = {
            "asset": {"version": "2.0", "generator": "ZKAEDI GPU-Native Rigger"},
            "buffers": [{"byteLength": len(self.buffer)}],
            "bufferViews": self.buffer_views,
            "accessors": self.accessors,
            "meshes": self.meshes,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0}]
        }

        json_bytes = json.dumps(gltf, separators=(',', ':')).encode('utf-8')
        json_padded = self._align(len(json_bytes))
        json_bytes += b' ' * (json_padded - len(json_bytes))

        # === Write GLB ===
        total_length = 12 + 8 + len(json_bytes) + 8 + len(self.buffer)

        with open(output_path, "wb") as f:
            # GLB Header
            f.write(struct.pack("<4sII", b"glTF", 2, total_length))
            # JSON chunk
            f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
            f.write(json_bytes)
            # BIN chunk
            f.write(struct.pack("<I4s", len(self.buffer), b"BIN\0"))
            f.write(self.buffer)

        print(f"[ZKAEDI] Baked GLB written → {output_path}")
        print(f"         Custom attributes: _SLAVE_TRIANGLE_IDX + _SLAVE_BARY_UVW")
        print(f"         Fully compatible with your C-native parser.")
