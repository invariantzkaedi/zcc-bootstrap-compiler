# ZCC Track 21B — Verified SPIR-V & Vulkan Compute Reproduction and Migration Guide

**ABI Version:** `0x00010100` (`track21b.receipt.v1`)  
**Frozen Track 21A Base:** `7e5821daf` (`track21a-v1.0.0-verified`, untouched)  
**Governing Rules:** `ZCC-EF-0`, `ZCC-EF-7`, `INV-T21B-001`..`INV-T21B-010`

---

## 1. Prerequisites & Independent Oracles (WSL2 / Linux)

Track 21B enforces independent Khronos external validation and fails closed with `BLOCKED` (exit `3`) when required external tools are absent:

```bash
sudo apt-get update && sudo apt-get install -y \
  spirv-tools glslang-tools vulkan-validationlayers vulkan-tools libvulkan-dev
```

Verify available tools and Vulkan ICD identity:

```bash
which spirv-val glslangValidator vulkaninfo
vulkaninfo --summary
```

Expected WSL2 environment: Mesa `llvmpipe` (`VK_PHYSICAL_DEVICE_TYPE_CPU = 4`, vendor `0x10005`, `VK_LAYER_KHRONOS_validation` available).

---

## 2. Reproducing All Track 21B Gates

All commands run cleanly from repository root (and from an isolated `git worktree` checkout):

1. **Build portable runtime & standalone SPIR-V compiler CLI (plus ISA isolation check):**
   ```bash
   make track21b-build
   ```
   Confirms `0` AVX/AVX2/FMA/AVX-512 instructions inside the 9 portable object files (`part6_spirv.o`, `compute_vulkan.o`, `compute_dispatch_v2.o`, and Track 21A portable units), while `compute_avx2.o` keeps isolated AVX2 kernels.

2. **Emit SPIR-V 1.0 kernels & verify independently with Khronos `spirv-val`:**
   ```bash
   /tmp/zcc-spirv --emit-all /tmp/t21b_spv
   for f in /tmp/t21b_spv/*.spv; do spirv-val --target-env vulkan1.0 "$f"; done
   ```

3. **Run full C/UBSan + ASan/LSan/UBSan + O2 + MJS SDK contract suite:**
   ```bash
   make track21b-c-suite
   ```
   Notes:
   - `[3/6]` Full UBSan tests all backends including `SCALAR_C99`, `AVX2_FMA`, `PTX_SM120`, and `VULKAN_SPV` on live `llvmpipe` with `VK_LAYER_KHRONOS_validation` enabled.
   - `[4/6]` ASan + UBSan + LSan (`detect_leaks=1`) passes `--skip-ptx` strictly to avoid a known double-free inside NVIDIA's proprietary WSL2 `libcuda.so.1` (`cuInit(0)` -> `libnvdxgdmal.so.1`), while exercising live Mesa `llvmpipe` Vulkan and Khronos validation layers under ASan + LSan.

4. **Run multi-backend differential numerical oracle & performance collector:**
   ```bash
   make track21b-diff
   ```
   Generates `TRACK21B_DIFFERENTIAL_REPORT.json` and `TRACK21B_PERFORMANCE_REPORT.json`.

5. **Run controlled mutation observatory:**
   ```bash
   make track21b-mutate
   ```
   Generates `TRACK21B_MUTATION_REPORT.json` across malformed SPIR-V, shader arithmetic/quant corruption, fabricated `DISCRETE_GPU` device identity, unsupported capability bypasses, synchronization barrier drops, self-certifying receipts, stripped external oracle (`PATH`), baseline tampering, and verifier fail-open checks.

6. **Run full E3 fail-closed release verifier:**
   ```bash
   make track21b-verify
   ```
   Generates `TRACK21B_RELEASE_EVIDENCE.json` and confirms core compiler `PARTS` (`zcc2.s == zcc3.s`), `BOOTSTRAP_BASELINES.tsv`, and Track 21A frozen deliverables remain unchanged (`S_before == S_after`).

---

## 3. Explicit Status Semantics & Hardware Deferral

Every Track 21B verifier report and receipt preserves explicit states:

| State | Meaning |
| --- | --- |
| `PASS` | Executed cleanly and independently validated within specification/tolerance. |
| `FAIL` | Violated numerical bound, capability invariant, validation layer rule, or receipt check. |
| `BLOCKED` | External validator (`spirv-val`, `glslangValidator`, `libvulkan.so.1`, `VK_LAYER_KHRONOS_validation`) unavailable or host-blocked. Never promoted to `PASS`. |
| `UNKNOWN` | Unregistered anomaly preserved verbatim by the mutation observatory. |
| `DEFERRED` | Explicitly deferred hardware gate outside WSL2 software Vulkan scope. |

### Deferred Hardware Gates (`T21B-GPU-NV`, `T21B-GPU-AMD`)

- `T21B-GPU-NV`: Native NVIDIA RTX 5070 Vulkan execution (`0x10de`) -> **`DEFERRED`**
- `T21B-GPU-AMD`: Native AMD Radeon Graphics Vulkan execution (`0x1002`) -> **`DEFERRED`**

On this Windows + WSL2 workstation:
- WSL2 `/dev/dxg` exposes CUDA (`PTX_SM120`) and Vulkan software rendering (`llvmpipe`, `VK_PHYSICAL_DEVICE_TYPE_CPU`), not hardware Vulkan ICDs for `0x10de` / `0x1002`.
- Native Windows unsigned executables are blocked by Windows Smart App Control (explicit policy forbids bypassing Smart App Control).
- Software `llvmpipe` runs MUST always identify as `device_type = "CPU"` and satisfy `T21-G02-CPU` cleanly without claiming hardware GPU execution.

---

## 4. Migration Guide (Track 21A -> Track 21B)

1. Existing callers including `"zcc_compute.h"` or importing `sdk/zcc_compute.py` / `sdk/zcc_compute.mjs` remain 100% compatible without changes.
2. Callers wanting SPIR-V + Vulkan execution switch to `"zcc_vulkan_compute.h"` (`zcc_compute_exec_v2`, `ZCC_BACKEND_VULKAN_SPV = 4`, `ZCC_BACKEND_VULKAN_GPU = 5`) or `sdk/zcc_vulkan_compute.py` (`ZccVulkanComputeRuntime`) / `sdk/zcc_vulkan_compute.mjs` (`validateVulkanReceiptShape`).
3. Compiler CLI users emit standalone SPIR-V 1.0 compute kernels via `/tmp/zcc-spirv --emit-op <OP> -o <out.spv>` or `/tmp/zcc-spirv --emit-all <dir>` (`src/part6_spirv.c` linked into `Makefile` `PASSES` without altering `PARTS`).
4. Separate baseline hygiene fix `DEFECT-T21A-VP01`: `bash scripts/log_bootstrap_hash.sh --check` inspects bootstrap MD5 continuity read-only without mutating `BOOTSTRAP_BASELINES.tsv`.
