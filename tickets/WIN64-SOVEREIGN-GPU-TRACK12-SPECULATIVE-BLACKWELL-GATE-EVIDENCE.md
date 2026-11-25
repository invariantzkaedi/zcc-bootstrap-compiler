# Win64 Sovereign GPU Track 12 — Blackwell SM 12.0 GPU-Chained Speculative Decoding (`1.5B` Draft + `7B` Target) Gate Evidence

**Status:** PASS (`ZCC-EF-7 v1.0.0` Tier `E2`, closed `2026-11-22`)  
**governing_rule_version:** `1.0.0` (`ZCC-EF-0` / `ZCC-EF-7 v1.0.0`)  
**evidence_tier:** `E2`  
**evidence_tier_assigned_at:** `2026-11-22T02:00:00Z` (predates final verification execution `2026-11-22T05:24:30Z`)  
**Git HEAD:** `28f81f658baaf408670db2c6db5f92a4d270d94b`  
**Target Hardware:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, `driver=12090`) on Windows 11 + WSL2 (`Linux 6.18.58.1-microsoft-standard-WSL2 x86_64`, `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`)  
**Toolchain Isolation:** Zero MSVC, zero MinGW, zero GCC/binutils in Windows runtime link, zero `nvcc` / CUDA Toolkit SDK. Strict Win32 PE32+ IAT `{msvcrt.dll, nvcuda.dll}`.

---

## 1. Track 12 Architecture & Modified Files

Track 12 implements zero-toolchain sovereign speculative decoding on Blackwell SM 12.0, pairing `H:\models\zkaedi-prime-dpo-q4_k_m.gguf` (`1.5B` Q4_K_M draft, `980,097,536 B` weights, 28L, dim 1536, vocab 151,936) with `H:\models\zcc-mini-v7-q4_k_m.gguf` (`7B` Q4_K_M target, `4,677,120,000 B` weights, 28L, dim 3584, vocab 152,064) on a single 8 GiB WDDM GPU (`8150 MiB`):

1. **Compiler / SM 12.0 PTX Codegen (`part6_ptx.c`):**
   - Emits GPU-side argmax (`zcc_aot_mc_argmax_f32`) and autonomous GPU-chained draft step (`zcc_aot_mc_draft_chain_step`) avoiding host round-trips per draft token.
   - Emits dynamic `pos0 == 0xFFFFFFFFU` batch RoPE / KV-store / Flash GQA kernels (`McVerifyPlan` reusable across sequence positions).
   - Emits dual fused multi-token GEMM families: register-slimmed 4-row `zcc_aot_mc_matvec_multi4_%s` (`M <= 4`, `.maxntid 128, 1, 1`, `%pt<4>`, `%fa<4>`, `%r<34>`, `%rd<18>`, `%f<15>`, `%fw<4>` via 2-pass Q4_K nibble accumulation) and 8-row `zcc_aot_mc_matvec_multi_%s` (`M <= 8`), replacing `%pm<8>` PTX ISA predicate collisions with `%pt<...>`.
2. **Win64 PE32+ Import Router (`src/win64_pe_emit.c`):**
   - Added `cuMemHostAlloc`, `cuMemHostGetDevicePointer_v2`, `cuMemFreeHost`, `cuStreamWaitEvent`, and `cuMemcpyHtoDAsync_v2` to the strict `nvcuda.dll` import table (alongside `cuStreamCreate`, `cuStreamSynchronize`, `cuStreamDestroy_v2`).
3. **Win64 Sovereign Runtime (`src/win64_gpu_llm.c`):**
   - **Draft-First Hybrid VRAM + WDDM Commit Probe:** Initializes context with `CU_CTX_MAP_HOST` (`8U`), loads 1.5B Draft first (`256 MiB` reserve -> `draft_host_zc_bytes=0`, 100% resident in GDDR7), then runs an 8-iteration commit-verified WDDM allocation probe (`cuMemAlloc_v2` + `cuMemsetD8_v2` + `cuCtxSynchronize` + `cuMemGetInfo_v2` checking `>= 480 MiB` reserve `503316480ULL`) before allocating Target.
   - **Strided Q4_K FFN Spilling + Double-Buffered Async PCIe DMA:** Moves `token_embd.weight` (`~306.56 MB`) to zero-copy pinned host (`cuMemHostAlloc(..., 2U)`), then spills Q4_K `ffn_gate.weight` / `ffn_up.weight` (`38,191,104 B` per matrix) across layers `l >= 2` in stride-4 phase order `{3, 1, 2, 0}` (`target_vram_bytes=4141412352`, `target_host_zc_bytes=535707648`, `spill_slots=2`), overlapping 2-slot ping-pong `cuMemcpyHtoDAsync_v2` transfers on `CU_STREAM_NON_BLOCKING` with per-segment `McVerifySeg` CUDA Graphs (`mc_verify_plan_step`).
   - **Speculative Verification & UX:** Supports `--draft-model`, `--draft-ptx`, `--draft-k` (`1..7`), `--spec-fixed-k`, exact greedy argmax verify (`logits_fnv` parity via unified `matvec_multi` for `M <= 8` in Tier B prefill), Leviathan et al. stochastic speculative rejection sampling, adaptive `k` adjustment (`accepted * 2 < k_step`), skip of unused post-sequence KV catchup (`keep_kv_tail = interactive`), multi-turn `--interactive` / `/reset` / `--clean`, and fail-closed `T12-NC-1..4` validation.
4. **Tests, Build Targets & Baseline Lock:**
   - `tests/test_win64_gpu_track12_spec.py` (6 tests: real `1.5B + 7B` greedy parity & speedup, stochastic determinism & seed sensitivity, multi-turn `--interactive` + `/reset` + `--clean`, synthetic low-acceptance adaptive `K` decay, solo GPU argmax graph vs `--no-cuda-graph` & strict IAT audit, `T12-NC-1..4`).
   - `tests/test_win64_gpu_real_qwen2.py` (updated `test_05` `r_auto` from `--kv-type q8_0` to `--kv-type f32` at line 830 so `AUTO_CTX` clamping `32768 -> 16384` triggers deterministically regardless of idle desktop VRAM headroom).
   - `Makefile` & `BOOTSTRAP_BASELINES.tsv` (lines 260–265 locked to `8b28903bb66619f2c38397ae2a0ddfed`).
   - **Compiler frontend/optimizer integrity:** `part0_pp.c..part5.c` were not modified by Track 12; pre-existing working-tree files outside Track 12 were left untouched.

---

## 2. Artifact Checksums & Strict PE32+ IAT

Captured `2026-11-22T05:24:30Z` at `HEAD=28f81f658baaf408670db2c6db5f92a4d270d94b`:

| Artifact | Size / Details | Checksum |
| --- | --- | --- |
| `zcc2.s` / `zcc3.s` | Self-host assembly (`cmp` exit 0, `elisions=2213`) | MD5 `8b28903bb66619f2c38397ae2a0ddfed` / SHA256 `987ef5ed22c1fd705fc9784256a0b894c58115a506ddf3a216798f0ce3481000` |
| `build_win64/zcc_sov_stage1..3.exe` | Zero-toolchain Win64 self-host (`FC /B` stage1==stage2==stage3) | `3275264 B` / SHA256 `8a5992b394011bfab9ff5958338a0d317772d6781cf7ed3a757ae849d5aacda8` |
| `zcc.wasm` / `zcc2.wasm` / `zcc3.wasm` | WASI self-host (`cmp zcc2.wasm zcc3.wasm` exit 0) | `1187003 B` / SHA256 `ce32c81f853a8352668cafb2eb2e029acaef3284edcec5da60e94faa6d530fb1` |
| `build_win64/gpu_llm_kernels.ptx` | Generic PTX kernels (`FC /B` host==Win64) | `196264 B` / SHA256 `2ff25c6dae066b57dfd51e2a2ccbcced1cd1952fe0bf9e573a43ab18711260f7` |
| `build_win64/zcc_gpu_llm.exe` | Speculative Win64 GPU runtime (`FC /B` host==`zcc_sov_stage3.exe`) | `528896 B` / SHA256 `a09d190247618bde0f37d33ed8919546ff8482b1b332369db43934b600166b4d` |
| `build_win64/zcc_tiny_llama_standalone.exe` | Standalone embedded-GGUF Win64 executable (`FC /B` host==Win64) | `546304 B` / SHA256 `cf078e501e65406cba8e45f8a176ea14f9ca4c384f2e07cb2615dc610b58398a` |
| `build_win64/zcc_mini_7b_sm120.ptx` | 7B AOT SM 12.0 PTX (`0` `%pm` collisions, `multi4` + `multi`) | `137823 B` / SHA256 `a6f45308183f2c32879fbf7357b26b811ecd9b77cd53e5f7cba4a0d26c919007` |
| `build_win64/zkaedi_prime_1_5b_sm120.ptx` | 1.5B Draft AOT SM 12.0 PTX (`0` `%pm` collisions) | SHA256 `49a8c186a359fc243288df59e21f5f81c1f71198be854581deb15af408427677` |

---

## 3. Verbatim Raw Gate Outputs

### 3.1 Gate 1, Bootstrap Ledger, Gate 2 Interop & Nested Compound Regression

```text
2026-11-22T05:24:30Z
28f81f658baaf408670db2c6db5f92a4d270d94b
CMP_ZCC2_ZCC3_EXIT=0
8b28903bb66619f2c38397ae2a0ddfed  zcc2.s
8b28903bb66619f2c38397ae2a0ddfed  zcc3.s
987ef5ed22c1fd705fc9784256a0b894c58115a506ddf3a216798f0ce3481000  zcc2.s
987ef5ed22c1fd705fc9784256a0b894c58115a506ddf3a216798f0ce3481000  zcc3.s
a09d190247618bde0f37d33ed8919546ff8482b1b332369db43934b600166b4d  build_win64/zcc_gpu_llm.exe
cf078e501e65406cba8e45f8a176ea14f9ca4c384f2e07cb2615dc610b58398a  build_win64/zcc_tiny_llama_standalone.exe
a6f45308183f2c32879fbf7357b26b811ecd9b77cd53e5f7cba4a0d26c919007  build_win64/zcc_mini_7b_sm120.ptx
49a8c186a359fc243288df59e21f5f81c1f71198be854581deb15af408427677  build_win64/zkaedi_prime_1_5b_sm120.ptx
BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed  commit=28f81f65  elisions=2213
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
=== 1. ZCC LIB + GCC MAIN ===
[Phase 1] Lexical Array Bootstrap... OK
[Phase 2] AST Topological Generation... OK
[Phase 3] Native AST Constant Folding... OK
[Phase 4] SystemV ABI X86-64 Codegen... OK
[Phase 5] Native C Peephole Optimization... OK (22 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
INTEROP_SUCCESS: x=10 y=17 z=300
=== 2. GCC LIB + ZCC MAIN ===
...
INTEROP_SUCCESS: x=10 y=17 z=300
...
PASS: nested compound assignment evaluated cleanly with zero clobbering
```

### 3.2 Win64 Sovereign Self-Host, WASM Self-Host & Win64 GPU Binary Build + Strict IAT

```text
=== ZCC Sovereign Zero-Toolchain Win64 Self-Host ===
[STAGE 1] Built build_win64/zcc_sov_stage1.exe (3275264 bytes, sha256=8a5992b394011bfab9ff5958338a0d317772d6781cf7ed3a757ae849d5aacda8)
[STAGE 2] Built build_win64/zcc_sov_stage2.exe (3275264 bytes, sha256=8a5992b394011bfab9ff5958338a0d317772d6781cf7ed3a757ae849d5aacda8)
[STAGE 3] Built build_win64/zcc_sov_stage3.exe (3275264 bytes, sha256=8a5992b394011bfab9ff5958338a0d317772d6781cf7ed3a757ae849d5aacda8)
[CONVERGENCE] stage1==stage2: True, stage2==stage3: True
Comparing files BUILD_WIN64\zcc_sov_stage2.exe and BUILD_WIN64\ZCC_SOV_STAGE3.EXE
FC: no differences encountered
[AUDIT] PE32+ OK: .text=0x60000020 .data=0xc0000040 .pdata=15744B .reloc=1576B StackReserve=0x1000000 TimeDateStamp=0 Imports=msvcrt.dll(65)
[PROBE 1] Link-order independence (zcc.s first, bridge.s last): PASS (rc=29)
[PROBE 2] Native >4KB multi-page stack probe + argc/argv: PASS (STK_OK argc=3 a1=arg1 a2=arg2 sum=20589)
[PROBE 3] Native stdio/fgets/getenv + .data funcptr table: PASS (ops=47)
[EF-0 NEGATIVE 1] Invalid C rejected by stage3 (rc=1): PASS
[EF-0 NEGATIVE 2] 1-byte PE mutation caught by SHA-256 & fc /b (rc=1): PASS
=== [PASS] WIN64 SOVEREIGN ZERO-TOOLCHAIN SELF-HOST VERIFIED ===
```

```text
WASMTIME_STATUS: NOT_INSTALLED_ON_HOST
test_01_wasi_runtime_primitives_dual_host ... ok
test_02_wasi_selfhost_convergence ... ok
test_03_browser_wasi_shim_compile_wasm_and_asm ... ok
test_04_ef0_negative_controls ... ok
----------------------------------------------------------------------
Ran 4 tests in 31.241s

OK
```

```text
[HOST-PE:GPU_LLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_llm_host_zcc.exe: 528896 bytes, SHA256=a09d190247618bde0f37d33ed8919546ff8482b1b332369db43934b600166b4d
[WIN64-PE:GPU_LLM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/zcc_gpu_llm.exe: 528896 bytes, SHA256=a09d190247618bde0f37d33ed8919546ff8482b1b332369db43934b600166b4d
[FC /B:GPU_LLM] Comparing files BUILD_WIN64\zcc_gpu_llm_host_zcc.exe and BUILD_WIN64\ZCC_GPU_LLM.EXE
FC: no differences encountered
[PE-AUDIT:GPU_LLM] imports={'bytes': 528896, 'pdata': 3168, 'reloc': 40, 'stack_reserve': 16777216, 'dlls': {'msvcrt.dll': ['__iob_func', '__getmainargs', 'exit', '_strtoi64', '_strtoui64', '_atoi64', '_getpid', '_getcwd', '_errno', 'memcpy', 'floor', 'ceil', 'fmod', 'log', 'pow', 'frexp', 'ldexp', 'exp', 'strtod', 'memset', '_ftime64', '_stricmp', '_strnicmp', '_open', '_close', '_read', '_write', '_lseek', '_mkdir', '_access', 'fprintf', 'free', '_lseeki64', 'fopen', 'malloc', 'fclose', 'fread', 'strcmp', 'realloc', 'memcmp', 'calloc', 'memchr', 'qsort', '_snprintf', 'strlen', 'strcpy', 'strncmp', 'printf', 'putchar', 'fseek', 'ftell', 'strtol', 'fwrite', 'fflush', 'atoi', 'fgets'], 'nvcuda.dll': ['cuGetErrorName', 'cuGetErrorString', 'cuMemFree_v2', 'cuMemFreeHost', 'cuMemGetInfo_v2', 'cuMemAlloc_v2', 'cuMemsetD8_v2', 'cuCtxSynchronize', 'cuMemHostAlloc', 'cuMemHostGetDevicePointer_v2', 'cuMemcpyHtoD_v2', 'cuLaunchKernel', 'cuModuleGetFunction', 'cuGraphCreate', 'cuGraphAddKernelNode', 'cuGraphInstantiateWithFlags', 'cuGraphDestroy', 'cuGraphLaunch', 'cuGraphExecDestroy', 'cuEventRecord', 'cuStreamWaitEvent', 'cuMemcpyHtoDAsync_v2', 'cuEventSynchronize', 'cuEventElapsedTime', 'cuMemcpyDtoH_v2', 'cuModuleLoadDataEx', 'cuModuleUnload', 'cuInit', 'cuDriverGetVersion', 'cuDeviceGetCount', 'cuDeviceGet', 'cuDeviceGetName', 'cuDeviceGetAttribute', 'cuDeviceTotalMem_v2', 'cuCtxCreate_v2', 'cuCtxDestroy_v2', 'cuStreamCreate', 'cuEventCreate', 'cuEventDestroy_v2', 'cuStreamDestroy_v2']}}
[GGUF-EXE:FC-EXE] 546304B SHA256=cf078e501e65406cba8e45f8a176ea14f9ca4c384f2e07cb2615dc610b58398a Comparing files BUILD_WIN64\zcc_tiny_llama_standalone_host_zcc.exe and BUILD_WIN64\ZCC_TINY_LLAMA_STANDALONE.EXE
FC: no differences encountered
[GGUF-EXE:PE-AUDIT] build_win64/zcc_tiny_llama_standalone.exe imports=['msvcrt.dll', 'nvcuda.dll'] bytes=546304
```

### 3.3 Track 12 Suite (`tests/test_win64_gpu_track12_spec.py -v`, `6/6 OK`) & Direct RTX 5070 Telemetry

```text
test_01_real_1_5b_plus_7b_greedy_parity_and_speedup (__main__.TestWin64GpuTrack12Speculative.test_01_real_1_5b_plus_7b_greedy_parity_and_speedup) ... ok
test_02_stochastic_speculative_repeatability_and_validity (__main__.TestWin64GpuTrack12Speculative.test_02_stochastic_speculative_repeatability_and_validity) ... ok
test_03_interactive_multiturn_spec_recall_reset_and_clean (__main__.TestWin64GpuTrack12Speculative.test_03_interactive_multiturn_spec_recall_reset_and_clean) ... ok
test_04_synthetic_worst_case_low_acceptance_parity_and_adaptive_k (__main__.TestWin64GpuTrack12Speculative.test_04_synthetic_worst_case_low_acceptance_parity_and_adaptive_k) ... ok
test_05_solo_gpu_argmax_parity_no_cuda_graph_and_strict_iat (__main__.TestWin64GpuTrack12Speculative.test_05_solo_gpu_argmax_parity_no_cuda_graph_and_strict_iat) ... ok
test_06_negative_controls_nc1_to_nc4 (__main__.TestWin64GpuTrack12Speculative.test_06_negative_controls_nc1_to_nc4) ... ok

----------------------------------------------------------------------
Ran 6 tests in 107.331s

OK

[T12-1 english] solo=46.48 tok/s | spec=40.00 tok/s (0.86x) | acc=38.5% | cycles=37 k_end=2 | tokens=64

[T12-1 c_code] solo=46.49 tok/s | spec=49.71 tok/s (1.07x) | acc=75.0% | cycles=43 k_end=3 | tokens=128

[T12-1 c_sort] solo=48.10 tok/s | spec=52.88 tok/s (1.10x) | acc=84.0% | cycles=35 k_end=2 | tokens=119
```

Direct Solo vs Spec (`--draft-k 3 --kv-type q8_0 --ctx 2048 --prefill-batch 8 --chat`) verbatim runs on the final binary:

```text
=== SPEC c_code k=3 steps=64 ===
GPU_LLM_WEIGHT_MEM target_vram_bytes=4141412352 target_host_zc_bytes=535707648 draft_vram_bytes=980097536 draft_host_zc_bytes=0 spill_slots=2 spill_stage_bytes=38191104
GPU_LLM_PREFILL batch=8 prompt_tokens=22 chunk_count=3 tc_prefill=1 prefill_ms=257.6889 prefill_tok_per_sec=85.37
GPU_LLM_OK steps=64 total_gpu_ms=1614.3960 tok_per_sec=53.27 top1_id=8420 top1_logit=23.253841 logits_fnv=af2e13ad
GPU_LLM_SPEC draft_k=3 final_k=3 adaptive=1 cycles=20 proposed=56 accepted=42 accept_rate=0.7500 verify_launches=20

=== SOLO c_code steps=128 ===
GPU_LLM_PREFILL batch=8 prompt_tokens=22 chunk_count=3 tc_prefill=1 prefill_ms=169.1602 prefill_tok_per_sec=130.05
GPU_LLM_OK steps=128 total_gpu_ms=3277.7693 tok_per_sec=45.76 top1_id=8420 top1_logit=23.253841 logits_fnv=af2e13ad

=== SPEC c_code k=3 steps=128 ===
GPU_LLM_WEIGHT_MEM target_vram_bytes=4141412352 target_host_zc_bytes=535707648 draft_vram_bytes=980097536 draft_host_zc_bytes=0 spill_slots=2 spill_stage_bytes=38191104
GPU_LLM_PREFILL batch=8 prompt_tokens=22 chunk_count=3 tc_prefill=1 prefill_ms=255.6513 prefill_tok_per_sec=86.05
GPU_LLM_OK steps=128 total_gpu_ms=3053.0645 tok_per_sec=49.13 top1_id=8420 top1_logit=23.253841 logits_fnv=af2e13ad
GPU_LLM_SPEC draft_k=3 final_k=3 adaptive=1 cycles=43 proposed=112 accepted=84 accept_rate=0.7500 verify_launches=43

=== SOLO c_sort steps=128 ===
GPU_LLM_PREFILL batch=8 prompt_tokens=26 chunk_count=4 tc_prefill=1 prefill_ms=196.1350 prefill_tok_per_sec=132.56
GPU_LLM_OK steps=119 total_gpu_ms=2997.9197 tok_per_sec=48.37 top1_id=1004 top1_logit=24.291531 logits_fnv=ed53d3fe

=== SPEC c_sort k=3 steps=128 ===
GPU_LLM_WEIGHT_MEM target_vram_bytes=4141412352 target_host_zc_bytes=535707648 draft_vram_bytes=980097536 draft_host_zc_bytes=0 spill_slots=2 spill_stage_bytes=38191104
GPU_LLM_PREFILL batch=8 prompt_tokens=26 chunk_count=4 tc_prefill=1 prefill_ms=322.1687 prefill_tok_per_sec=80.70
GPU_LLM_OK steps=119 total_gpu_ms=2729.8259 tok_per_sec=53.12 top1_id=1004 top1_logit=24.291531 logits_fnv=ed53d3fe
GPU_LLM_SPEC draft_k=3 final_k=2 adaptive=1 cycles=35 proposed=100 accepted=84 accept_rate=0.8400 verify_launches=35
```

- **Decode-only speedup comparison (`total_gpu_ms - prefill_ms`):**
  - `c_code` (`steps=128`): Solo `3108.61 ms` vs Spec `2797.41 ms` (`1.111x`, `+11.1%` faster decode; `1.074x` end-to-end).
  - `c_sort` (`steps=119` EOS): Solo `2801.78 ms` vs Spec `2407.66 ms` (`1.164x`, `+16.4%` faster decode; `1.098x` end-to-end).
  - Exact 100% greedy token ID parity and bit-identical `logits_fnv` (`af2e13ad` on `c_code`, `ed53d3fe` on `c_sort`, `e664dd14` on `english`) between Solo 7B and Speculative `1.5B + 7B`.

- **Negative Controls (`T12-NC-1..4`, `test_06`):**
  - `T12-NC-1`: `--draft-model` without `--draft-ptx` (and vice versa) -> non-zero exit + `INVALID_ARG`.
  - `T12-NC-2`: `--draft-k 0` and `--draft-k 8` -> non-zero exit + `INVALID_ARG`.
  - `T12-NC-3`: Tier A PTX (`zcc_tiny_llama_f16.gguf` compiled to `zcc_track12_tiera_nc_sm120.ptx`) passed as `--draft-ptx` -> non-zero exit + `SPEC_REQUIRES_TIER_B`.
  - `T12-NC-4`: Vocab-mismatched draft (`zcc_hf_llama_spm_q4_0.gguf`, Llama SPM) paired with Qwen2 BPE target -> non-zero exit + `SPEC_VOCAB_MISMATCH`.

### 3.4 Track 7–11 Regression Suite (`34/34 OK`, Combined Tracks 7–12 = `40/40 OK`)

```text
tests/test_win64_gpu_llm.py       : Ran 8 tests in 5.630s   -> OK (8/8)
tests/test_win64_gpu_aot_gguf.py  : Ran 6 tests in 4.029s   -> OK (6/6)
tests/test_win64_gpu_gguf_v2.py   : Ran 7 tests in 25.532s  -> OK (7/7)
tests/test_win64_gpu_track10.py   : Ran 7 tests in 11.644s  -> OK (7/7)
tests/test_win64_gpu_real_qwen2.py: Ran 6 tests in 145.013s -> OK (6/6)
```

Exact Track 11 (`test_win64_gpu_real_qwen2.py`) telemetry on the final binary:
```text
Ran 6 tests in 145.013s

OK
[T11-T01] prime_1_5b: gguf=986048032B ptx=137346B sha256=49a8c186a359fc24...
[T11-T01] trinity_1_5b_f16: gguf=3093668896B ptx=78664B sha256=a34d7bba02d856d8...
[T11-T01] mini_7b: gguf=4683073536B ptx=137823B sha256=a6f45308183f2c32...
[T11-T02] f32_pb1: top1=785==785 top5_overlap=5/5 max_diff=0.00002 mean_diff=0.000003 tok/s=174.73 gen='The capital of France is Paris.'
[T11-T02] f16_pb64: top1=785==785 top5_overlap=5/5 max_diff=0.01149 mean_diff=0.001590 tok/s=283.94 gen='The capital of France is Paris.'
[T11-T03] math='42' repl_recall='7391' reset_after='UNKNOWN'
[T11-T04] trinity F16 TC: tok/s=200.80 gen='Tokyo is the capital of Japan.'
[T11-T05] 7B >4GiB: tok/s=87.61 wblob=4677120000 gen='Paris.'
```

### 3.5 Gate 5 Cross-Backend Regression Matrix

| Suite | Command | Result |
| --- | --- | --- |
| Win64 PE32+ Emitter | `python3 tests/test_win64_pe_emitter.py -v` | **PASS** (`Ran 39 tests in 4.524s`, `OK (skipped=5)` — Win32 host execution skipped inside WSL2) |
| Sovereign Self-Host ELF/Obj | `make selfhost-sovereign` | **PASS** (`zcc2_sov.o == zcc3_sov.o`, `sov_abi2 == sov_abi3`, `p=0x1234 d=504.000000 x=42`) |
| WASI Preview 1 | `python3 tests/test_wasm_wasi.py -v` | **PASS** (`Ran 12 tests in 1.408s`, `OK`) |
| ARM64 & RISC-V Direct ELF | `python3 tests/test_arm64_riscv64_direct_elf.py -v` | **PASS** (`Ran 5 tests in 6.230s`, `OK`; `qemu-aarch64-static` / `qemu-riscv64-static` not installed on host) |

---

## 4. `ZCC-EF-7 v1.0.0 §9` Adjudication Table

| Claim | Evidence Class | Evidence Tier | Evidence | Scope | Limitations | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| ZCC compiles Blackwell SM 12.0 GPU-chained speculative decoding (`zcc_aot_mc_argmax_f32`, `zcc_aot_mc_draft_chain_step`, dynamic `pos0` batch kernels, `multi4` + `multi` GEMM) and executes real `1.5B + 7B` Q4_K_M speculative decoding on a single 8 GiB RTX 5070 with zero MSVC/CUDA SDK, exact greedy token + `logits_fnv` parity, stochastic repeatability, interactive `/reset`/`--clean` support, fail-closed negative controls, and full regression stability across Tracks 7–12 and Gates 1–5. | `OBSERVED` (all compiler runs, `fc /b` / SHA-256 checks, RTX 5070 benchmarks, Track 7–12 `40/40` tests, and Gate 1/2/5 runs executed in this session). | `E2` (`governing_rule_version=1.0.0`, `evidence_tier_assigned_at=2026-11-22T02:00:00Z`). | `make selfhost` (`8b28903bb66619f2c38397ae2a0ddfed`, `cmp` exit 0), `log_bootstrap_hash.sh --check` (`0`), `gate2_interop.sh` (`INTEROP_SUCCESS` both directions), `win64_selfhost_sovereign.py` (`PASS`), `test_wasm_wasi_selfhost.py` (`4/4 OK`), `build_gpu_llm_win64.py` (`FC /B` identical, strict `{msvcrt.dll, nvcuda.dll}` IAT), `test_win64_gpu_track12_spec.py` (`6/6 OK`, `c_code` `1.07x` / `c_sort` `1.10x`), Tracks 7–11 (`34/34 OK`), Gate 5 (`39+12+5` + `selfhost-sovereign`). | Windows 11 + NVIDIA RTX 5070 Laptop GPU (`8150 MiB`, `cc=12.0`, `driver=12090`), WSL2 Ubuntu (`gcc 15.2.0`), real GGUF models `H:\models\zkaedi-prime-dpo-q4_k_m.gguf` + `H:\models\zcc-mini-v7-q4_k_m.gguf`. | 1) On the `8150 MiB` WDDM GPU, simultaneous `1.5B` (`980 MB`) + `7B` (`4.68 GB`) + KV (`93.6 MB`) + workspaces hits WDDM's committed dedicated-VRAM cliff (~`4.65–4.75 GiB`), requiring `535.7 MB` pinned host spillover on Target (`306.6 MB` `token_embd.weight` + 6× `38.19 MB` strided Q4_K FFN matrices overlapped via `spill_slots=2` async `cuMemcpyHtoDAsync_v2`, leaving `draft_host_zc_bytes=0`). 2) Net wall-clock speedup depends on draft/target alignment: structured C prompts achieve `75–84%` acceptance and `+7–10%` end-to-end (`+11–16%` decode) speedup, whereas unaligned cross-DPO English prose (`38.5%` acceptance) incurs net verification overhead (`0.86x`) while preserving 100% lossless token and `logits_fnv` parity. 3) `test_win64_pe_emitter.py` skips 5 Win32-host tests inside WSL2, `wasmtime` and `qemu-{aarch64,riscv64}-static` are absent on host, and pre-existing uncommitted non-Track-12 edits remain untouched in the working tree. | **PASS** |
