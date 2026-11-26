# ZCC Track 21B — Implementation & Verification Plan

**Governing Invariants:** `ZCC-EF-0`, `ZCC-EF-7`, `INV-RELEASE-CLOSURE`, `INV-VERIFIER-PURITY`  
**Frozen Predecessor Baseline:** `7e5821daf` (`track21a-v1.0.0-verified`, untouched)  
**Specification:** [`TRACK21B_COMPUTE_CONTRACT.md`](TRACK21B_COMPUTE_CONTRACT.md)

---

## 1. Module Plan & ABI Extension (Zero Track 21A & Zero Core Disruption)

1. **Defect Resolution (Separate Scope, Requirement 8):**
   - Resolve `DEFECT-T21A-VP01` inside `scripts/log_bootstrap_hash.sh` (`--check` / `--readonly` exits 0/1 cleanly without appending `BOOTSTRAP_BASELINES.tsv`), verified in a dedicated test preserving `S_before == S_after` and kept in a separately reviewed patch/commit distinct from Track 21B.
2. **Isolated SPIR-V Backend (`PASSES` + Dedicated CLI + Library API):**
   - `include/zcc_spirv.h` & `src/part6_spirv.c` (added strictly to `Makefile` `PASSES`): SSA SPIR-V 1.0 binary assembler + emitters for all `13` compute kernels (`zcc_spirv_emit_op_kernel(...)`), deterministic SHA-256 verifiable output.
   - `tools/zcc_spirv_cli.c`: standalone CLI (`/tmp/zcc-spirv`, `make track21b-spirv`) emitting `.spv` files cleanly and running programmatic self-audit without altering `part5.c` or `BOOTSTRAP_BASELINES.tsv`.
3. **Extended Compute ABI & Vulkan Runtime (`runtime/` + `include/` + `sdk/`):**
   - `include/zcc_vulkan_compute.h`: includes frozen `include/zcc_compute.h` untouched; defines `ZCC_COMPUTE_V2_ABI_VERSION`, `ZCC_BACKEND_VULKAN_SPV = 4`, `ZCC_BACKEND_VULKAN_GPU = 5`, `zcc_vulkan_caps_t`, `zcc_vulkan_exec_options_t`, `zcc_compute_v2_receipt_t` (`track21b.receipt.v1`), and `zcc_compute_exec_v2(...)`.
   - `runtime/compute_vulkan.c`: dynamic `libvulkan.so.1` loader, `VK_LAYER_KHRONOS_validation` debug messenger capturing validation errors fail-closed, physical-device capability negotiation (`CPU` vs `DISCRETE_GPU` vs `INTEGRATED_GPU`), staging/descriptor management, explicit `vkCmdPipelineBarrier` host->shader & shader->host barriers, `VkFence` wait, zero-leak teardown.
   - `runtime/compute_dispatch_v2.c`: dispatches `SCALAR_C99`, `AVX2_FMA`, `PTX_SM120` (via untouched `zcc_compute_exec`) and `VULKAN_SPV` / `VULKAN_GPU`, rejects unsupported `FP64_REF` / missing GPU on `CPU`-only `llvmpipe` fail-closed under `EXACT_REQUIRED`, seals `track21b.receipt.v1` SHA-256 + optional HMAC-SHA256 (`verification_status = "UNVERIFIED_RUNTIME_ONLY"`, `zk_proof_present = 0`).
   - `sdk/zcc_vulkan_compute.py` & `sdk/zcc_vulkan_compute.mjs`: additive Python/MJS SDK bindings re-exporting Track 21A & studio clients non-destructively.
4. **Verification, Mutation Observatory & Independent Verifier (`tests/track21b/`, `tools/verify_track21b_release.py`):**
   - `tests/track21b/run_vulkan_suite.sh`: builds `/tmp/libzcc_compute_v2.so` & `/tmp/zcc-spirv`, verifies `ASan+LSan+UBSan` (`--cpu-only` for CUDA safety, including `llvmpipe` Vulkan + `VK_LAYER_KHRONOS_validation` under `ASan` + `UBSan`!), dual-toolchain checks, `13` `spirv-val --target-env vulkan1.0` invocations, deterministic replay, and OOM/teardown tests.
   - `tests/track21b/differential_oracle_21b.py`: verifies all `13` ops + edge cases across `SCALAR_C99`, `AVX2_FMA`, `PTX_SM120`, and `VULKAN_SPV (llvmpipe)` against independent FP64 oracle (`TRACK21B_DIFFERENTIAL_REPORT.json`), enforces `BLOCKED` if `spirv-val` missing, logs performance (`TRACK21B_PERFORMANCE_REPORT.json`).
   - `tests/track21b/mutation_observatory_21b.py`: injects controlled mutants across malformed SPIR-V (`spirv-val` kill), wrong shader arithmetic/quant, fabricated `DISCRETE_GPU` identity on CPU `llvmpipe`, missing `spirv-val` (`BLOCKED`), capability/accumulation bypass, barrier/validation tampering, baseline tamper, and verifier fail-open (`TRACK21B_MUTATION_REPORT.json`).
   - `tools/verify_track21b_release.py`: fail-closed verifier (`--self-test`, `--verify-staged`, full E3 run -> `TRACK21B_RELEASE_EVIDENCE.json`), explicitly preserving `T21B-GPU-NV` & `T21B-GPU-AMD` as `DEFERRED` / host-blocked while passing `T21-G01`, `T21-G02-CPU`, `T21B-G03..G10`.
   - Isolated clean-checkout reproduction before committing or tagging.
