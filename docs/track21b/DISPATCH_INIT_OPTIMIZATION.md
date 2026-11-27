# ZCC Track 21B Runtime Optimization: Single-Context Reuse in `compute_dispatch_v2.c`

> **Scope clarification:** Reducing redundant Vulkan initialization in `runtime/compute_dispatch_v2.c` is strictly a runtime latency and allocation optimization. It does **NOT** fix or mask Mesa's upstream `u_cpu_detect()` `L3_affinity_masks` leak on stock drivers; upstream leak elimination and verification are handled cleanly via [patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch](file:///h:/__DOWNLOADS/zcc_github_upload/patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch) and [tools/track21b_transitive_verifier.py](file:///h:/__DOWNLOADS/zcc_github_upload/tools/track21b_transitive_verifier.py).

## 1. Change Summary

Previously, stateless Vulkan calls through `zcc_compute_exec_v2` (`ctx == NULL` and `restricted_vk_caps == NULL` and `requested_backend > ZCC_BACKEND_PTX_SM120`):
1. Created and destroyed a throwaway `VkInstance` in `zcc_probe_vulkan_caps(&live_vk, 1)` (unconditionally loading `VK_LAYER_KHRONOS_validation`) strictly to inspect device limits.
2. Created and destroyed a second `VkInstance`/`VkDevice` inside `zcc_vulkan_exec_on_ctx(NULL, ...)` to execute the shader.

In [compute_dispatch_v2.c](file:///h:/__DOWNLOADS/zcc_github_upload/runtime/compute_dispatch_v2.c#L579-L613), `zcc_compute_exec_v2_with_ctx` now creates a single temporary `zcc_vulkan_ctx_t` (honoring `opts->require_validation_layer`), passes it through capability inspection, validation, negotiation, and Vulkan execution inside `compute_exec_v2_with_ctx_inner`, and unconditionally destroys it via `zcc_vulkan_ctx_destroy(owned_tmp_ctx)` before returning. Non-Vulkan requests (`<= ZCC_BACKEND_PTX_SM120`) preserve zero Vulkan initialization.

---

## 2. Measured Before vs After (`ubuntu-24.04` `bwrap`, Clean Lavapipe `25.2.8-.5`)

### 2.1 Single Stateless Dispatch Footprint (`one_vk_exec`, `require_validation_layer=0`)
| Metric | Before Refactor | After Refactor | Delta |
|---|---|---|---|
| `libvulkan_lvp.so` Init/Unload Cycles (`unmapped_target_events`) | 2 | 1 | **-50.0%** |
| Total Heap Allocations (`total_allocs`) | 1,029,154 | 61,849 | **-94.0%** |
| Peak Live Heap Bytes (`peak_live_bytes`) | 48,696,436 B | 32,571,155 B | **-33.1%** |
| `VULKAN_SPV` Output Value (`[1..4] + [5..8]`) | `6.0` | `6.0` | Identical |

### 2.2 O2 Dispatch Latency (`test_track21b_vulkan --bench`, Validated Output Bit-Identity)
| Workload (`VULKAN_SPV` Stateless) | Before (`avg_us`) | After (`avg_us`) | Speedup | Output SHA-256 Prefix |
|---|---|---|---|---|
| `GEMM_F32_128x512x128` | 104,012.72 us | 20,456.99 us | **5.08x** | `e0062927f907c4e5` (identical) |
| `RMSNORM_F32_128x512` | 97,052.54 us | 15,804.44 us | **6.14x** | `9b82d6f8efb2d029` (identical) |
| `SOFTMAX_F32_128x512` | 99,128.41 us | 14,890.96 us | **6.66x** | `1173ac2e3a1bcea3` (identical) |
| `MATVEC_Q4_K_128x512` | 95,755.18 us | 13,630.19 us | **7.03x** | `b9201f3a51bc4186` (identical) |

---

## 3. Semantic Equivalence, Fault Injection & Memory Safety Verification
- **Strict ASan + UBSan + LSan (`detect_leaks=1`, zero suppressions):** `TRACK21B_C_TESTS pass=262 fail=0`, `AFTER_ASAN_RC=0`.
- **21-Point Boundary Fault Injection & Lifetime Audit:** `TRACK21B_VK_LIFETIME_AUDIT cycles=10 faults_injected=21 live_total=0 seq_violations=0 teardown_mask=0x7ffe`.
