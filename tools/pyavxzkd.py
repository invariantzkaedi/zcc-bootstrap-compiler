"""
pyavxzkd — High-Performance Python C-ABI Wrapper for AVXzkd SUPREME
===================================================================
Hardware-vectorized Hamiltonian field dynamics, topological curvature,
two-regime scar navigation, and Layer-1 Quantum DTQW engine.
"""

import os
import sys
import math
import struct
import hashlib
import ctypes
import numpy as np
from typing import Tuple, List, Optional, Dict, Any

# Locate shared library
_LIB_NAME = "libavxzkd.so" if sys.platform != "win32" else "libavxzkd.dll"
_POSSIBLE_PATHS = [
    os.path.join(os.path.dirname(__file__), "..", _LIB_NAME),
    os.path.join(os.path.dirname(__file__), _LIB_NAME),
    os.path.join(os.getcwd(), _LIB_NAME),
    f"/tmp/{_LIB_NAME}",
]

_LIB = None
for p in _POSSIBLE_PATHS:
    if os.path.exists(p):
        try:
            _LIB = ctypes.CDLL(p)
            break
        except Exception:
            pass

class _AvxzkdParams(ctypes.Structure):
    _fields_ = [
        ("eta", ctypes.c_float),
        ("gamma", ctypes.c_float),
        ("beta", ctypes.c_float),
        ("eps", ctypes.c_float),
        ("kick", ctypes.c_float),
        ("kappa", ctypes.c_float),
        ("momentum", ctypes.c_float),
        ("seed", ctypes.c_uint64 * 4),
    ]

class _AvxzkdField(ctypes.Structure):
    _fields_ = [
        ("base", ctypes.POINTER(ctypes.c_float)),
        ("current", ctypes.POINTER(ctypes.c_float)),
        ("scars", ctypes.POINTER(ctypes.c_float)),
        ("curvature", ctypes.POINTER(ctypes.c_float)),
        ("hessian_det", ctypes.POINTER(ctypes.c_float)),
        ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32),
        ("stride", ctypes.c_uint32),
        ("total_cells", ctypes.c_uint32),
        ("tripwires", ctypes.c_uint32),
    ]

class _AvxzkdAudit(ctypes.Structure):
    _fields_ = [
        ("l_inf_error", ctypes.c_float),
        ("mean_error", ctypes.c_float),
        ("measured_gain", ctypes.c_float),
        ("floor_drift", ctypes.c_float),
        ("lyapunov_exponent", ctypes.c_float),
        ("state_digest", ctypes.c_uint64),
        ("pass_all_invariants", ctypes.c_bool),
    ]

class _AvxzkdWalker(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_int32),
        ("y", ctypes.c_int32),
        ("prev_dx", ctypes.c_int32),
        ("prev_dy", ctypes.c_int32),
        ("target_x", ctypes.c_int32),
        ("target_y", ctypes.c_int32),
        ("steps_taken", ctypes.c_uint32),
        ("backtracks", ctypes.c_uint32),
        ("solved", ctypes.c_bool),
        ("path_x", ctypes.POINTER(ctypes.c_int32)),
        ("path_y", ctypes.POINTER(ctypes.c_int32)),
        ("path_len", ctypes.c_uint32),
        ("capacity", ctypes.c_uint32),
    ]

class _AvxzkdDtqw(ctypes.Structure):
    _fields_ = [
        ("real0", ctypes.c_float * 16),
        ("imag0", ctypes.c_float * 16),
        ("real1", ctypes.c_float * 16),
        ("imag1", ctypes.c_float * 16),
        ("node_probs", ctypes.c_double * 16),
        ("node_phases", ctypes.c_double * 16),
        ("s_q0", ctypes.c_double),
        ("coherence", ctypes.c_double),
        ("total_steps", ctypes.c_uint32),
    ]

def _bind_library(lib):
    lib.avxzkd_create.argtypes = [ctypes.c_uint32, ctypes.c_uint32]
    lib.avxzkd_create.restype = ctypes.POINTER(_AvxzkdField)

    lib.avxzkd_init.argtypes = [ctypes.POINTER(_AvxzkdField), ctypes.POINTER(ctypes.c_float)]
    lib.avxzkd_init.restype = ctypes.c_int

    lib.avxzkd_destroy.argtypes = [ctypes.POINTER(_AvxzkdField)]
    lib.avxzkd_destroy.restype = None

    lib.avxzkd_get_cpu_features.argtypes = []
    lib.avxzkd_get_cpu_features.restype = ctypes.c_uint32

    lib.avxzkd_deep_recurse_auto.argtypes = [ctypes.POINTER(_AvxzkdField), ctypes.POINTER(_AvxzkdParams), ctypes.c_uint32]
    lib.avxzkd_deep_recurse_auto.restype = ctypes.c_int

    lib.avxzkd_deep_recurse_parallel.argtypes = [ctypes.POINTER(_AvxzkdField), ctypes.POINTER(_AvxzkdParams), ctypes.c_uint32, ctypes.c_uint32]
    lib.avxzkd_deep_recurse_parallel.restype = ctypes.c_int

    lib.avxzkd_compute_topology_avx2.argtypes = [ctypes.POINTER(_AvxzkdField)]
    lib.avxzkd_compute_topology_avx2.restype = ctypes.c_int

    lib.avxzkd_couple_fields_avx2.argtypes = [ctypes.POINTER(_AvxzkdField), ctypes.POINTER(_AvxzkdField), ctypes.c_float]
    lib.avxzkd_couple_fields_avx2.restype = ctypes.c_int

    lib.avxzkd_audit.argtypes = [ctypes.POINTER(_AvxzkdField), ctypes.POINTER(_AvxzkdParams), ctypes.POINTER(_AvxzkdAudit)]
    lib.avxzkd_audit.restype = ctypes.c_int

    lib.avxzkd_walker_create.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_uint32]
    lib.avxzkd_walker_create.restype = ctypes.POINTER(_AvxzkdWalker)

    lib.avxzkd_walker_solve.argtypes = [ctypes.POINTER(_AvxzkdWalker), ctypes.POINTER(_AvxzkdField), ctypes.POINTER(_AvxzkdParams), ctypes.c_uint32]
    lib.avxzkd_walker_solve.restype = ctypes.c_int32

    lib.avxzkd_walker_destroy.argtypes = [ctypes.POINTER(_AvxzkdWalker)]
    lib.avxzkd_walker_destroy.restype = None

    lib.avxzkd_dtqw_create.argtypes = []
    lib.avxzkd_dtqw_create.restype = ctypes.POINTER(_AvxzkdDtqw)

    lib.avxzkd_dtqw_step_auto.argtypes = [ctypes.POINTER(_AvxzkdDtqw), ctypes.c_uint32]
    lib.avxzkd_dtqw_step_auto.restype = ctypes.c_int

    lib.avxzkd_dtqw_dephase.argtypes = [ctypes.POINTER(_AvxzkdDtqw), ctypes.c_float]
    lib.avxzkd_dtqw_dephase.restype = ctypes.c_int

    lib.avxzkd_dtqw_destroy.argtypes = [ctypes.POINTER(_AvxzkdDtqw)]
    lib.avxzkd_dtqw_destroy.restype = None

if _LIB is not None:
    _bind_library(_LIB)


class PyAvxzkdField:
    """Pythonic, high-speed interface to the AVXzkd SIMD field engine."""

    def __init__(self, width: int, height: int, lib: Optional[Any] = None):
        global _LIB
        self._lib = lib or _LIB
        if self._lib is None:
            raise RuntimeError("libavxzkd shared library not found. Build with 'make libavxzkd.so'.")
        self.width = width
        self.height = height
        self._field_ptr = self._lib.avxzkd_create(width, height)
        if not self._field_ptr:
            raise MemoryError("Failed to allocate 64-byte aligned AVXzkd field.")

    def __del__(self):
        if hasattr(self, "_field_ptr") and self._field_ptr and self._lib:
            self._lib.avxzkd_destroy(self._field_ptr)
            self._field_ptr = None

    def init_field(self, data: np.ndarray) -> None:
        """Initializes the potential field with a 2D float32 NumPy array."""
        arr = np.ascontiguousarray(data, dtype=np.float32)
        if arr.shape[0] != self.height or arr.shape[1] != self.width:
            raise ValueError(f"Shape {arr.shape} does not match field size ({self.height}, {self.width})")
        ptr = arr.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        res = self._lib.avxzkd_init(self._field_ptr, ptr)
        if res != 0:
            raise RuntimeError(f"avxzkd_init failed with code {res}")

    def step(self, eta: float = 0.4, gamma: float = 0.3, beta: float = 0.1,
             eps: float = 0.05, kick: float = 2.0, momentum: float = 0.25,
             k_steps: int = 1) -> None:
        """Executes k recursive steps of vectorized field evolution."""
        params = _AvxzkdParams(
            eta=eta, gamma=gamma, beta=beta, eps=eps,
            kick=kick, kappa=0.0, momentum=momentum,
            seed=(ctypes.c_uint64 * 4)(0x12345678, 0x9abcdef0, 0x13579bdf, 0x2468ace0)
        )
        res = self._lib.avxzkd_deep_recurse_auto(self._field_ptr, ctypes.byref(params), k_steps)
        if res != 0:
            raise RuntimeError(f"avxzkd_deep_recurse failed with code {res}")

    def step_parallel(self, eta: float = 0.4, gamma: float = 0.3, beta: float = 0.1,
                      eps: float = 0.05, kick: float = 2.0, momentum: float = 0.25,
                      k_steps: int = 1, threads: int = 0) -> None:
        """Executes k recursive steps using multi-threaded OpenMP parallelism."""
        params = _AvxzkdParams(
            eta=eta, gamma=gamma, beta=beta, eps=eps,
            kick=kick, kappa=0.0, momentum=momentum,
            seed=(ctypes.c_uint64 * 4)(0x12345678, 0x9abcdef0, 0x13579bdf, 0x2468ace0)
        )
        res = self._lib.avxzkd_deep_recurse_parallel(self._field_ptr, ctypes.byref(params), k_steps, threads)
        if res != 0:
            raise RuntimeError(f"avxzkd_deep_recurse_parallel failed with code {res}")

    def compute_topology(self) -> None:
        """Computes vectorized Laplacian and Hessian curvature stencils."""
        res = self._lib.avxzkd_compute_topology_avx2(self._field_ptr)
        if res != 0:
            raise RuntimeError(f"avxzkd_compute_topology failed with code {res}")

    def get_current(self) -> np.ndarray:
        """Returns the active potential field as a 2D NumPy array."""
        f = self._field_ptr.contents
        buf = np.ctypeslib.as_array(f.current, shape=(f.height, f.stride))
        return buf[:, :self.width].copy()

    def get_curvature(self) -> np.ndarray:
        """Returns the Laplacian curvature field as a 2D NumPy array."""
        f = self._field_ptr.contents
        buf = np.ctypeslib.as_array(f.curvature, shape=(f.height, f.stride))
        return buf[:, :self.width].copy()

    def get_scars(self) -> np.ndarray:
        """Returns the scar departure memory field as a 2D NumPy array."""
        f = self._field_ptr.contents
        buf = np.ctypeslib.as_array(f.scars, shape=(f.height, f.stride))
        return buf[:, :self.width].copy()

    def audit(self, eta: float = 0.4, gamma: float = 0.3) -> Dict[str, Any]:
        """Runs the invariant mathematical audit on the current field state."""
        params = _AvxzkdParams(eta=eta, gamma=gamma, beta=0.1, eps=0.0, kick=2.0, kappa=0.0, momentum=0.25)
        audit_res = _AvxzkdAudit()
        res = self._lib.avxzkd_audit(self._field_ptr, ctypes.byref(params), ctypes.byref(audit_res))
        if res != 0:
            raise RuntimeError(f"avxzkd_audit failed with code {res}")
        return {
            "measured_gain": audit_res.measured_gain,
            "floor_drift": audit_res.floor_drift,
            "state_digest": hex(audit_res.state_digest),
            "pass_all_invariants": audit_res.pass_all_invariants,
        }

    def solve_walker(self, start: Tuple[int, int], target: Tuple[int, int],
                     kick: float = 2.0, momentum: float = 0.25, max_steps: int = 50000) -> Dict[str, Any]:
        """Solves navigation from start to target using the two-regime scar walker."""
        params = _AvxzkdParams(eta=0.4, gamma=0.3, beta=0.1, eps=0.05, kick=kick, kappa=0.0, momentum=momentum)
        w_ptr = self._lib.avxzkd_walker_create(start[0], start[1], target[0], target[1], max_steps)
        if not w_ptr:
            raise MemoryError("Failed to allocate AVXzkd walker.")

        try:
            steps = self._lib.avxzkd_walker_solve(w_ptr, self._field_ptr, ctypes.byref(params), max_steps)
            w = w_ptr.contents
            path_x = np.ctypeslib.as_array(w.path_x, shape=(w.path_len,)).tolist()
            path_y = np.ctypeslib.as_array(w.path_y, shape=(w.path_len,)).tolist()
            path = list(zip(path_x, path_y))
            return {
                "solved": w.solved,
                "total_steps": steps,
                "path_len": w.path_len,
                "path": path,
            }
        finally:
            self._lib.avxzkd_walker_destroy(w_ptr)


class PyAvxzkdQuantumWalk:
    """Vectorized Discrete-Time Quantum Walk (DTQW) Engine for Trinity Layer 1."""

    def __init__(self, lib: Optional[Any] = None):
        global _LIB
        self._lib = lib or _LIB
        if self._lib is None:
            raise RuntimeError("libavxzkd shared library not found. Build with 'make libavxzkd.so'.")
        self._qw_ptr = self._lib.avxzkd_dtqw_create()
        if not self._qw_ptr:
            raise MemoryError("Failed to allocate AVXzkd DTQW state.")

    def __del__(self):
        if hasattr(self, "_qw_ptr") and self._qw_ptr and self._lib:
            self._lib.avxzkd_dtqw_destroy(self._qw_ptr)
            self._qw_ptr = None

    def step(self, steps: int = 1) -> None:
        """Executes n vectorized Hadamard coin & spatial shift steps."""
        res = self._lib.avxzkd_dtqw_step_auto(self._qw_ptr, steps)
        if res != 0:
            raise RuntimeError(f"avxzkd_dtqw_step failed with code {res}")

    def dephase(self, gamma_dephase: float = 0.05) -> None:
        """Applies Lindblad environmental dephasing / decoherence."""
        res = self._lib.avxzkd_dtqw_dephase(self._qw_ptr, gamma_dephase)
        if res != 0:
            raise RuntimeError(f"avxzkd_dtqw_dephase failed with code {res}")

    def get_probabilities(self) -> np.ndarray:
        """Returns 16 spatial node Born probabilities P(x)."""
        qw = self._qw_ptr.contents
        return np.ctypeslib.as_array(qw.node_probs, shape=(16,)).copy()

    def get_phases(self) -> np.ndarray:
        """Returns 16 spatial node phase angles theta(x)."""
        qw = self._qw_ptr.contents
        return np.ctypeslib.as_array(qw.node_phases, shape=(16,)).copy()

    def get_entanglement_entropy(self) -> float:
        """Returns coin subsystem entanglement entropy S(q0)."""
        return float(self._qw_ptr.contents.s_q0)

    def get_coherence(self) -> float:
        """Returns quantum purity / coherence index."""
        return float(self._qw_ptr.contents.coherence)

    def get_public_commitment(self) -> Tuple[bytes, str]:
        """Packs the canonical 296-byte public commitment layout for SP1 / Yul settlement."""
        probs = self.get_probabilities()
        phases = self.get_phases()
        s_q0 = self.get_entanglement_entropy()

        payload = bytearray()
        for p in probs:
            payload.extend(struct.pack("<d", float(p)))
        for phi in phases:
            payload.extend(struct.pack("<d", float(phi)))
        payload.extend(struct.pack("<d", float(s_q0)))

        assert len(payload) == 264, f"Payload size must be 264 bytes, got {len(payload)}"
        digest = hashlib.sha256(payload).digest()
        full_commitment = payload + digest
        return bytes(full_commitment), digest.hex()
