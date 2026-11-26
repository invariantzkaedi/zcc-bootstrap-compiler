"""Zero-dependency Python ctypes bindings for Track 21A Portable Verified Compute.

Compatible with sdk/zcc_client.py (re-exports ZccClient and ZccApiError).
"""

from __future__ import annotations

import ctypes
import json
import os
from typing import Dict, List, Optional, Tuple

from sdk.zcc_client import ZccApiError, ZccClient  # noqa: F401

ABI_VERSION = 0x00010000

DTYPE_F32 = 1
DTYPE_F16 = 2
DTYPE_BF16 = 3
DTYPE_Q8_0 = 4
DTYPE_Q4_0 = 5
DTYPE_Q4_K = 6
DTYPE_U8 = 7
DTYPE_I32 = 8

OWNER_BORROWED_HOST = 0x01
OWNER_OWNED_HOST = 0x02
OWNER_READONLY = 0x04

OP_VEC_ADD_F32 = 1
OP_VEC_MUL_F32 = 2
OP_VEC_FMA_F32 = 3
OP_DOT_F32 = 4
OP_GEMM_F32 = 5
OP_RMSNORM_F32 = 6
OP_SOFTMAX_F32 = 7
OP_DEQUANT_Q8_0 = 8
OP_MATVEC_Q8_0 = 9
OP_DEQUANT_Q4_0 = 10
OP_MATVEC_Q4_0 = 11
OP_DEQUANT_Q4_K = 12
OP_MATVEC_Q4_K = 13

BACKEND_AUTO = 0
BACKEND_SCALAR_C99 = 1
BACKEND_AVX2_FMA = 2
BACKEND_PTX_SM120 = 3

POLICY_EXACT_REQUIRED = 1
POLICY_AUTO_FALLBACK = 2

ACCUM_FP32_FMA = 1
ACCUM_FP64_REF = 2
ACCUM_STRICT_FP32 = 3

DET_FIXED_REDUCTION = 1
DET_RELAXED = 2


class ZccTensorDesc(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("dtype", ctypes.c_uint32),
        ("rank", ctypes.c_uint32),
        ("ownership_flags", ctypes.c_uint32),
        ("shape", ctypes.c_int64 * 4),
        ("strides", ctypes.c_int64 * 4),
        ("data", ctypes.c_void_p),
        ("nbytes", ctypes.c_uint64),
        ("alignment", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
    ]


class ZccCpuCaps(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("max_basic_leaf", ctypes.c_uint32),
        ("has_cpuid", ctypes.c_uint32),
        ("has_sse2", ctypes.c_uint32),
        ("has_osxsave", ctypes.c_uint32),
        ("xcr0_xmm", ctypes.c_uint32),
        ("xcr0_ymm", ctypes.c_uint32),
        ("has_avx", ctypes.c_uint32),
        ("has_f16c", ctypes.c_uint32),
        ("has_fma", ctypes.c_uint32),
        ("has_avx2", ctypes.c_uint32),
        ("has_avx512f", ctypes.c_uint32),
        ("xcr0_raw", ctypes.c_uint64),
    ]


class ZccPtxCaps(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("cuda_available", ctypes.c_uint32),
        ("driver_version", ctypes.c_int32),
        ("device_count", ctypes.c_int32),
        ("cc_major", ctypes.c_int32),
        ("cc_minor", ctypes.c_int32),
        ("vram_bytes", ctypes.c_uint64),
        ("device_name", ctypes.c_char * 128),
    ]


class ZccExecOptions(ctypes.Structure):
    _fields_ = [
        ("abi_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("requested_backend", ctypes.c_uint32),
        ("selection_policy", ctypes.c_uint32),
        ("accum_policy", ctypes.c_uint32),
        ("determinism_mode", ctypes.c_uint32),
        ("seed", ctypes.c_uint64),
        ("rmsnorm_eps", ctypes.c_float),
        ("emit_receipt", ctypes.c_uint32),
        ("compiler_id", ctypes.c_char_p),
        ("compiler_sha256_hex", ctypes.c_char_p),
        ("executable_sha256_hex", ctypes.c_char_p),
        ("ptx_path_override", ctypes.c_char_p),
        ("hmac_key", ctypes.c_void_p),
        ("hmac_key_len", ctypes.c_size_t),
        ("alloc_fn", ctypes.c_void_p),
        ("free_fn", ctypes.c_void_p),
        ("alloc_ctx", ctypes.c_void_p),
    ]


class ZccComputeReceipt(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("abi_version", ctypes.c_uint32),
        ("schema_version", ctypes.c_char * 32),
        ("contract_version", ctypes.c_char * 16),
        ("compiler_id", ctypes.c_char * 64),
        ("compiler_sha256_hex", ctypes.c_char * 65),
        ("executable_sha256_hex", ctypes.c_char * 65),
        ("ptx_module_sha256_hex", ctypes.c_char * 65),
        ("backend_requested", ctypes.c_uint32),
        ("backend_executed", ctypes.c_uint32),
        ("selection_policy", ctypes.c_uint32),
        ("fallback_occurred", ctypes.c_uint32),
        ("selection_reason", ctypes.c_char * 128),
        ("cpu_caps", ZccCpuCaps),
        ("ptx_caps", ZccPtxCaps),
        ("op_id", ctypes.c_uint32),
        ("primary_dtype", ctypes.c_uint32),
        ("accum_policy", ctypes.c_uint32),
        ("determinism_mode", ctypes.c_uint32),
        ("seed", ctypes.c_uint64),
        ("input_count", ctypes.c_uint32),
        ("dim_m", ctypes.c_int64),
        ("dim_n", ctypes.c_int64),
        ("dim_k", ctypes.c_int64),
        ("inputs_descriptor_sha256", ctypes.c_char * 65),
        ("input_buffers_sha256", ctypes.c_char * 65),
        ("output_descriptor_sha256", ctypes.c_char * 65),
        ("output_buffer_sha256", ctypes.c_char * 65),
        ("status_code", ctypes.c_int32),
        ("execution_status", ctypes.c_char * 32),
        ("error_message", ctypes.c_char * 192),
        ("verification_status", ctypes.c_char * 32),
        ("integrity_kind", ctypes.c_char * 40),
        ("zk_proof_present", ctypes.c_uint32),
        ("elapsed_ns", ctypes.c_uint64),
        ("receipt_canonical_sha256", ctypes.c_char * 65),
        ("hmac_sha256_hex", ctypes.c_char * 65),
    ]


class ZccComputeRuntime:
    """Thin ctypes wrapper around Track 21A shared library (`libzcc_compute.so`)."""

    def __init__(self, lib_path: str = "/tmp/libzcc_compute.so") -> None:
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"Track 21A library not found at {lib_path}")
        self.lib_path = lib_path
        self.lib = ctypes.CDLL(lib_path)

        self.lib.zcc_exec_options_init.argtypes = [ctypes.POINTER(ZccExecOptions)]
        self.lib.zcc_exec_options_init.restype = None

        self.lib.zcc_probe_cpu_caps.argtypes = [ctypes.POINTER(ZccCpuCaps)]
        self.lib.zcc_probe_cpu_caps.restype = ctypes.c_int

        self.lib.zcc_probe_ptx_caps.argtypes = [ctypes.POINTER(ZccPtxCaps), ctypes.c_char_p]
        self.lib.zcc_probe_ptx_caps.restype = ctypes.c_int

        self.lib.zcc_compute_exec.argtypes = [
            ctypes.c_int,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.POINTER(ZccExecOptions),
            ctypes.POINTER(ZccComputeReceipt),
        ]
        self.lib.zcc_compute_exec.restype = ctypes.c_int

        self.lib.zcc_compute_exec_with_caps.argtypes = [
            ctypes.c_int,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.POINTER(ZccExecOptions),
            ctypes.POINTER(ZccCpuCaps),
            ctypes.POINTER(ZccComputeReceipt),
        ]
        self.lib.zcc_compute_exec_with_caps.restype = ctypes.c_int

        self.lib.zcc_receipt_to_json.argtypes = [
            ctypes.POINTER(ZccComputeReceipt),
            ctypes.c_char_p,
            ctypes.c_size_t,
        ]
        self.lib.zcc_receipt_to_json.restype = ctypes.c_int

        self.lib.zcc_receipt_verify_canonical.argtypes = [
            ctypes.POINTER(ZccComputeReceipt),
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        self.lib.zcc_receipt_verify_canonical.restype = ctypes.c_int

        self.lib.zcc_ptx_shutdown.argtypes = []
        self.lib.zcc_ptx_shutdown.restype = None

    def probe_caps(self, ptx_path_override: Optional[str] = None) -> Tuple[ZccCpuCaps, ZccPtxCaps]:
        cpu = ZccCpuCaps()
        ptx = ZccPtxCaps()
        self.lib.zcc_probe_cpu_caps(ctypes.byref(cpu))
        self.lib.zcc_probe_ptx_caps(ctypes.byref(ptx), ptx_path_override.encode("utf-8") if ptx_path_override else None)
        return cpu, ptx

    def receipt_dict(self, receipt: ZccComputeReceipt) -> Dict[str, object]:
        buf = ctypes.create_string_buffer(8192)
        n = self.lib.zcc_receipt_to_json(ctypes.byref(receipt), buf, len(buf))
        if n <= 0:
            raise RuntimeError("zcc_receipt_to_json failed")
        return json.loads(buf.value.decode("utf-8"))

    def verify_receipt(self, receipt: ZccComputeReceipt, hmac_key: Optional[bytes] = None) -> int:
        if hmac_key:
            key_buf = ctypes.create_string_buffer(hmac_key, len(hmac_key))
            return int(self.lib.zcc_receipt_verify_canonical(ctypes.byref(receipt), ctypes.cast(key_buf, ctypes.c_void_p), len(hmac_key)))
        return int(self.lib.zcc_receipt_verify_canonical(ctypes.byref(receipt), None, 0))

    def shutdown(self) -> None:
        self.lib.zcc_ptx_shutdown()
