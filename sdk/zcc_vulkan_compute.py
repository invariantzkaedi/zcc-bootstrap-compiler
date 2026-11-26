"""ZCC Track 21B — Vulkan & SPIR-V Compute SDK (backward-compatible extension of sdk/zcc_compute.py)."""
from __future__ import annotations

import ctypes
from typing import Any, Dict, Optional

from sdk.zcc_client import ZccApiError, ZccClient  # noqa: F401
from sdk.zcc_compute import (  # noqa: F401
    ACCUM_FP32_FMA,
    ACCUM_FP64_REF,
    ACCUM_STRICT_FP32,
    BACKEND_AUTO,
    BACKEND_AVX2_FMA,
    BACKEND_PTX_SM120,
    BACKEND_SCALAR_C99,
    DET_FIXED_REDUCTION,
    DET_RELAXED,
    DTYPE_F32,
    DTYPE_Q4_0,
    DTYPE_Q4_K,
    DTYPE_Q8_0,
    OP_DEQUANT_Q4_0,
    OP_DEQUANT_Q4_K,
    OP_DEQUANT_Q8_0,
    OP_DOT_F32,
    OP_GEMM_F32,
    OP_MATVEC_Q4_0,
    OP_MATVEC_Q4_K,
    OP_MATVEC_Q8_0,
    OP_RMSNORM_F32,
    OP_SOFTMAX_F32,
    OP_VEC_ADD_F32,
    OP_VEC_FMA_F32,
    OP_VEC_MUL_F32,
    OWNER_BORROWED_HOST,
    OWNER_READONLY,
    POLICY_AUTO_FALLBACK,
    POLICY_EXACT_REQUIRED,
    ZccComputeReceipt,
    ZccComputeRuntime,
    ZccCpuCaps,
    ZccExecOptions,
    ZccPtxCaps,
    ZccTensorDesc,
)

ABI_V2_VERSION = 0x00010100
BACKEND_VULKAN_SPV = 4
BACKEND_VULKAN_GPU = 5

VK_DEVTYPE_OTHER = 0
VK_DEVTYPE_INTEGRATED_GPU = 1
VK_DEVTYPE_DISCRETE_GPU = 2
VK_DEVTYPE_VIRTUAL_GPU = 3
VK_DEVTYPE_CPU = 4


class ZccVulkanCaps(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("vulkan_available", ctypes.c_uint32),
        ("validation_layer_available", ctypes.c_uint32),
        ("validation_layer_enabled", ctypes.c_uint32),
        ("physical_device_count", ctypes.c_uint32),
        ("selected_device_index", ctypes.c_uint32),
        ("physical_device_type", ctypes.c_uint32),
        ("api_version", ctypes.c_uint32),
        ("driver_version", ctypes.c_uint32),
        ("vendor_id", ctypes.c_uint32),
        ("device_id", ctypes.c_uint32),
        ("compute_queue_family", ctypes.c_uint32),
        ("has_compute_queue", ctypes.c_uint32),
        ("max_push_constants_size", ctypes.c_uint32),
        ("max_compute_shared_mem", ctypes.c_uint32),
        ("max_compute_invocations", ctypes.c_uint32),
        ("max_workgroup_size", ctypes.c_uint32 * 3),
        ("max_workgroup_count", ctypes.c_uint32 * 3),
        ("max_storage_buffer_range", ctypes.c_uint64),
        ("non_coherent_atom_size", ctypes.c_uint64),
        ("host_coherent_mem_available", ctypes.c_uint32),
        ("float16_int8_supported", ctypes.c_uint32),
        ("device_name", ctypes.c_char * 256),
        ("device_type_str", ctypes.c_char * 32),
    ]


class ZccVulkanExecOptions(ctypes.Structure):
    _fields_ = [
        ("base", ZccExecOptions),
        ("abi_v2_version", ctypes.c_uint32),
        ("struct_size", ctypes.c_uint32),
        ("require_validation_layer", ctypes.c_uint32),
        ("require_hardware_gpu", ctypes.c_uint32),
        ("spirv_override_words", ctypes.POINTER(ctypes.c_uint32)),
        ("spirv_override_word_count", ctypes.c_size_t),
    ]


class ZccComputeV2Receipt(ctypes.Structure):
    _fields_ = [
        ("base", ZccComputeReceipt),
        ("v2_struct_size", ctypes.c_uint32),
        ("v2_abi_version", ctypes.c_uint32),
        ("v2_schema_version", ctypes.c_char * 32),
        ("v2_contract_version", ctypes.c_char * 16),
        ("vulkan_caps", ZccVulkanCaps),
        ("spirv_module_sha256_hex", ctypes.c_char * 65),
        ("spirv_word_count", ctypes.c_uint32),
        ("sync_barrier_count", ctypes.c_uint32),
        ("mem_flush_count", ctypes.c_uint32),
        ("mem_invalidate_count", ctypes.c_uint32),
        ("validation_error_count", ctypes.c_uint32),
        ("v2_verification_status", ctypes.c_char * 32),
        ("v2_integrity_kind", ctypes.c_char * 40),
        ("v2_zk_proof_present", ctypes.c_uint32),
        ("v2_canonical_sha256_hex", ctypes.c_char * 65),
        ("v2_hmac_sha256_hex", ctypes.c_char * 65),
    ]


class ZccVulkanComputeRuntime(ZccComputeRuntime):
    """Extended Track 21B ctypes runtime binding for /tmp/libzcc_compute_v2.so."""

    def __init__(self, lib_path: str = "/tmp/libzcc_compute_v2.so") -> None:
        super().__init__(lib_path)
        self.lib.zcc_vulkan_exec_options_init.argtypes = [ctypes.POINTER(ZccVulkanExecOptions)]
        self.lib.zcc_vulkan_exec_options_init.restype = None

        self.lib.zcc_probe_vulkan_caps.argtypes = [ctypes.POINTER(ZccVulkanCaps), ctypes.c_int]
        self.lib.zcc_probe_vulkan_caps.restype = ctypes.c_int32

        self.lib.zcc_compute_exec_v2.argtypes = [
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.POINTER(ZccVulkanExecOptions),
            ctypes.POINTER(ZccComputeV2Receipt),
        ]
        self.lib.zcc_compute_exec_v2.restype = ctypes.c_int32

        self.lib.zcc_compute_exec_v2_with_caps.argtypes = [
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.POINTER(ZccVulkanExecOptions),
            ctypes.POINTER(ZccCpuCaps),
            ctypes.POINTER(ZccVulkanCaps),
            ctypes.POINTER(ZccComputeV2Receipt),
        ]
        self.lib.zcc_compute_exec_v2_with_caps.restype = ctypes.c_int32

        self.lib.zcc_receipt_v2_to_json.argtypes = [
            ctypes.POINTER(ZccComputeV2Receipt),
            ctypes.c_char_p,
            ctypes.c_size_t,
        ]
        self.lib.zcc_receipt_v2_to_json.restype = ctypes.c_int

        self.lib.zcc_receipt_v2_verify_canonical.argtypes = [
            ctypes.POINTER(ZccComputeV2Receipt),
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
        ]
        self.lib.zcc_receipt_v2_verify_canonical.restype = ctypes.c_int32

        self.lib.zcc_vulkan_ctx_create.argtypes = [
            ctypes.c_int,
            ctypes.c_uint32,
            ctypes.POINTER(ctypes.c_void_p),
        ]
        self.lib.zcc_vulkan_ctx_create.restype = ctypes.c_int32

        self.lib.zcc_vulkan_ctx_destroy.argtypes = [ctypes.c_void_p]
        self.lib.zcc_vulkan_ctx_destroy.restype = None

        self.lib.zcc_vulkan_ctx_get_caps.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ZccVulkanCaps),
        ]
        self.lib.zcc_vulkan_ctx_get_caps.restype = ctypes.c_int32

        self.lib.zcc_compute_exec_v2_with_ctx.argtypes = [
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.c_uint32,
            ctypes.POINTER(ZccTensorDesc),
            ctypes.POINTER(ZccVulkanExecOptions),
            ctypes.POINTER(ZccCpuCaps),
            ctypes.POINTER(ZccVulkanCaps),
            ctypes.c_void_p,
            ctypes.POINTER(ZccComputeV2Receipt),
        ]
        self.lib.zcc_compute_exec_v2_with_ctx.restype = ctypes.c_int32

    def probe_vulkan(self, enable_validation_layer: bool = True) -> ZccVulkanCaps:
        caps = ZccVulkanCaps()
        self.lib.zcc_probe_vulkan_caps(ctypes.byref(caps), 1 if enable_validation_layer else 0)
        return caps

    def create_context(
        self,
        enable_validation_layer: bool = True,
        require_hardware_gpu: bool = False,
    ) -> "ZccVulkanContext":
        return ZccVulkanContext(
            self,
            enable_validation_layer=enable_validation_layer,
            require_hardware_gpu=require_hardware_gpu,
        )

    def exec_v2(
        self,
        op_id: int,
        inputs: list[ZccTensorDesc],
        output: ZccTensorDesc,
        backend: int = BACKEND_VULKAN_SPV,
        policy: int = POLICY_EXACT_REQUIRED,
        accum: int = ACCUM_FP32_FMA,
        require_validation_layer: bool = True,
        require_hardware_gpu: bool = False,
        hmac_key: Optional[bytes] = None,
        restricted_vk_caps: Optional[ZccVulkanCaps] = None,
        ctx: Optional["ZccVulkanContext"] = None,
    ) -> tuple[int, Dict[str, Any], ZccComputeV2Receipt]:
        opts = ZccVulkanExecOptions()
        self.lib.zcc_vulkan_exec_options_init(ctypes.byref(opts))
        opts.base.requested_backend = backend
        opts.base.selection_policy = policy
        opts.base.accum_policy = accum
        opts.require_validation_layer = 1 if require_validation_layer else 0
        opts.require_hardware_gpu = 1 if require_hardware_gpu else 0
        key_buf = None
        if hmac_key:
            key_buf = ctypes.create_string_buffer(hmac_key, len(hmac_key))
            opts.base.hmac_key = ctypes.cast(key_buf, ctypes.c_void_p)
            opts.base.hmac_key_len = len(hmac_key)

        in_arr = (ZccTensorDesc * len(inputs))(*inputs)
        rec = ZccComputeV2Receipt()
        ctx_ptr = ctx._require_handle() if ctx is not None else None
        rc = self.lib.zcc_compute_exec_v2_with_ctx(
            op_id,
            in_arr,
            len(inputs),
            ctypes.byref(output),
            ctypes.byref(opts),
            None,
            ctypes.byref(restricted_vk_caps) if restricted_vk_caps is not None else None,
            ctx_ptr,
            ctypes.byref(rec),
        )
        return rc, self.receipt_v2_dict(rec), rec

    def receipt_v2_dict(self, rec: ZccComputeV2Receipt) -> Dict[str, Any]:
        import json
        buf = ctypes.create_string_buffer(16384)
        n = self.lib.zcc_receipt_v2_to_json(ctypes.byref(rec), buf, ctypes.sizeof(buf))
        if n <= 0:
            raise RuntimeError("zcc_receipt_v2_to_json failed")
        return json.loads(buf.value.decode("utf-8"))


class ZccVulkanContext:
    """RAII wrapper around persistent zcc_vulkan_ctx_t (cached instance/device/pipelines/staging)."""

    def __init__(
        self,
        runtime: ZccVulkanComputeRuntime,
        enable_validation_layer: bool = True,
        require_hardware_gpu: bool = False,
    ) -> None:
        self._rt = runtime
        self._handle = ctypes.c_void_p()
        rc = self._rt.lib.zcc_vulkan_ctx_create(
            1 if enable_validation_layer else 0,
            1 if require_hardware_gpu else 0,
            ctypes.byref(self._handle),
        )
        if rc != 0 or not self._handle.value:
            self._handle = ctypes.c_void_p()
            raise RuntimeError(f"zcc_vulkan_ctx_create failed rc={rc}")

    def _require_handle(self) -> ctypes.c_void_p:
        if not self._handle or not self._handle.value:
            raise RuntimeError("ZccVulkanContext is closed")
        return self._handle

    def caps(self) -> ZccVulkanCaps:
        out = ZccVulkanCaps()
        rc = self._rt.lib.zcc_vulkan_ctx_get_caps(self._require_handle(), ctypes.byref(out))
        if rc != 0:
            raise RuntimeError(f"zcc_vulkan_ctx_get_caps failed rc={rc}")
        return out

    def exec_v2(
        self,
        op_id: int,
        inputs: list[ZccTensorDesc],
        output: ZccTensorDesc,
        backend: int = BACKEND_VULKAN_SPV,
        policy: int = POLICY_EXACT_REQUIRED,
        accum: int = ACCUM_FP32_FMA,
        require_validation_layer: bool = True,
        require_hardware_gpu: bool = False,
        hmac_key: Optional[bytes] = None,
        restricted_vk_caps: Optional[ZccVulkanCaps] = None,
    ) -> tuple[int, Dict[str, Any], ZccComputeV2Receipt]:
        return self._rt.exec_v2(
            op_id,
            inputs,
            output,
            backend=backend,
            policy=policy,
            accum=accum,
            require_validation_layer=require_validation_layer,
            require_hardware_gpu=require_hardware_gpu,
            hmac_key=hmac_key,
            restricted_vk_caps=restricted_vk_caps,
            ctx=self,
        )

    def close(self) -> None:
        h = getattr(self, "_handle", None)
        if h and h.value:
            self._rt.lib.zcc_vulkan_ctx_destroy(h)
            self._handle = ctypes.c_void_p()

    def __enter__(self) -> "ZccVulkanContext":
        self._require_handle()
        return self

    def __exit__(self, exc_type: Any, exc: Any, tb: Any) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass
