# ZCC Track 21A — Reproduction, Compatibility, and Migration Guide

**Scope:** Portable Verified Compute Foundation (`SCALAR_C99`, `AVX2_FMA`, `PTX_SM120`)  
**Contract:** [`TRACK21A_COMPUTE_CONTRACT.md`](../TRACK21A_COMPUTE_CONTRACT.md)  
**Gates Enforced:** `T21-G03` through `T21-G10` (`T21-G01` SPIR-V and `T21-G02` Vulkan remain deferred to Track 21B)

---

## 1. Reproducing Track 21A Build & Verification Gates

Run all commands from the repository root inside WSL2 / Linux (`gcc >= 13`, `python3 >= 3.10`, `binutils`/`objdump`, NVIDIA driver `libcuda.so.1` for `PTX_SM120`):

### 1.1 Compile Portable vs Accelerated Runtime + Verify ISA Isolation (`T21-G03`)
```bash
bash tests/track21a/build_runtime.sh
```
Compiles all portable translation units (`runtime/compute_sha256.c`, `runtime/cpu_capabilities.c`, `runtime/compute_scalar.c`, `runtime/compute_ptx.c`, `runtime/compute_dispatch.c`, `runtime/execution_receipt.c`) with `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -Wall -Wextra -Werror`, compiles `runtime/compute_avx2.c` separately with `-mavx2 -mfma`, links `/tmp/libzcc_compute.so`, and asserts via `objdump -d` that portable `.o` objects contain `0` AVX/AVX2/FMA instructions (`ymm`/`zmm`/`vfmadd*`).

### 1.2 Regenerate PTX Kernels Using Native ZCC Compiler (`part6_ptx.c`)
```bash
./zcc --target=ptx --gpu-arch=sm_120 runtime/kernels_t21a_ptx.c -o runtime/kernels_t21a_sm120.ptx
```

### 1.3 Run Full C Verification, Memory Safety, Replay & Dual-Compiler Suite (`T21-G03`, `T21-G04`, `T21-G06`, `T21-G08`, `T21-G09`)
```bash
bash tests/track21a/run_c_suite.sh
```
Executes five distinct passes:
1. ASan + LSan (`detect_leaks=1`) + UBSan across CPU backends, capability down-gating with `SIGILL` traps, bounds/OOB/overflow/readonly/aliasing guards, custom-allocator OOM injection (`0` leaked allocations), deterministic replay, and receipt tamper checks (`--cpu-only`).
2. Full UBSan (`-fsanitize=undefined -fno-sanitize-recover=all`) across `SCALAR_C99`, `AVX2_FMA`, and `PTX_SM120` on GPU.
3. Shared library build + `objdump -d` ISA scan.
4. `-O2` full C unit tests + hardware latency benchmark suite emitting [`TRACK21A_PERFORMANCE_REPORT.json`](../TRACK21A_PERFORMANCE_REPORT.json).
5. Dual-compiler check compiling `runtime/compute_scalar.c` and `tests/track21a/test_scalar_zcc.c` directly through `./zcc` (`T21-G04`).

### 1.4 Run Independent Differential Numerical Oracle (`T21-G05`)
```bash
python3 tests/track21a/differential_oracle.py
```
Compares `SCALAR_C99` (`FP64_REF` and `FP32_FMA`), `AVX2_FMA`, and `PTX_SM120` against an independent Python `float64`/`struct`/`math.fsum` oracle across normal, prime/boundary, unaligned (`+1`, `+4`, `+8`, `+12`), non-contiguous/transposed strides, extreme magnitudes/cancellation, NaN/Inf propagation, Q8_0/Q4_0/Q4_K quantization, and simulated unsupported CPU states. Writes [`TRACK21A_DIFFERENTIAL_REPORT.json`](../TRACK21A_DIFFERENTIAL_REPORT.json).

### 1.5 Run Silent Failure Observatory (`T21-G07`)
```bash
python3 tests/track21a/mutation_observatory.py
```
Compiles and executes controlled fault-injected builds in isolated `/tmp` sandboxes across all 8 required domains (arithmetic, indexing, Q4_K & ZCC-recompiled PTX Q8_0 decode, FP64 accumulation/eps precision, silent backend fallback, OSXSAVE/XCR0/FMA capability detection, receipt generation, and verifier fail-open bypass), verifying zero false positives on unmutated controls and preserving unregistered anomalies as `"UNKNOWN"`. Writes [`TRACK21A_MUTATION_REPORT.json`](../TRACK21A_MUTATION_REPORT.json).

### 1.6 Run Independent Fail-Closed Release Verifier (`T21-G09`, `T21-G10`)
```bash
python3 tools/verify_track21a_release.py --self-test
python3 tools/verify_track21a_release.py
```
`--self-test` verifies that tampered receipts, runtime self-certification (`verification_status="PASS"`), missing evidence directories, and survived mutants fail closed (`exit != 0`). Full execution re-runs suites, verifies independent JSON canonical SHA-256 + HMAC-SHA256 receipt reconstruction, checks core compiler/baseline cleanliness, and writes [`TRACK21A_RELEASE_EVIDENCE.json`](../TRACK21A_RELEASE_EVIDENCE.json).

### 1.7 Re-Verify Existing Compiler Self-Host & Baseline Integrity
```bash
make selfhost
make verify-differential
make check-forgezero-receipt check-ir-vuln-tag
```

---

## 2. SDK and ABI Compatibility

| Existing Surface | Compatibility Status | Track 21A Extension |
| --- | --- | --- |
| [`include/zcc_gpu_llm_api.h`](../include/zcc_gpu_llm_api.h) | Unmodified; binary ABI preserved (`ZccLlmGenConfig`, `ZccLlmStats`, `zcc_llm_*`) | Additive [`include/zcc_compute.h`](../include/zcc_compute.h) (`ZCC_COMPUTE_ABI_VERSION = 0x00010000U`, `zcc_tensor_desc_t`, `zcc_compute_exec`, `zcc_compute_receipt_t`) |
| [`sdk/zcc_client.py`](../sdk/zcc_client.py) | Unmodified (`ZccClient`, `ZccApiError`, 28 HTTP/SSE Studio endpoints unchanged) | Additive [`sdk/zcc_compute.py`](../sdk/zcc_compute.py) (`ZccComputeRuntime` ctypes wrapper re-exporting `ZccClient` and `ZccApiError`) |
| [`sdk/zcc_client.mjs`](../sdk/zcc_client.mjs) | Unmodified (`ZccClient`, `ZccApiError`) | Additive [`sdk/zcc_compute.mjs`](../sdk/zcc_compute.mjs) (`ZccBackendId`, `ZccOpId`, `validateComputeReceiptShape`, re-exporting `ZccClient` and `ZccApiError`) |

---

## 3. Migration Notes from Audited Legacy Modules

1. **SIMD / Hardware Capability Gating (`src/avxzkd_supreme.c`):**
   - `src/avxzkd_supreme.c` remains scoped to its existing 2D PDE / two-regime walker / 16-node DTQW tests (`make test-avxzkd`).
   - Do **not** depend on `avxzkd_get_cpu_features()` or `avxzkd_deep_recurse_auto()`/`avxzkd_dtqw_step_auto()` for general compute or pre-AVX2 portability: `Makefile` builds `libavxzkd` with global `-mavx2 -mfma`, `avxzkd_get_cpu_features()` omits `OSXSAVE`/`XCR0` and independent `FMA` checks, and both `*_auto` entrypoints lack scalar fallback branches.
   - **Migration:** Call `zcc_probe_cpu_caps()`, `zcc_cpu_has_avx2_fma()`, and `zcc_compute_exec()` from [`include/zcc_compute.h`](../include/zcc_compute.h), which enforces isolated portable/AVX2 compilation units.
2. **Receipts & Attestation (`forgezero_receipt.c`, `src/zcc_oracle_substrate.c`, `src/zcc_tensor_attest.c`):**
   - `forgezero_receipt.c` uses non-cryptographic 32-bit `djb2` for IR vulnerability events and is linked as no-op stubs (`forgezero_receipt_stub.c`) in `./zcc`. `src/zcc_oracle_substrate.c` logs compiler-pass replay events with hardcoded stage hashes.
   - **Migration:** Compute provenance uses `runtime/execution_receipt.c` + `runtime/compute_sha256.c` (FIPS 180-4 SHA-256 + optional HMAC-SHA256) matching `src/zcc_tensor_attest.c`'s SHA-256 design without static copies inside unrelated subsystems.
3. **PTX Compilation (`part6_ptx.c` vs untracked `src/backend/ir_to_ptx.c`):**
   - Track 21A standardizes exclusively on active tracked `part6_ptx.c` (`./zcc --target=ptx --gpu-arch=sm_120`), deprecating reliance on untracked `src/backend/ir_to_ptx.c` for compute kernels.

---

## 4. Documented Environment & Scope Limitations

- **Windows Device Guard / Smart App Control:** Unsigned new Win64 `.exe` binaries remain blocked at launch on native Windows; all Track 21A verification executes natively under WSL2 Linux ELF64 + `/usr/lib/wsl/lib/libcuda.so.1`, while `./zcc -target win64` compiler paths remain intact.
- **WSL2 NVIDIA Driver + AddressSanitizer Interaction:** Calling `cuInit(0)` inside proprietary `/usr/lib/wsl/drivers/.../libcuda.so.1` (`libnvdxgdmal.so.1`) triggers a driver-internal ASan double-free error. Therefore `run_c_suite.sh` separates CPU ASan+LSan+UBSan (`--cpu-only`) from full CPU+PTX UBSan (`-fsanitize=undefined -fno-sanitize-recover=all`).
- **Track 21B Deferrals:** `T21-G01` (SPIR-V) and `T21-G02` (Vulkan) are intentionally unimplemented in Track 21A and deferred to Track 21B.
