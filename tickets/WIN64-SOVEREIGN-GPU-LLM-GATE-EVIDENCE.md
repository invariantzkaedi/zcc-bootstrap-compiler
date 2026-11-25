# WIN64 Sovereign Zero-Toolchain GPU AI Inference (`zcc_gpu_llm.exe`) — Gate Evidence (`ZCC-EF-7 v1.0.0`)

**Classification:** Gate 1–5 Independent Verification & Hardening Closure Ticket  
**`governing_rule_version`:** `ZCC-EF-7 v1.0.0`  
**`evidence_tier`:** `E2` (Engineering Milestone Closure — live hardware + cross-ISA parity + adversarial negative controls)  
**`evidence_tier_assigned_at`:** `2026-11-21T09:05:00Z`  
**`source_revision`:** `28f81f658baaf408670db2c6db5f92a4d270d94b` (working tree on top of `HEAD`, Track 6 + Track 7 Hardened + Track 8)  
**Hardware / Driver Oracle:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, CUDA driver version `12090`, native PTX `.version 8.7` / `.target sm_120`)  
**Zero-External-Toolchain Proof:** Zero `nvcc`, zero CUDA Toolkit, zero PyTorch/libtorch at inference runtime, zero MSVC, zero MinGW, zero GCC for Win64/PTX emission, zero binutils (`fc /b` + PE32+ System32 export audit against `msvcrt.dll` + `nvcuda.dll` only).

---

## 1. Scope & Hardening Fixes Applied

| Artifact | Purpose | Observed Size / LOC | Notes |
| --- | --- | --- | --- |
| `part6_ptx.c` | C99 → Blackwell `sm_120` PTX compiler backend (+5 AI/Tensor Core intrinsics + Track 8 GGUF AOT frontend) | `2981` lines | Byte-identical across `./zcc`, `zcc_sov_stage3.exe`, and WASI `zcc.wasm` |
| `src/win64_gpu_llm_kernels.c` | 15 PTX GPU `.visible .entry` kernels (`llm_embed_{f32,f16,q8_0}`, `llm_rmsnorm_f32`, `llm_f32_to_f16`, `llm_gemm_tensor_f16`, `llm_matvec_{f32,f16,q8_0}`, `llm_rope_f32`, `llm_kv_store_f32`, `llm_attn_gqa_f32`, `llm_swiglu_f32`, `llm_add_f32`, `llm_softmax_f32`) | `366` lines | Embedded (`45992` B) into `build_win64/gpu_llm_kernels_ptx.h` |
| `src/win64_gpu_llm.c` | Hardened Win64 PE32+ GGUF v3 parser + RTX 5070 `nvcuda.dll` inference runtime + `--temp` scaling + `--aot-ptx` single-launch mode | `1113` lines | Compiles identically with `./zcc` and `zcc_sov_stage3.exe` (`150528` B) |
| `tools/generate_sovereign_gguf.py` | NumPy AdamW trainer (`dim=64`, `hidden=128`, `layers=2`, `heads=4`, `kv_heads=2`, `vocab=128`) + GGUF v3 emitter (`F32`/`F16`/`Q8_0`) + pure-Python stdlib (`struct`+`math`) reference oracle | `709` lines | `steps=450 final_loss=0.004690` |
| `tools/build_gpu_llm_win64.py` | Dual PTX builder, dual PE32+ builder, System32 multi-DLL audit, GGUF generator + dual AOT PTX builder | `196` lines | Reuses `tools/build_gpu_win64.py:write_ptx_header(..., guard, var_name)` |
| `tests/test_win64_gpu_llm.py` | 8-test sovereign verification suite (`NC-1..NC-10` + `--temp` distribution test) | `626` lines | `Ran 8 tests in 5.356s — OK` |
| `BOOTSTRAP_BASELINES.tsv` | Synchronized WSL2 + Azure `E-LEARN-017` baseline rows for `bc2aa8396a28c351a35d0cf712d81fea` + `--check` audit | Verified (`EXIT=0`) | Current locked MD5 matches host GCC 15.2 WSL2 |

### Phase 1 Hardening Summary (`src/win64_gpu_llm.c`)

1. **Real `--temp` scaling & CLI rejection (`INVALID_ARG`):** Host double-precision softmax divides `(logits[i] - max_l)` by `temp` for `temp > 0` and rejects negative, `NaN`, `+inf`, `> 1e6`, and trailing-garbage values (`NC-10`, verified in `test_08_real_temp_sampling_distribution`).
2. **Power-of-two `general.alignment` & wrap guard:** Requires `v >= 1 && (v & (v - 1)) == 0 && v <= (1 << 20)` and guards `(off + align_m1)` before computing `data_start` (`NC-8`).
3. **Checked `mul_u64_checked` tensor & KV-cache sizing:** Prevents 64-bit integer overflow when multiplying `dims[0..n_dims-1]`, element byte sizes, and KV-cache buffers (`NC-9`).
4. **Vocab cross-check:** Verifies `token_embd.weight` second dimension equals `tokenizer.ggml.tokens` array length when present.

### Protected / Core Compiler Invariant Check

Raw observed `git diff --stat` on compiler core & upstream trees:
```text
 part5.c             | 26 ++++++++++++++++++++++-
 src/codegen.c       | 60 ++++++++++++++++++++++++++++++++++++-----------------
 src/win64_pe_emit.c | 30 +++++++++++++++++++++++++++
 3 files changed, 96 insertions(+), 20 deletions(-)
```
- `part0_pp.c`, `part1.c`, `part2.c`, `part3.c`, `part4.c`, `part6_arm.c`, `part6_riscv.c`, `part6_wasm.c`, `doom/`, `src/sqlite3.c`, `tests/embedded_lua/`, `tests/embedded_quickjs/`, `tests/embedded_micropython/`: **0 diff lines**.
- `part5.c`: strictly `25 1 part5.c` (`26` total lines changed, strictly `< 40` Track 6 limit and `< 50` AGENTS limit).

---

## 2. Cryptographic Artifact Identity & PE32+ Audit

| Artifact | Size (bytes) | SHA-256 | Parity Check |
| --- | --- | --- | --- |
| `build_win64/gpu_llm_kernels_host.ptx` (`./zcc`) | `45992` | `99d10c3338762a74d1a17812a956277f345c9cc8a208838171e1a94d2d975961` | Identical (`fc /b`: `FC: no differences encountered`) |
| `build_win64/gpu_llm_kernels.ptx` (`zcc_sov_stage3.exe`) | `45992` | `99d10c3338762a74d1a17812a956277f345c9cc8a208838171e1a94d2d975961` | Identical across `./zcc`, `zcc_sov_stage3.exe`, and WASI `zcc.wasm` |
| `build_win64/zcc_gpu_llm_host_zcc.exe` (`./zcc -mabi=ms`) | `150528` | `cb41200faf38f3edd765ad66cef3b6ff95813bb87d0d2f659515f73fca6e83f6` | Identical (`fc /b`: `FC: no differences encountered`) |
| `build_win64/zcc_gpu_llm.exe` (`zcc_sov_stage3.exe -mabi=ms`) | `150528` | `cb41200faf38f3edd765ad66cef3b6ff95813bb87d0d2f659515f73fca6e83f6` | PE32+ imports strictly `msvcrt.dll` (`47`) + `nvcuda.dll` (`25`) |
| `build_win64/zcc_sov_stage3.exe` | `3075584` | `6e6e9dcab1773614cd2d1524cddcb6519f800953a9472a8546285b630caac8d8` | `stage1 == stage2 == stage3` |
| `build_wasm/zcc.wasm` | `1060797` | `2ac7e978e9f75abd3ec6168eae1b82e6bd6e0f042dc8a929706a498ba5407eb2` | `zcc.wasm == zcc2.wasm == zcc3.wasm` |
| `build_win64/zcc_tiny_llama_f32.gguf` | `364800` | `3255f296b1fe6c36476ad91eba7b9a4a88675f9440cc786d5bbb16555d051f22` | GGUF v3 `F32` |
| `build_win64/zcc_tiny_llama_f16.gguf` | `184576` | `56a6294193013bba64cce4ed439c9ea55f0504758b15545f4c1b8691e88df548` | GGUF v3 `F16` |
| `build_win64/zcc_tiny_llama_q8_0.gguf` | `100096` | `5d571b1bb2517d27e266dac7ca08a66cc533d18ef1acf259b4af5adde0f4e379` | GGUF v3 `Q8_0` (34-byte blocks) |

---

## 3. Live RTX 5070 Full-Vector Differentials & Oracle Decomposition

### Generic Engine vs Pure-Python Reference Oracle (`44` steps, RTX 5070 `cc=12.0`)

| Mode / Model | Prompt | Observed GPU Output | Median `tok/s` | Max Abs Logit Diff vs Pure-Python Oracle | Greedy Token Parity |
| --- | --- | --- | --- | --- | --- |
| `F32` | `"Blackwell "` | `20.3736 ms`, `logits_fnv=7665bc3b`, `top1_id=115`, `top1_logit=16.036377` | `2589.22` | `0.00000608` (`< 1e-4`) | `44 / 44` (`100%`) |
| `F16-TC` (`mma.sync.aligned.m16n8k16`) | `"ZCC compiles "` | `30.8098 ms`, `logits_fnv=227a55f4`, `top1_id=67`, `top1_logit=17.459389` | `1922.48` | `0.00000280` (`< 1e-2`, FP16-act oracle) | `44 / 44` (`100%`) |
| `Q8_0` (GPU 34B block dequant) | `"RTX 5070 "` | `21.0336 ms`, `logits_fnv=99bc6024`, `top1_id=84`, `top1_logit=19.182350` | `2865.89` | `0.00000761` (`< 2e-3`) | `44 / 44` (`100%`) |

### Saved 128-Element `tc_logits.bin` vs `matvec_logits.bin` Full-Vector Audit

```text
elements: 128
max_abs_error: 0.011053800582885742 (at idx 100: TC=3.88913870 vs Matvec=3.87808490)
mean_abs_error: 0.0015565472422167659
rms_error: 0.002469552988655882
max_rel_error: 0.16997513790692254
top1_agreement: True (67 == 67)
top5_tc: [67, 118, 119, 57, 112]
top5_matvec: [67, 118, 119, 57, 112]
allclose_1e-3_1e-2: True (GPU_LLM_F16_PATH_DIFFERENTIAL_OK)
tc_vs_oracle_fp16act_max_abs: 2.7976930141448975e-06 (rms: 3.820031105303901e-07)
matvec_vs_oracle_fp32act_max_abs: 1.2996001729881357e-05 (rms: 3.299703580702541e-06)
oracle_fp16act_vs_fp32act_max_abs: 0.011044170163002853
gpu_delta_vs_oracle_delta_max_abs: 1.261648778072555e-05
```

---

## 4. Gate 1–5 Summary (`test_win64_gpu_llm.py` `8/8`)

```text
test_01_ptx_triple_compiler_byte_identity ... ok
test_02_pe32plus_nvcuda_audit_and_sovereign_parity ... ok
test_03_live_rtx5070_f32_differential_vs_independent_python_oracle ... ok
test_04_live_rtx5070_f16_tensor_core_differential_and_gemm_bench ... ok
test_05_live_rtx5070_q8_0_gpu_dequant_differential ... ok
test_06_multi_prompt_coherence_and_kv_cache_causality ... ok
test_07_ef0_negative_controls (NC-1..NC-10) ... ok
test_08_real_temp_sampling_distribution ... ok
----------------------------------------------------------------------
Ran 8 tests in 5.356s — OK
```

- `make selfhost` (`bc2aa8396a28c351a35d0cf712d81fea`) + `bash scripts/log_bootstrap_hash.sh --check` -> `BOOTSTRAP_HASH_EXIT=0`
- `bash scripts/gate2_interop.sh` -> both directions `INTEROP_SUCCESS: x=10 y=17 z=300`; `track3_compound_assign_c99_regressions.c` -> `11/11 PASS`
- `make test-win64-sovereign-suite` -> Exit `0` (Win64 self-host, Lua+SQLite `6/6`, QuickJS+MicroPython `5/5`, DOOM `6/6`, Track 6 `6/6`, Track 7 `8/8`, Track 8 `6/6`)
- `python3 tests/test_win64_pe_emitter.py -v`: `Ran 39 tests in 4.186s — OK (skipped=5)` (`test_04`, `test_08`, `test_09`, `test_13`, `test_26` skipped in Linux WSL environment; live Win64 execution proven via `test_win64_sovereign_suite`)
- `make selfhost-sovereign`: Exit `0`; `test_wasm_wasi.py -v`: `12/12 OK`; `test_wasm_wasi_selfhost.py -v`: `4/4 OK` (`WASMTIME_STATUS=NOT_INSTALLED_ON_HOST`); `test_arm64_riscv64_direct_elf.py -v`: `5/5 OK` (`QEMU_AARCH64_STATUS=NOT_INSTALLED_ON_HOST`, `QEMU_RISCV64_STATUS=NOT_INSTALLED_ON_HOST`)

---

## 5. `ZCC-EF-7 v1.0.0 §9` Closure Adjudication

| Claim | Evidence Class | Evidence Tier | Evidence | Scope | Limitations | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| Hardened GGUF v3 validation & `--temp` distribution (`NC-1..NC-10`, `test_08`) fail closed and match sampling temperature physics | OBSERVED | E2 | `test_win64_gpu_llm.py` (`8/8` OK, `5.356s`), `128`-element `tc_logits.bin` vs `matvec_logits.bin` audit | `src/win64_gpu_llm.c` (`F32`/`F16`/`Q8_0`) | Uses `ftell()` (`< 2 GiB` GGUF files on LLP64 Win64); BPE tokenizer & `Q4_K_M` deferred to Track 7.1 | **PASS** |
| Live RTX 5070 `F32`/`F16-TC`/`Q8_0` inference & `msvcrt.dll`+`nvcuda.dll`-only PE32+ build parity | OBSERVED | E2 | `test_01..test_06`, `fc /b`, live RTX 5070 differential (`<= 7.61e-6` vs pure-Python oracle) | Sovereign 2-layer Llama GQA checkpoint (`dim=64`, `hidden=128`) | Host `wasmtime` & QEMU static emulators `NOT_INSTALLED_ON_HOST` (`node:wasi` + `BrowserWasiShim` verified instead); Azure CI runner pending | **PASS** |
