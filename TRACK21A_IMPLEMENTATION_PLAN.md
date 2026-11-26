# ZCC Track 21A — Implementation & Verification Plan

**Governing Invariants:** `ZCC-EF-0`, `ZCC-EF-7`, Read-Before-Touch (RBT), Separate TU ISA Isolation  
**Predecessor Audit:** `TRACK21A_REPO_AUDIT.json` (Baseline GREEN, `HEAD 8ff7da72d`, self-host MD5 `6b83f8d4c14bbdc925fc33be6af1979b`)  
**Canonical Specification:** `TRACK21A_COMPUTE_CONTRACT.md`

---

## 1. Architectural Strategy (Zero Compiler-Core Disruption)

1. **Compiler Core Stability:** Do NOT mutate `part0_pp.c`, `part1.c`..`part6_ptx.c`, `BOOTSTRAP_BASELINES.tsv`, or `ir_pass_*`. Track 21A exercises the existing `./zcc` compiler cleanly as a **consumer & toolchain oracle**:
   - Compiling scalar reference code cleanly via BOTH `gcc -std=c99 -O2 -mno-avx -mno-avx2 -mno-fma` AND `./zcc`.
   - Compiling `runtime/kernels_t21a_ptx.c` into Blackwell `sm_120` PTX ISA 8.7 via `./zcc --target=ptx --ptx-arch=sm_120`.
2. **Legacy Defects Isolated (Not Blindly Reused):**
   - `src/avxzkd_supreme.c` (`DEFECT-01..04`: global `-mavx2 -mfma`, missing OSXSAVE/XCR0+FMA gating, missing scalar fallback) is excluded from Track 21A compute execution and documented in `docs/TRACK21A_REPRODUCTION_AND_MIGRATION.md`.
   - `forgezero_receipt.c` (`djb2` 32-bit hash, stubbed in `zcc`) and `src/zcc_oracle_substrate.c` (hardcoded ZXR hashes) are excluded from authoritative compute receipts.
   - `src/zcc_tensor_attest.c`'s FIPS 180-4 SHA-256 algorithm design is reused in dedicated `runtime/compute_sha256.c`.
3. **Separate Compilation Units for ISA Safety (`T21-G03`):**
   - Portable TUs (`cpu_capabilities.c`, `compute_sha256.c`, `compute_scalar.c`, `compute_dispatch.c`, `compute_ptx.c`, `execution_receipt.c`) MUST be compiled with `-mno-avx -mno-avx2 -mno-fma`.
   - Only `runtime/compute_avx2.c` is compiled with `-mavx2 -mfma`, and every kernel entrypoint verifies `zcc_cpu_has_avx2_fma(&caps)` at entry before executing any AVX/FMA instruction.
   - `objdump -d` disassembly audit verifies zero `ymm`/`zmm`/`vfmadd`/`vaddps` opcodes inside portable `.o` objects.

---

## 2. Module Breakdown & Deliverables Map

### Phase 2 — Reference & Accelerated Runtime Backends (`runtime/`, `include/`, `sdk/`)
| Artifact | Responsibility | Compilation Flags |
| --- | --- | --- |
| `include/zcc_compute.h` | Canonical versioned C99 Compute ABI (`ZCC_COMPUTE_ABI_VERSION`), tensor descriptor, op/dtype/error enums, CPU caps struct, receipt structs, C API declarations | C99 header (`_Static_assert` layout checks) |
| `runtime/compute_sha256.c` | Freestanding FIPS 180-4 SHA-256 + optional HMAC-SHA256 helper + canonical digest formatting | `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fPIC` |
| `runtime/cpu_capabilities.c` | Inline x86_64 `CPUID` (`leaf 1`, `leaf 7`) + `XGETBV(XCR0)` probe + pure negotiation `zcc_negotiate_backend(...)` accepting explicit `zcc_cpu_caps_t` | `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fPIC` |
| `runtime/compute_scalar.c` | Pure C99 IEEE-754 reference implementations of all 13 operations (`vec_add`, `vec_mul`, `vec_fma`, `dot`, `gemm`, `rmsnorm`, `softmax`, `dequant/matvec` `Q8_0`/`Q4_0`/`Q4_K`), supporting both `FP32` and `FP64` internal accumulation and arbitrary strides/unaligned ptrs | `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fPIC` (also compiled & executed via `./zcc` for `T21-G04`) |
| `runtime/compute_avx2.c` | Separate-TU AVX2+FMA kernels (`_mm256_loadu_ps`/`_mm256_storeu_ps`, `_mm256_fmadd_ps`, unaligned + odd tail handling + Q8_0/Q4_0/Q4_K vectorized dequant/FMA) with entry capability gate | `-std=c99 -O2 -mavx2 -mfma -fno-unsafe-math-optimizations -fPIC` |
| `runtime/kernels_t21a_ptx.c` | C99 CUDA/PTX kernels compiled cleanly via `./zcc --target=ptx --ptx-arch=sm_120` to `runtime/kernels_t21a_sm120.ptx` (tracked `<= 100 KiB`) | Compiled by `./zcc --target=ptx` |
| `runtime/compute_ptx.c` | Dynamic CUDA Driver loader (`libcuda.so.1` / `nvcuda.dll`), `cuInit`, device query, `cuModuleLoadDataEx` JIT error capture, buffer HtoD/DtoH, synchronous barrier (`cuCtxSynchronize`), explicit OOM/missing-device reporting | `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fPIC` |
| `runtime/compute_dispatch.c` | Unified entrypoint `zcc_compute_exec(...)`, shape/stride/alignment/bounds/aliasing/readonly/NaN-param validation, strict selection policy (`EXACT_REQUIRED` vs `AUTO_FALLBACK`), deterministic replay metadata | `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fPIC` |
| `runtime/execution_receipt.c` | Deterministic JSON + binary receipt builder (`track21a.receipt.v1`), canonical SHA-256 sealing, `"UNVERIFIED_RUNTIME_ONLY"` default status (per ZCC-EF-0) | `-std=c99 -O2 -mno-avx -mno-avx2 -mno-fma -fPIC` |
| `sdk/zcc_compute.py` & `sdk/zcc_compute.mjs` | Additive Python `ctypes` + Node bindings compatible with `sdk/zcc_client.py` / `sdk/zcc_client.mjs` | Pure stdlib / Node ES module |

### Phase 3 — Differential, Edge-Case, Memory-Safety & Benchmark Suites (`tests/track21a/`)
1. `tests/track21a/test_track21a_compute.c`:
   - Compiled under **both** `gcc -fsanitize=address,undefined -fno-sanitize-recover=all` (`T21-G06` memory safety) and standard `-O2` shared-library harness, plus scalar C99 translation check under `./zcc`.
   - Exercises: capability detection & mocked pre-AVX2/missing-OSXSAVE/missing-XCR0/missing-FMA (`T21-G03`), bounds/overflow/readonly/aliasing/misalignment/OOM injection (`T21-G06`), deterministic replay repeat-run bitwise digests (`T21-G08`), and real wall/device timings (`TRACK21A_PERFORMANCE_REPORT.json`).
2. `tests/track21a/differential_oracle.py`:
   - Generates independent IEEE-754 `float64` + pure-Python oracle vectors.
   - Compares `SCALAR_C99` (`FP64` & `FP32`), `AVX2_FMA`, and `PTX_SM120` across: normal dimensions, prime/boundary shapes (`1, 3, 7, 8, 9, 17, 31, 32, 33, 63, 256, 257`), non-contiguous matrix strides, unaligned host pointers (`+4`, `+8`, `+12` byte offset inside 64B-aligned allocation), extreme magnitudes (`1e-18`, `1e12`, cancellation pairs), invalid shapes, `NaN`/`+/-Inf` propagation, OOM injection, and simulated unsupported hardware.
   - Runs `objdump -d` ISA check confirming 0 AVX/FMA opcodes in portable objects (`cpu_capabilities.o`, `compute_scalar.o`, `compute_dispatch.o`, `execution_receipt.o`).
   - Emits `TRACK21A_DIFFERENTIAL_REPORT.json` and `TRACK21A_PERFORMANCE_REPORT.json`.

### Phase 4 — Silent Failure Observatory (`tests/track21a/mutation_observatory.py`)
Injects **actual executable/structural mutants** across all 8 required domains (using real C source/PTX perturbations compiled in isolated `/tmp` sandboxes or binary receipt mutations — no synthetic formula stubs):
1. Arithmetic mutation (`vec_add`: `+` -> `-`; `gemm`: skipped term)
2. Tensor indexing mutation (off-by-one row stride)
3. Quantization block decoding mutation (`Q4_K` scale mask corruption / `Q4_0` nibble offset bias)
4. Accumulation precision / normalization mutation (`rmsnorm` eps dropped or wrong scale)
5. Backend selection mutation (silent fallback to Scalar when `AVX2_FMA` `EXACT_REQUIRED` requested)
6. Capability detection mutation (dropping `XCR0` check and `FMA` check in simulated non-OSXSAVE/non-FMA probe)
7. Receipt generation mutation (corrupted `output_buffer_sha256`, mutated `op_id`, schema downgrade, runtime self-asserting `"PASS"`)
8. Verifier bypass mutation (always-return-0 flag / skipping tolerance check)
Preserves unknown failure signatures explicitly as `"UNKNOWN"` and writes `TRACK21A_MUTATION_REPORT.json`.

### Phase 5 & Phase 6 — Provenance Receipts & Independent Fail-Closed Verifier (`tools/verify_track21a_release.py`)
1. Tamper negative controls (`T21-G09`): 6+ controlled tamper vectors against canonical SHA-256 and HMAC-SHA256 receipts (`output_buffer_sha256` flip, `input_buffers_sha256` flip, `backend_executed` spoof, `status_code` flip, `verification_status` forged `"PASS"`, truncated/malformed JSON, schema version downgrade).
2. Independent verifier (`tools/verify_track21a_release.py`, `T21-G10`):
   - Re-checks git cleanliness of core compiler / baselines (`git diff BOOTSTRAP_BASELINES.tsv` empty), verifies PTX artifact provenance, validates SHA-256 digests of all deliverables & binaries, executes differential + mutation + receipt tamper + memory safety checks (or verifies cryptographic bindings to live run artifacts produced by the test harness), enforces non-vacuous negative controls (`--self-test` proving verifier rejects missing files, stale hashes, contradictory verdicts, runtime self-certification, and survived mutants), and writes `TRACK21A_RELEASE_EVIDENCE.json`.

---

## 3. Build & Pre-Commit Constraints Check
- All new artifacts strictly `< 1.00 MiB` (`tools/pre-commit-binary-tripwire.py` limit `1,048,576 B`), no ELF binaries committed to root (`test` binaries built under `/tmp` or gitignored paths / Makefile `build/track21a/` cleanly excluded from git).
- Before editing `Makefile` (if adding `track21a-*` convenience targets), execute RBT 0.3 `git log -p --follow Makefile | head -300` and preserve all existing targets identically.
- After all implementation & tests finish, re-run existing `make selfhost`, `make verify-differential`, `make check-forgezero-receipt`, `make check-ir-vuln-tag` to prove zero regression (`Gate 1..5`).
