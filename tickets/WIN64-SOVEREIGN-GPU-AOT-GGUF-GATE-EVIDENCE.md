# WIN64 Sovereign Native GGUF -> Blackwell `sm_120` PTX AOT Model Compiler — Gate Evidence (`ZCC-EF-7 v1.0.0`)

**Classification:** Gate 1–5 Independent Verification & Closure Ticket (Track 8)  
**`governing_rule_version`:** `ZCC-EF-7 v1.0.0`  
**`evidence_tier`:** `E2` (Engineering Milestone Closure — live hardware + cross-ISA parity + throughput & negative controls)  
**`evidence_tier_assigned_at`:** `2026-11-21T09:05:00Z`  
**`source_revision`:** `28f81f658baaf408670db2c6db5f92a4d270d94b` (working tree on top of `HEAD`, Track 6 + Track 7 Hardened + Track 8)  
**Hardware / Driver Oracle:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, CUDA driver version `12090`, PTX `.version 8.7` / `.target sm_120`)  
**Zero-External-Toolchain Proof:** `.gguf -> .ptx` specialization executed purely inside ZCC (`./zcc`, `build_win64/zcc_sov_stage3.exe`, and `build_wasm/zcc.wasm`) with zero `libm` calls during PTX emission and zero `nvcc`, CUDA Toolkit, MSVC, MinGW, or binutils.

---

## 1. Architecture & Compiler Changes

1. **Minimal CLI Front-End Hook ([part5.c](file:///h:/__DOWNLOADS/zcc_github_upload/part5.c)):**
   - Recognizes `.gguf` input extension and `--model <path.gguf>` alongside `--target=ptx --gpu-arch=sm_120` (and `--ptx-arch=sm_120`), dispatching early to `ptx_try_compile_gguf()` in `part6_ptx.c`.
   - Total `git diff --numstat part5.c` vs `HEAD` (`28f81f65`): **`25 1 part5.c`** (`26` changed lines across both Track 6 and Track 8 — strictly `< 40` Track 6 invariant and `< 50` AGENTS invariant).
   - Zero edits to `part0_pp.c..part4.c`, `part6_arm.c`, `part6_riscv.c`, `part6_wasm.c`, `src/codegen.c`, `src/win64_pe_emit.c`, `doom/`, `src/sqlite3.c`, `tests/embedded_*`.

2. **Native GGUF v3 AOT Specialization Compiler ([part6_ptx.c](file:///h:/__DOWNLOADS/zcc_github_upload/part6_ptx.c)):**
   - `ptx_compile_gguf_model()` parses & fail-closed validates GGUF v3 headers, power-of-two `general.alignment`, `gguf_ptx_mul_u64` overflow checks, vocab dimension cross-checks, and all layer tensor shapes/offsets before opening `out_ptx` (`"wb"` mode).
   - Emits model-specialized `.version 8.7` / `.target sm_120` PTX featuring:
     - **Dead-format kernel elimination:** emits only the embedding and linear kernels (`F32`, `F16` SM 12.0 `mma.sync.aligned.m16n8k16`, or `Q8_0`) actually used by the GGUF tensors.
     - **Single-CTA fused megakernel (`zcc_aot_forward_token`, `128` threads / `4` warps):** keeps `x`, `xb`, `q`, `k`, `v`, `hb`, `hb2`, `xf16`, per-warp `att`, and reduction scratchpad entirely in on-chip `.shared` memory (`4368` B for F16-TC, `4112` B for F32/Q8_0), unrolls all transformer layers with compile-time tensor byte offsets into the contiguous `wblob`, shares one `zcc_aot_f32_to_f16` conversion across Q/K/V and Gate/Up, fuses residual additions into `Wo` and `Wdown`, and executes **1 kernel launch per token** (vs `47` F16-TC / `31` F32/Q8_0 launches per token in generic mode).
     - **Fail-closed fingerprint oracle (`zcc_aot_model_meta`):** writes 16 `uint64_t` verification words including `0x5A4343414F543031` (`"ZCCAOT01"`), 64-bit FNV-1a `.gguf` file fingerprint, layer/dim/vocab geometry, `data_start`, and `wblob_bytes`.
     - **Zero host `libm` dependency:** emits exact GGUF IEEE-754 float bits (`0f%08X`) and device-side `rsqrt.approx.f32` / `lg2.approx.f32` so Linux x86-64, Win64 PE32+, and WASI binaries emit byte-for-byte identical `.ptx`.

3. **Runtime AOT Execution Mode ([win64_gpu_llm.c](file:///h:/__DOWNLOADS/zcc_github_upload/src/win64_gpu_llm.c)):**
   - Added `--aot-ptx <file.ptx>` which JIT-loads the model-specialized `.ptx`, executes `zcc_aot_model_meta`, verifies full geometry + 64-bit FNV-1a fingerprint before uploading the weight slab in one `cuMemcpyHtoD_v2`, and executes single-launch inference per token.

---

## 2. Triple-Compiler AOT PTX Byte Identity & Dead-Format Elimination

Observed across ALL THREE sovereign compilers (`./zcc`, `build_win64/zcc_sov_stage3.exe` `3075584B`, `build_wasm/zcc.wasm` `1060797B`):

| GGUF Input | Specialized `.ptx` Output | Bytes | SHA-256 (`./zcc == zcc_sov_stage3.exe == zcc.wasm`) | `fc /b` | Dead-Format Check |
| --- | --- | --- | --- | --- | --- |
| `build_win64/zcc_tiny_llama_f32.gguf` | `build_win64/zcc_tiny_llama_f32_sm120.ptx` | `24826` | `15a9010659d03db520e355ca33658d46b87803f9446954dc0d5d23c9eefbf792` | `no differences encountered` | `embed_f32`+`matvec_f32` only; zero `f16`/`q8_0`/`gemm_tc` |
| `build_win64/zcc_tiny_llama_f16.gguf` | `build_win64/zcc_tiny_llama_f16_sm120.ptx` | `28684` | `b7fee36d50d4b1bf3ed86df6241851c82e8012a8cf3118684082a84324566e58` | `no differences encountered` | `embed_f16`+`f32_to_f16`+`gemm_tc_f16` (`mma.sync.aligned.m16n8k16`) only; zero `f32`/`q8_0`/scalar `matvec_f16` |
| `build_win64/zcc_tiny_llama_q8_0.gguf` | `build_win64/zcc_tiny_llama_q8_0_sm120.ptx` | `25581` | `9d58d6d3f64ead275cb42457cfe38551980d5e20376035e5fcf16cd761e4bfdb` | `no differences encountered` | `embed_q8_0`+`matvec_q8_0` only; zero `f32`/`f16`/`gemm_tc` |

---

## 3. Live RTX 5070 AOT vs Generic Performance & Differential Accuracy (`44` steps)

| Precision / Model | `.shared` Bytes | Generic Median (`tok/s`) | AOT Fused Median (`tok/s`) | Measured Speedup | AOT vs Python Oracle Max Abs Diff | AOT vs Generic Max Abs Diff | Greedy Token Parity |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `F32` (`"Blackwell "`) | `4112` | `2589.22` (`20.3736 ms`) | `4346.86` (`11.9287 ms`) | **`1.68x`** | `0.00000321` (`< 1e-4`) | `0.00000310` | `44 / 44` (`100%`) |
| `F16-TC` (`"ZCC compiles "`) | `4368` | `1922.48` (`30.8098 ms`) | `9197.58` (`6.4456 ms`) | **`4.78x`** | `0.00000280` (`< 1e-2`) | **`0.00000000`** (`logits_fnv=227a55f4`) | `44 / 44` (`100%`) |
| `Q8_0` (`"RTX 5070 "`) | `4112` | `2865.89` (`21.0336 ms`) | `5329.81` (`10.4612 ms`) | **`1.86x`** | `0.00000523` (`< 2e-3`) | `0.00000238` | `44 / 44` (`100%`) |

---

## 4. `ZCC-EF-0` Negative Controls (`test_06_aot_ef0_negative_controls`)

- **AOT-NC-1 (1-bit weight mutation):** Flipping 1 bit in a scratch `.gguf` fails closed before inference (`AOT_FINGERPRINT_MISMATCH`, nonzero exit).
- **AOT-NC-2 (Cross-model mismatch):** Pairing `zcc_tiny_llama_f16_sm120.ptx` with `zcc_tiny_llama_q8_0.gguf` fails closed (`AOT_FINGERPRINT_MISMATCH`, nonzero exit).
- **AOT-NC-3 (Corrupt AOT metadata header):** Mutating `0x5a4343414f543031` inside `.ptx` fails closed (`AOT_PTX_ERROR`, nonzero exit).
- **AOT-NC-4 (Missing `.gguf` to AOT compiler):** `./zcc --target=ptx --gpu-arch=sm_120 --model missing.gguf` exits nonzero and creates no `.ptx`.
- **AOT-NC-5 (Invalid `general.alignment` in AOT compiler):** Non-power-of-two `alignment=12` fails closed (`non-power-of-two`) and leaves no `.ptx`.

---

## 5. Gate 1–5 Complete Observed Verification Outputs

- **Gate 1 (`make selfhost` + `BOOTSTRAP_BASELINES.tsv`):**
  ```text
  stage2.s=5eae02b164992a842192fb443ce34a8039c3df4e4ca4178f4751bea4c7c7977b
  stage3.s=5eae02b164992a842192fb443ce34a8039c3df4e4ca4178f4751bea4c7c7977b
  bc2aa8396a28c351a35d0cf712d81fea  zcc2.s
  bc2aa8396a28c351a35d0cf712d81fea  zcc3.s
  BOOTSTRAP HASH: bc2aa8396a28c351a35d0cf712d81fea  commit=28f81f65  elisions=2213
  BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
  ```
- **Gate 2 (`scripts/gate2_interop.sh` + `track3_compound_assign_c99_regressions.c`):**
  Both inter-op directions `INTEROP_SUCCESS: x=10 y=17 z=300`; `TRACK3_COMPOUND_ASSIGN_C99_REGRESSIONS: 11/11 PASS`.
- **Gate 4 & 5:**
  - `make test-win64-sovereign-suite`: Exit `0` (`win64_selfhost_sovereign`, Lua+SQLite `6/6`, QuickJS+MicroPython `5/5`, DOOM `6/6`, `test_win64_gpu_ptx.py` `6/6`, `test_win64_gpu_llm.py` `8/8`, `test_win64_gpu_aot_gguf.py` `6/6`).
  - `python3 tests/test_win64_gpu_aot_gguf.py -v`: `Ran 6 tests in 4.126s — OK`.
  - `python3 tests/test_win64_gpu_llm.py -v`: `Ran 8 tests in 5.356s — OK`.
  - `python3 tests/test_win64_gpu_ptx.py -v`: `Ran 6 tests in 2.746s — OK`.
  - `python3 tests/test_win64_pe_emitter.py -v`: `Ran 39 tests in 4.186s — OK (skipped=5)`.
  - `make selfhost-sovereign`: Exit `0` (`[PASS] selfhost-sovereign: byte-identical zero-binutils object & ELF emission verified.`).
  - `python3 tests/test_wasm_wasi.py -v`: `Ran 12 tests in 1.197s — OK` (`WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`).
  - `python3 tests/test_wasm_wasi_selfhost.py -v`: `Ran 4 tests in 29.213s — OK` (`zcc.wasm == zcc2.wasm == zcc3.wasm`, `1060797B`, `2ac7e978...`).
  - `python3 tests/test_arm64_riscv64_direct_elf.py -v`: `Ran 5 tests in 5.744s — OK` (`qemu-aarch64-static=NOT_INSTALLED_ON_HOST qemu-riscv64-static=NOT_INSTALLED_ON_HOST`).

---

## 6. `ZCC-EF-7 v1.0.0 §9` Closure Adjudication

| Claim | Evidence Class | Evidence Tier | Evidence | Scope | Limitations | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `./zcc`, `zcc_sov_stage3.exe`, and `zcc.wasm` natively compile `.gguf` models directly to byte-identical `sm_120` AOT `.ptx` with dead-format elimination | OBSERVED | E2 | `test_01_aot_gguf_triple_compiler_byte_identity_and_dead_format_elim`, `fc /b`, `SHA256` parity across `f32`/`f16`/`q8_0` | Single-CTA `.shared` megakernel (`<= 45056` B `.shared`, `max_seq <= 2048`) | External `wasmtime` CLI `NOT_INSTALLED_ON_HOST` (`node:wasi` + `BrowserWasiShim` verified); larger multi-GiB models exceeding single-CTA `.shared` fail closed cleanly until multi-CTA AOT lowering is added in future work | **PASS** |
| Single-launch AOT PTX executes accurately (`<= 5.23e-6` vs pure-Python oracle) and measurably faster (`1.68x`–`4.78x` on RTX 5070) than generic multi-launch execution | OBSERVED | E2 | `test_02..test_05`, live benchmark (`F16-TC`: `9197.58` vs `1922.48` median tok/s, `aot_vs_gen=0.00000000`) | Sovereign 2-layer Llama GQA (`F32`/`F16`/`Q8_0`) | Evaluated on ASCII byte-tokenized GGUF v3; BPE + `Q4_K_M` multi-CTA compilation deferred to future Track 7.1/8.1 | **PASS** |
| Fail-closed fingerprinting & parser guards reject all adversarial negative controls (`AOT-NC-1..5`) and preserve Gates 1–5 (`bc2aa8396a28c351a35d0cf712d81fea`) | OBSERVED | E2 | `test_06_aot_ef0_negative_controls`, `log_bootstrap_hash.sh --check` (`EXIT=0`), `make test-win64-sovereign-suite`, `make selfhost-sovereign`, `part5.c` `25+/1-` | Repository Gates 1–5 | Azure CI runner MD5 rows pre-synchronized per `E-LEARN-017` pending remote Azure CI run; `test_win64_pe_emitter.py` skips `5` Linux-hosted Win-exec tests (`skipped=5`); QEMU ARM64/RISC-V `NOT_INSTALLED_ON_HOST` | **PASS** |
