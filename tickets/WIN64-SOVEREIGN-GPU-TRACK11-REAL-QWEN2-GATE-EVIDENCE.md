# Win64 Sovereign GPU Track 11 — Real Trained Qwen2.5 Models (`sm_120` + NeoX RoPE + 152K BPE + `> 4 GiB` Streaming + Clean REPL) Gate Evidence

**Status:** PASS (`ZCC-EF-7 v1.0.0` Tier `E2`, closed `2026-11-22`)  
**Governing Rule:** `ZCC-EF-0` / `ZCC-EF-7 v1.0.0` (`evidence_tier_assigned_at=2026-11-22T01:42:00Z`)  
**Target Hardware:** NVIDIA GeForce RTX 5070 Laptop GPU (`cc=12.0`, `vram_mib=8150`, `driver=12090`) on Windows 11 + WSL2 (`Linux 6.18.58.1-microsoft-standard-WSL2 x86_64`)  
**Toolchain Isolation:** Zero MSVC, zero MinGW, zero GCC/binutils in Windows runtime link, zero `nvcc` / CUDA Toolkit SDK. Strict Win32 IAT `{msvcrt.dll, nvcuda.dll}`.

---

## 1. Track 11 Engineering Scope & Root-Cause Fixes

Track 11 extended ZCC's zero-toolchain sovereign C99->PTX AOT compiler (`part6_ptx.c`), Generic PTX kernel suite (`src/win64_gpu_llm_kernels.c`), and Win64 PE32+ CUDA Driver runtime (`src/win64_gpu_llm.c`) from synthetic test fixtures to **real multi-gigabyte trained Hugging Face Qwen2/Qwen2.5 `.gguf` weights**:

1. **NeoX (Split-Half) RoPE Fix Across All 3 Backends (`part6_ptx.c`, `src/win64_gpu_llm_kernels.c`, `src/win64_gpu_llm.c`, `tools/generate_gguf_{v2_fixtures,track10_bench}.py`):**
   - `arch=qwen2` uses split-half pairs `(p, p + head_dim/2)` (`+256B` F32 / `+128B` F16 stride for `head_dim=128`), whereas Llama uses interleaved pairs `(2p, 2p+1)` (`+4B` F32).
   - Fixed compile-time stride selection (`is_neox = strcmp(m->arch, "qwen2") == 0`) in Tier B `zcc_aot_mc_rope_*` decode & prefill kernels, added `llm_rope_neox_f32` & `llm_rope_neox_batch_f32` in Generic PTX kernels, and updated Python reference oracles.
2. **Qwen2 152K GPT-2 BPE + Dual Stop Token + Fast Lossless Sampling (`src/win64_gpu_llm.c`):**
   - Updated `next_bpe_pretoken_len` for leading non-alphanumeric contraction/letter runs (`[^\r\n\p{L}\p{N}]?\p{L}+`) and trailing newlines on punctuation (` ?[^\s\p{L}\p{N}]+[\r\n]*`).
   - Added dual Qwen2 EOS handling (`<|im_end|>` + `<|endoftext|>`), multi-turn `<|im_end|>\n` delimiter in interactive ChatML prompts, and an `O(V)` candidate pre-filter in `sample_next_token` for `vocab_size > 4096`.
3. **`> 4 GiB` GGUF Streaming, Auto-Context VRAM Fit, Bug Fixes & Clean REPL (`part6_ptx.c`, `src/win64_gpu_llm.c`):**
   - 256 KiB chunked buffer + 4x unrolled FNV-1a streaming hash for 1–5 GB GGUFs (`_open` + `_lseeki64` in runtime).
   - Fixed `mc_emit_linear` node insertion ordering before `p->fn_f32_to_f16` and raised `mc_plan_init` `cap_nodes` headroom (`24 * n_layers + 16`).
   - Added `AUTO_CTX` halving down to `256` when explicit `--ctx` is omitted (e.g., native `32768` on 7B inside 8 GiB VRAM), while failing closed when an explicit `--ctx` exceeds VRAM.
   - Added `--clean` REPL mode (`Assistant> ` streaming on `stdout`, startup/status + `You> ` prompts on `stderr`).

### File Accounting (Zero Untracked Changes to Frontends/Emitters)
- Modified/created by Track 11:
  - `part6_ptx.c`
  - `src/win64_gpu_llm_kernels.c`
  - `src/win64_gpu_llm.c`
  - `tools/generate_gguf_v2_fixtures.py`
  - `tools/generate_gguf_track10_bench.py`
  - `tests/test_win64_gpu_real_qwen2.py` (new, 6 tests)
  - `Makefile` (`build-gpu-real-qwen2-win64`, `test-win64-gpu-real-qwen2`)
  - `BOOTSTRAP_BASELINES.tsv` (Track 11 WSL2 & Azure lock rows)
  - `tickets/WIN64-SOVEREIGN-GPU-TRACK11-REAL-QWEN2-GATE-EVIDENCE.md` (this report)
- **Untouched core compiler disclosure:** `part0_pp.c`, `part1.c`, and `part3.c` have **0 lines** changed. Pre-existing external uncommitted edits on `part2.c` (`12` lines), `part4.c` (`4` lines), `part5.c` (`77` lines), and `src/win64_pe_emit.c` (`42` lines) were preserved untouched by Track 11.

---

## 2. Gate 1, Gate 2 & Cross-Platform Sovereign Compiler Convergence

| Gate | Command | Result | Artifact / Checksum |
| --- | --- | --- | --- |
| **Gate 1 (Self-Host)** | `make zcc && make selfhost` | **PASS** (`stage2.s == stage3.s`, `cmp` exit 0, `2213 elided`) | `zcc2.s` MD5: `2bed4da749e4d9e4896bae7cf3e0d7c4`, SHA256: `acca9f9cbe67e34972eb792f03598354a53b1bcb9e8f35a715fdd59450db479b` |
| **Bootstrap Ledger** | `bash scripts/log_bootstrap_hash.sh --check` | **PASS** (`BOOTSTRAP_HASH_EXIT=0`, `commit=28f81f65`) | Locked in `BOOTSTRAP_BASELINES.tsv` |
| **Gate 2 (Interop + Regress)** | `bash scripts/gate2_interop.sh` + `regress_nested_compound_assign.c` | **PASS** (`INTEROP_SUCCESS: x=10 y=17 z=300` both ways) | `PASS: nested compound assignment` |
| **Win64 Sovereign Self-Host** | `python3 tools/win64_selfhost_sovereign.py` | **PASS** (`stage1 == stage2 == stage3`, `fc /b` identical, PE32+ & EF-0 clean) | `build_win64/zcc_sov_stage3.exe` (`3,229,696 B`) SHA256: `6e96da2ddea1afd7564950da00ae1b3da37a685f5b7adcf8a55e5a6566cbfaaf` |
| **WASI Self-Host** | `python3 tests/test_wasm_wasi_selfhost.py -v` | **PASS** (`4/4 OK`, `zcc.wasm == zcc2.wasm == zcc3.wasm`) | `zcc.wasm` (`1,157,611 B`) SHA256: `58c22c212a208bf4a458664616874e5f2db74910656f8eac3f728ba93415faaf` |
| **Win64 GPU Runtime + Kernels** | `python3 tools/build_gpu_llm_win64.py` | **PASS** (`fc /b` identical host vs `zcc_sov_stage3.exe`) | `zcc_gpu_llm.exe` (`440,832 B`) SHA256: `8a823100046a368ecef6a9f2d043aba7bf4b3619c1ebf89363b25193b9aead3b`; `gpu_llm_kernels.ptx` (`196,264 B`) SHA256: `2ff25c6dae066b57dfd51e2a2ccbcced1cd1952fe0bf9e573a43ab18711260f7` |

**PE32+ Strict IAT Audit (`build_win64/zcc_gpu_llm.exe`):**  
`allowed_dlls == required_dlls == {'msvcrt.dll', 'nvcuda.dll'}` (`.pdata=2808B`, `.reloc=12B`, `stack_reserve=16777216B`).

---

## 3. Track 7–10 GPU Regression Verification (`28/28 OK`)

All prior sovereign GPU test suites passed after NeoX RoPE updates:
- `python3 tests/test_win64_gpu_llm.py -v`: **8/8 OK** (`7.605s`)
- `python3 tests/test_win64_gpu_aot_gguf.py -v`: **6/6 OK** (`5.028s`)
- `python3 tests/test_win64_gpu_gguf_v2.py -v`: **7/7 OK** (`24.868s`)
- `python3 tests/test_win64_gpu_track10.py -v`: **7/7 OK** (`13.582s`)

---

## 4. Track 11 Real Trained Qwen2.5 Models (`test_win64_gpu_real_qwen2.py`, `6/6 OK`, `101.977s`)

### 4.1 Real Models Compiled & Executed on RTX 5070

| Model (`H:\models\`) | Size (B) | Architecture / Types | Emitted PTX (`fc /b` host==Win64) | Live RTX 5070 Observed Output & Speed |
| --- | --- | --- | --- | --- |
| `zkaedi-prime-dpo-q4_k_m.gguf` | `986,048,032` | `qwen2`, 28L, dim 1536, hidden 8960, 12Q/2KV, vocab 151936, `Q4_K`/`Q6_K`/`F32` | `82,966 B`, `tc=0`, `rope=neox`, SHA256 `a349813923667d42a3999e803b5d67c702fed03ae777e52408be032c0f0b5a34` | `"Paris is the capital of France."` (**355.56 tok/s**, `prefill_tok/s=563.61`, `launches_per_token=1`, `graph_nodes=423`) / `"The capital of France is Paris."` (`283.11 tok/s` `f16` KV; `174.83 tok/s` `f32` KV) |
| `zkaedi-trinity-dpo-f16.gguf` | `3,093,668,896` | `qwen2`, 28L, dim 1536, hidden 8960, 12Q/2KV, vocab 151936, full `F16` (`tc=1`) | `67,722 B`, `tc=1`, `rope=neox`, SHA256 `b8a0966612b7ff9e96d1e09b7e3f3db677d0e6939f565c208c55f8875b27f3d5` | `"Tokyo is the capital of Japan."` (**215.15 tok/s**, `tensor_core=1`, `wtype=F16`, `kv=q8_0`, `launches_per_token=1`) |
| `zcc-mini-v7-q4_k_m.gguf` | `4,683,073,536` (`> 4 GiB`) | `qwen2`, 28L, dim 3584, hidden 18944, 28Q/4KV, vocab 152064, `Q4_K`/`Q6_K`/`F32` | `83,443 B`, `tc=0`, `rope=neox`, SHA256 `173cb68b45ceba1b9993e47f8b9bcc96cad2760f2bb1670e35c473e6cb3c2d03` | `"Paris."` (**94.60 tok/s**, `wblob=4677120000`, `launches_per_token=1`) + `AUTO_CTX: clamped context 32768 ->` |

### 4.2 Streaming NumPy Differential Oracle (1.5B `zkaedi-prime-dpo-q4_k_m.gguf`, 151,936 Logits)
Prompt: `"What is the capital of France?"` (ChatML + Qwen2 BPE):
- `f32` KV (`prefill-batch=1`): `top1=785==785`, `top5_overlap=5/5`, **`max_diff=0.00002`**, `mean_diff=0.000003`, `tok/s=174.83`, `gen='The capital of France is Paris.'`
- `f16` KV (`prefill-batch=64`): `top1=785==785`, `top5_overlap=5/5`, **`max_diff=0.01149`**, `mean_diff=0.001590`, `tok/s=283.11`, `gen='The capital of France is Paris.'`

### 4.3 Live Functional & Clean Interactive REPL Checks (`test_03`)
- Arithmetic: `"What is 17 + 25? Answer with just the number."` -> `'42'`
- C codegen: `"Write a 3-line C function int add(int a, int b) that returns their sum."` -> valid `int add(...)` with `return`
- Interactive KV memory + `/reset` (`--interactive --chat`): recalled secret `"7391"` in Turn 1, cleared KV (`GPU_LLM_REPL_RESET kv_cleared=1 pos=0`) and answered `"UNKNOWN"` after `/reset`, emitted `You> ` to `stderr`.
- Clean UX (`--clean --interactive`): streamed `Assistant> Tokyo...` on `stdout`, zero `GPU_LLM_*` metadata on `stdout`, and printed `GPU_LLM_MODEL` + `You> ` + `gen tok` status on `stderr`.

### 4.4 `ZCC-EF-0` Negative Controls (`test_06`)
- **`T11-NC-1` (NeoX RoPE Stride Discrimination):** Verified `zkaedi_prime_1_5b_sm120.ptx` carries `rope=neox` and `[%rd12+256]` (and zero `[%rd12+4]`), while Tier B Llama (`zcc_hf_llama_spm_q4_0_sm120.ptx`) carries `rope=norm` and `[%rd12+4]` (and zero `[%rd12+256]`).
- **`T11-NC-2` (Cross-Model Fail-Closed Guard):** Pairing `zkaedi_prime_1_5b_sm120.ptx` with `zkaedi-trinity-dpo-f16.gguf` exits non-zero with `AOT_FINGERPRINT_MISMATCH`.
- **`T11-NC-3` (IAT & Explicit `--ctx` Bounds):** Strict `{msvcrt.dll, nvcuda.dll}` IAT verified; explicit `--ctx 131072` on `zkaedi-prime-dpo-q4_k_m.gguf` exits non-zero with `VRAM_BUDGET_EXCEEDED` (confirming `AUTO_CTX` never overrides explicit user `--ctx`).

---

## 5. Gate 5 Cross-Backend Regression Matrix

| Suite | Command | Result |
| --- | --- | --- |
| Win64 PE32+ Emitter | `python3 tests/test_win64_pe_emitter.py -v` | **PASS** (`39 tests`, `OK`, `skipped=5` Win32-host skips in WSL2) |
| Sovereign Self-Host ELF/Obj | `make selfhost-sovereign` | **PASS** (`zcc2_sov.o == zcc3_sov.o`, `sov_fp2 == sov_fp3`, `sov_abi2 == sov_abi3`) |
| WASI Preview 1 | `python3 tests/test_wasm_wasi.py -v` | **PASS** (`12/12 OK` in `1.191s`) |
| ARM64 & RISC-V Direct ELF | `python3 tests/test_arm64_riscv64_direct_elf.py -v` | **PASS** (`5/5 OK` in `5.966s`) |

---

## 6. `ZCC-EF-7 v1.0.0 §9` Closure Verdict

| Field | Value |
| --- | --- |
| **Claim** | ZCC compiles real trained Qwen2/Qwen2.5 `.gguf` models (1.5B Q4_K_M, 1.5B full F16 SM 12.0 Tensor Core, and 4.68 GB `> 4 GiB` 7B Q4_K_M) into byte-identical host/Win64 `sm_120` PTX and executes them natively on an RTX 5070 via strict `{msvcrt.dll, nvcuda.dll}` Win64 PE32+ runtime with coherent English/math/C output and differential NumPy logit parity. |
| **Evidence Class** | `OBSERVED` (all compiler runs, `fc /b` / SHA-256 checks, RTX 5070 executions, streaming NumPy oracle comparisons, and Gate 5 suites executed in session). |
| **Evidence Tier** | `E2` (`evidence_tier_assigned_at=2026-11-22T01:42:00Z`). |
| **Evidence** | Gate 1 `make selfhost` (`2bed4da749e4d9e4896bae7cf3e0d7c4`), `log_bootstrap_hash.sh --check` (`0`), Gate 2 interop, Win64 & WASI self-host, PE32+ IAT audit, Track 7–10 (`28/28 OK`), Track 11 (`6/6 OK`, `max_diff=0.00002` `f32` / `0.01149` `f16` across 151,936 logits), Gate 5 (`39+12+5` tests + `selfhost-sovereign`). |
| **Scope** | Local workstation Windows 11 + RTX 5070 (`cc=12.0`), WSL2 Ubuntu (`gcc 15.2.0`), real trained models under `H:\models\*.gguf`. |
| **Limitations** | 1) Streaming NumPy differential logit comparison covered the 986 MB 1.5B model (`zkaedi-prime-dpo-q4_k_m.gguf`); 3.09 GB F16 and 4.68 GB 7B models were validated by dual-compiler PTX parity + live English/factuality + `tensor_core=1` + `AUTO_CTX` rather than CPU float32 full-vocab dump due to host RAM bounds. 2) `test-win64-gpu-real-qwen2` requires external `H:\models\*.gguf` files on disk and is intentionally excluded from `test-win64-sovereign-suite`. 3) Pre-existing external uncommitted edits on `part2.c`/`part4.c`/`part5.c`/`src/win64_pe_emit.c` remain in the working tree untouched. |
| **Verdict** | **PASS** |
