# ZCC Track 21B — Portable SPIR-V and Vulkan Compute Contract (`track21b.compute_contract.v1`)

**Status:** Normative (`ZCC-EF-0`, `ZCC-EF-7`)  
**Track 21A Base ABI Preserved:** `ZCC_COMPUTE_ABI_VERSION = 0x00010000U` ([`include/zcc_compute.h`](include/zcc_compute.h) untouched)  
**Track 21B Extended ABI Version:** `ZCC_COMPUTE_V2_ABI_VERSION = 0x00010100U` ([`include/zcc_vulkan_compute.h`](include/zcc_vulkan_compute.h))  
**Track 21B Receipt Schema:** `"track21b.receipt.v1"` (wrapping `zcc_compute_receipt_t` cleanly)  
**Frozen Predecessor Baseline:** Commit `7e5821daf` / Tag `track21a-v1.0.0-verified`

---

## 1. Epistemic Honesty, Status States & Hardware Separation

1. **Explicit Multi-Valued Gate States:** Every Track 21B gate and verification report MUST distinguish:
   - `PASS`: Independent execution and external oracle verification succeeded.
   - `FAIL`: Predefined invariant or numerical bound violated.
   - `BLOCKED`: Required external independent oracle (`spirv-val`, `VK_LAYER_KHRONOS_validation`, or `libvulkan.so.1`) is missing or policy-blocked on the host. Missing external validator tools NEVER produce `PASS`.
   - `UNKNOWN`: Unregistered anomaly or novel fault signature preserved without guessing.
   - `DEFERRED`: Explicitly out-of-scope or unverified native Windows target (`T21B-GPU-NV-NATIVE`, `T21B-GPU-AMD-NATIVE`) blocked by Smart App Control.
2. **Physical Device Identity Honesty:**
   - Software/CPU Vulkan implementations such as Mesa `llvmpipe` (`VK_PHYSICAL_DEVICE_TYPE_CPU = 4`, `vendor_id = 0x10005`) MUST always be identified as `device_type = "CPU"`.
   - Emulated or CPU-executed Vulkan runs (`llvmpipe`) MUST NEVER satisfy hardware-specific GPU gates (`T21B-GPU-NV-DZN`, `T21B-GPU-AMD-DZN`, `T21B-GPU-NV-NATIVE`, `T21B-GPU-AMD-NATIVE`).
   - Hardware Vulkan verification over Mesa Dozen (`dzn`) -> D3D12 (`/dev/dxg`) MUST explicitly record the Dozen bridge identity, `conformanceVersion 0.0.0.0`, and never claim native proprietary Win64 ICD signoff.
   - Requesting `ZCC_BACKEND_VULKAN_GPU` (`5`) on a host with only `VK_PHYSICAL_DEVICE_TYPE_CPU` MUST fail closed with `ZCC_STATUS_ERR_UNSUPPORTED_HW` under `ZCC_POLICY_EXACT_REQUIRED`, and only fall back under `ZCC_POLICY_AUTO_FALLBACK` with `fallback_occurred = 1` and explicit `selection_reason` (and NEVER fall back to CPU when `require_hardware_gpu = 1`).
3. **Zero Core Bootstrap Drift:**
   - SPIR-V compiler emission (`src/part6_spirv.c`) integrates strictly via `PASSES` in `Makefile`, dedicated CLI (`zcc-spirv`), and shared compute runtime (`libzcc_compute_v2.so`). `PARTS` (`part0_pp.c..part6_ptx.c`) and `BOOTSTRAP_BASELINES.tsv` remain byte-for-byte unchanged.

---

## 2. SPIR-V Binary & Compiler Contract (`T21-G01`)

1. **Target Environment & Capabilities:**
   - SPIR-V binary magic `0x07230203`, version `0x00010000` (SPIR-V `1.0` under Vulkan `1.0` semantics), generator ID `0x5A434300` (`"ZCC\0"`).
   - Required capability: `Capability Shader` (`1`) only. Optional extensions (`Float16`, `Int8`, `8bitStorage`, `16bitStorage`, `StorageBuffer8BitAccess`) MUST NOT be unconditionally required so emitted modules remain portable across Vulkan `1.0..1.4` drivers (`llvmpipe`, NVIDIA, AMD, Intel).
   - Quantized `Q8_0`, `Q4_0`, `Q4_K` decoders MUST implement exact GGUF byte extraction and IEEE-754 `f16 -> f32` conversion directly over `uint32_t` storage buffers (`Uniform` storage class + `BufferBlock` decoration, `ArrayStride 4`) and deterministic `OpLoopMerge` structured control flow.
2. **Canonical Descriptor & Push Constant ABI (`64` bytes <= Vulkan `128B` minimum):**
   - `DescriptorSet 0`:
     - `Binding 0`: `bufA[]` (`uint32_t` storage buffer — primary tensor A or quantized weight blocks)
     - `Binding 1`: `bufB[]` (`uint32_t` storage buffer — secondary tensor B, weights W, or dummy 4B buffer)
     - `Binding 2`: `bufC[]` (`uint32_t` storage buffer — ternary input C or dummy 4B buffer)
     - `Binding 3`: `bufOut[]` (`uint32_t` output tensor words)
   - `PushConstant` block (`16 x uint32_t = 64` bytes):
     `{dim_m, dim_n, dim_k, flags, strideA0, strideA1, strideB0, strideB1, strideC0, strideC1, strideO0, strideO1, offA_bytes, offB_bytes, offC_bytes, offO_bytes_or_eps}` (Wait: `rmsnorm_eps` passed in `flags`/word `15` with aligned offset encoding: dedicated `uint32_t` words verified by `_Static_assert(sizeof(zcc_spv_push_t) == 64)`!).
3. **Deterministic Generation:** Emitting any kernel op `(op_id, local_size)` repeatedly produces bit-for-bit identical `.spv` word streams (`sha256` identical).
4. **Independent Oracle Mandate:** Every generated `.spv` artifact MUST pass `spirv-val --target-env vulkan1.0` (verified external process, never self-certified by internal checks).

---

## 3. Vulkan Physical-Device Negotiation & Explicit Synchronization (`T21-G02`)

1. **Capability Negotiation (`zcc_probe_vulkan_caps` + `zcc_vulkan_negotiate_with_caps`):**
   - Enumerates Vulkan physical devices via `libvulkan.so.1` (`vulkan-1.dll`), inspecting `apiVersion`, `vendorID`, `deviceID`, `deviceType`, compute queue family (`VK_QUEUE_COMPUTE_BIT`), `maxPushConstantsSize >= 64`, `maxStorageBufferRange`, `maxComputeWorkGroupCount`, `maxComputeWorkGroupSize`, `maxComputeWorkGroupInvocations`, `nonCoherentAtomSize`, and host-visible memory type flags.
   - Injecting mocked/restricted `zcc_vulkan_caps_t` cleanly supports negative capability tests (missing loader, `0` compute queues, insufficient `maxPushConstantsSize`, storage buffer size exceeding `maxStorageBufferRange`, CPU-only device rejecting `VULKAN_GPU`).
2. **Explicit Memory Synchronization & Lifetime:**
   - Host writes are made visible to compute shaders explicitly via `vkFlushMappedMemoryRanges` (when not `HOST_COHERENT`) AND `vkCmdPipelineBarrier(VK_PIPELINE_STAGE_HOST_BIT -> VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_HOST_WRITE_BIT -> VK_ACCESS_SHADER_READ_BIT)`.
   - Shader writes are made visible back to host via `vkCmdPipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT -> VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT -> VK_ACCESS_HOST_READ_BIT)` + `VkFence` (`vkWaitForFences`) + `vkInvalidateMappedMemoryRanges` (when not `HOST_COHERENT`).
   - All Vulkan objects (`VkCommandPool`, `VkFence`, `VkDescriptorPool`, `VkPipeline`, `VkPipelineLayout`, `VkDescriptorSetLayout`, `VkShaderModule`, `VkBuffer`, `VkDeviceMemory`, `VkDevice`, `VkDebugUtilsMessengerEXT`, `VkInstance`) are deterministically destroyed on every success and failure path (`ASan+LSan` zero leaks + `VK_LAYER_KHRONOS_validation` zero errors/warnings).

---

## 4. Differential Numerical Policy (`SCALAR_C99` vs `AVX2_FMA` vs `PTX_SM120` vs `VULKAN_SPV`)

- All `13` compute operations share Track 21A's `(atol, rtol, max_ulp)` policy with `ZCC_ACCUM_FP32_FMA` / `ZCC_ACCUM_STRICT_FP32` semantics (explicit `FP64_REF` accumulation stays `SCALAR_C99`-exclusive because SPIR-V `1.0` baseline targets `Shader` without `Float64` capability — requesting `ZCC_ACCUM_FP64_REF` on `VULKAN_SPV` under `EXACT_REQUIRED` rejects with `ZCC_STATUS_ERR_UNSUPPORTED_HW`).
- Non-contiguous strides, unaligned host buffers (`+1/+4/+8/+12`), prime/boundary shapes, extreme magnitudes, `NaN`/`+/-Inf` propagation (tested cleanly under IEEE `F32` SPIR-V semantics), invalid shapes/strides/ops/aliasing/`READONLY`, and OOM hooks fail identically to Track 21A.
- Hardware GPU Vulkan verification via WSL2 `/dev/dxg` + Mesa Dozen (`dzn`) (`0x10de` RTX 5070 `DISCRETE_GPU` and `0x1002` AMD Radeon `INTEGRATED_GPU`) is recorded in `TRACK21B_HW_VULKAN_REPORT.json`; direct unsigned Win64 native proprietary ICDs (`T21B-GPU-NV-NATIVE`, `T21B-GPU-AMD-NATIVE`) remain cleanly separated and `DEFERRED` under Smart App Control.
