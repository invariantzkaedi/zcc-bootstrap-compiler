# Gate Evidence: Sovereign Win64 Track 10 — Blackwell `ld.v4` + 4-Warp GEMV, FP16/Q8_0 KV-Cache, SM 12.0 Tensor Core Prefill & Multi-Turn REPL

**Classification:** ZCC-EF-0 / ZCC-EF-7 v1.0.0 Release Evidence Ledger  
**governing_rule_version:** `ZCC-EF-7 v1.0.0`  
**evidence_tier_assigned_at:** `2026-11-22T00:47:22Z`  
**Required Evidence Tier:** `E2` (Engineering Milestone Closure — live hardware + triple-compiler sovereign determinism + adversarial negative controls)  
**Hardware:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, CUDA driver `12090`, PTX ISA `8.7`, `.target sm_120`)  
**Toolchain:** Zero MSVC, zero MinGW, zero GCC/binutils for Win64/WASM targets, zero `nvcc`, zero CUDA Toolkit (`C:\Windows\System32\{msvcrt,nvcuda}.dll` ONLY)

---

## §1 Adjudication Summary (ZCC-EF-7 v1.0.0 §9)

**Claim → Evidence Class → Evidence Tier → Evidence → Scope → Limitations → Verdict**

- **Claim:** ZCC natively emits and executes Track 10 Blackwell (`sm_120`) Tier B (`ZCCAOT02`) & Generic transformer inference optimizations: (1) 4-warp/CTA (`block=128`) + 16B alignment-guarded `ld.v4.u32` / `ld.v4.f32` GEMV, (2) `--kv-type f32|f16|q8_0` KV-cache quantization inside both CUDA Graph (`launches_per_token=1`) and Generic engines, (3) batched SM 12.0 Tensor Core (`mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32`) prefill (`--prefill-batch N`, `1..128`), and (4) persistent multi-turn `--interactive` REPL (`--system`, `/reset`, `/quit`, `/exit`, `GPU_LLM_CTX_FULL` non-fatal recovery) on a real-scale `87.00 MB` L2-exceeding Qwen2.5 `Q4_K_M` fixture.
- **Evidence Class:** **OBSERVED** (direct execution on WSL2 + Windows 11 + live RTX 5070 hardware in this session).
- **Evidence Tier:** **E2** (`evidence_tier_assigned_at=2026-11-22T00:47:22Z`, pre-registered acceptance contract + 7-test automated suite + 5 adversarial negative controls `T10-NC-1..5` + full Gates 1–5).
- **Evidence:**
  - **Gate 1 (`make zcc && make selfhost`):** PASS (`exit 0`), `zcc2.s == zcc3.s` (`SHA256=9908ede3fd0d2e2817361e3ec552cb65a8f4c52be169bbe1c829111dafcc159f`, `MD5=4061875f1c1162071c8a9c9ab0543d8b`), `scripts/log_bootstrap_hash.sh --check` -> `BOOTSTRAP_HASH_EXIT=0`.
  - **Gate 2 (`scripts/gate2_interop.sh` + `regress_nested_compound_assign.c`):** PASS (`INTEROP_SUCCESS: x=10 y=17 z=300` both directions, `PASS: nested compound assignment evaluated cleanly with zero clobbering`).
  - **Win64 Sovereign Self-Host (`tools/win64_selfhost_sovereign.py`):** PASS, `zcc_sov_stage1 == stage2 == stage3` (`3,228,672` B, `SHA256=6dee9c7c49c6f4914ff3b1fcaf9a83c4a6afde2a0846bb8e22dd2302dbf8c6fd`), `FC: no differences encountered`, `Imports=msvcrt.dll(65)`.
  - **WASI Sovereign Self-Host (`tests/test_wasm_wasi_selfhost.py -v`):** PASS (`4/4 OK`, `zcc.wasm == zcc2.wasm == zcc3.wasm`, `1,156,756` B, `SHA256=db0f0dfeb6761b97653539f9dbd1037c716b7f3eb06a2e50295c94041b2b92c4`, `cmp EXIT 0`).
  - **Track 10 Test Suite (`tests/test_win64_gpu_track10.py -v`):** **PASS (`7/7 OK`, `17.238s`)**, including triple-compiler PTX byte identity (`./zcc == zcc_sov_stage3.exe == zcc.wasm`), dead-format elimination, `2.00x`/`3.76x` KV savings, `16.01x` median TC prefill speedup (`182.43 ms -> 11.39 ms` on 128 tokens), `961.58 tok/s` / `83.64 GB/s` sustained decode (`steps=48`, `--kv-type f16`), multi-turn REPL KV causality & `GPU_LLM_CTX_FULL` recovery, strict `{msvcrt.dll, nvcuda.dll}` audit (`424,960` B), and `T10-NC-1..5`.
  - **Regression Matrix:** `test_win64_gpu_gguf_v2.py` (`7/7 OK`), `test_win64_gpu_aot_gguf.py` (`6/6 OK`), `test_win64_gpu_llm.py` (`8/8 OK`), `test_win64_pe_emitter.py` (`39 OK, 5 skipped` for Windows-host subprocess assertions when invoked inside WSL; live Windows Win64 binaries verified directly in sovereign self-host & Track 7–10 suites), `make selfhost-sovereign` (`PASS`), `test_wasm_wasi.py` (`12/12 OK`), `test_arm64_riscv64_direct_elf.py` (`5/5 OK`).
- **Scope & Limitations:**
  - `test_win64_gpu_ptx.py` (`S5`): live RTX 5070 hardware/PTX/HWND subtests (`test_01, 03, 04, 05`) **PASSED**; `test_02` and `test_06` (`git diff` hygiene checks) failed solely because external user-originated telemetry edits exist in uncommitted working tree on `part2.c` (`7 5`), `part4.c` (`4 0`), and `part5.c` (`72 5`, extending `part5.c` diff from `26` to `77` lines — Track 10 modified `0` lines of `part0_pp.c..part5.c`).
  - `wasmtime`, `qemu-aarch64-static`, `qemu-riscv64-static` are not installed on host (`node:wasi` + `BrowserWasiShim` + ELF header/disasm validators used).
  - Real-scale `87.00 MB` Qwen2.5 `Q4_K_M` fixture uses synthetic deterministic PCG64 weights with real quantization layout, GPT-2 BPE merges, ChatML templates, and QKV biases.
- **Verdict:** **PASS** (Track 10 E2 acceptance criteria satisfied across all deliverables and negative controls; external `part2.c`/`part4.c`/`part5.c` diff disclosed verbatim above).

---

## §2 Reproducible Artifact Inventory (Observed Post-Rebuild)

| Artifact | Size (B) | SHA-256 | Parity Check |
| :--- | ---: | :--- | :--- |
| `build_win64/zcc_sov_stage3.exe` | 3,228,672 | `6dee9c7c49c6f4914ff3b1fcaf9a83c4a6afde2a0846bb8e22dd2302dbf8c6fd` | `stage1==stage2==stage3`, `fc /b` 0 diffs |
| `build_wasm/zcc.wasm` | 1,156,756 | `db0f0dfeb6761b97653539f9dbd1037c716b7f3eb06a2e50295c94041b2b92c4` | `zcc==zcc2==zcc3`, `cmp` 0 diffs |
| `build_win64/gpu_llm_kernels.ptx` | 189,128 | `42a3abe70a4d63931d3383ac645cfb4b2c3f33806c1b661dac307079a1711b3b` | `./zcc == zcc_sov_stage3.exe`, `fc /b` 0 diffs (45 entries) |
| `build_win64/zcc_gpu_llm.exe` | 424,960 | `a64992895345b6322b1f3892eb2892acbe93b846c9c2c56f4edefe2fa4ecde3e` | `./zcc == zcc_sov_stage3.exe`, imports `{msvcrt,nvcuda}.dll` ONLY |
| `build_win64/zcc_tiny_llama_standalone.exe` | 449,536 | `ed27b25582168d74c329178b3993f78df14b486c481a4c88e3cf6ccb6b20b311` | `./zcc == zcc_sov_stage3.exe`, `fc /b` 0 diffs |
| `build_win64/zcc_hf_qwen2_real_scale_q4km.gguf` | 87,002,048 | `9ca6f34f64a2a9711c6377352ad8d81af0aef91784148e598412d6149e064a3f` | NumPy Q8_0/Q4_K/Q6_K encoders byte-identical to pure-Python |
| `build_win64/zcc_hf_qwen2_real_scale_q4km_sm120.ptx` | 82,044 | `fed0e2e003035fb8dde0714d32637b24c7adf70f977bddc3fc25c59e541796eb` | Triple-compiler (`./zcc == zcc_sov_stage3.exe == zcc.wasm`) |

---

## §3 Live RTX 5070 Hardware Measurements (87.00 MB Qwen2.5 `Q4_K_M`, 12L, `dim=1024`, `hidden=2816`, `heads=8/2`)

### 3.1 KV-Cache Quantization & Oracle Parity (`steps=24`, CUDA Graph `launches_per_token=1`)

| Mode (`--kv-type`) | KV Bytes | Compression Ratio vs F32 | Max Logit Diff vs NumPy Oracle | Greedy Tokens (24/24) | Top-1 ID / Logit | `tok/s` (`24` steps) |
| :--- | ---: | :--- | :--- | :--- | :--- | ---: |
| `f32` | 12,582,912 | `1.00x` | `0.000006` | `24 / 24` exact | `358` / `9.055698` | `1070.82` |
| `f16` | 6,291,456 | `2.00x` | `0.002479` | `24 / 24` exact | `358` / `9.055790` | `1051.61` |
| `q8_0` | 3,342,336 | `3.76x` | `0.075820` | `24 / 24` exact | `358` / `9.062012` | `1062.38` |

### 3.2 Batched SM 12.0 Tensor Core Prefill (`128` tokens, `--kv-type q8_0`, median of 3 runs)

- **`--prefill-batch 1`** (serial CUDA Graph baseline): **`182.4261 ms`** (`tc_prefill=0`, `chunk_count=128`)
- **`--prefill-batch 128`** (single SM 12.0 Tensor Core chunk): **`11.3935 ms`** (`tc_prefill=1`, `chunk_count=1`, **`11,234 tok/s`**)
- **Measured Prefill Speedup:** **`16.01x`** (`> 4.0x` target cleanly cleared)
- **Chunk & Engine Invariance (`128` tokens):** `--prefill-batch 64` vs `--prefill-batch 128` max logit diff `< 1e-4`; Tier B vs Generic `B=64` max logit diff `0.006769 < 1e-2`.

### 3.3 L2-Exceeding Sustained Decode (`48` steps, `--kv-type f16`, `wblob=86,982,656 B`)

- **CUDA Graph (`launches_per_token=1`, `block_dim=128`) vs `--no-cuda-graph`:** `0.0` prompt logit diff, `48 / 48` identical tokens.
- **Sustained Throughput:** `961.58 tok/s` (`total_gpu_ms=64.4774`), **`83.64 GB/s`** effective weight-read bandwidth beyond the 32 MB L2 cache.

### 3.4 Multi-Turn Interactive REPL (`--interactive --system "SoV" --chat`) & Negative Controls (`T10-NC-1..5`)

- **REPL Causality & `/reset`:** Turn 1 (`ZCC GPU` after Turn 0 `hello world`) diverged in `logits_fnv` and strictly exceeded post-`/reset` Turn 0 (`ZCC GPU` at clean `pos=13`), and `--ctx 32` cleanly emitted `GPU_LLM_CTX_FULL` without crashing before recovering on `/reset` and exiting `0` with `GPU_LLM_INTERACTIVE_OK`.
- **Negative Controls (`test_07`, all 5 FAIL-CLOSED):**
  - `T10-NC-1`: `--kv-type fp8|int4|q4_0` -> nonzero + `INVALID_ARG`
  - `T10-NC-2`: `--prefill-batch 0|129|abc`, trailing `--kv-type`/`--prefill-batch` -> nonzero + `INVALID_ARG`
  - `T10-NC-3`: `--ctx 524288 --kv-type f32` on `real_scale` -> nonzero + `VRAM_BUDGET_EXCEEDED`
  - `T10-NC-4`: Tier A `--aot-ptx zcc_tiny_llama_f16_sm120.ptx --kv-type f16` -> nonzero + `INVALID_ARG`
  - `T10-NC-5`: `--model zcc_tiny_llama_f32.gguf --kv-type q8_0` (`head_dim=16`) -> nonzero + `INVALID_ARG`

---

## §4 Modified & Added Files (Track 10 Scope)

- Compiler PTX codegen: `part6_ptx.c` (4-warp GEMV + `ld.v4` fast paths, Tier B `f16`/`q8_0` KV kernels, batched SM 12.0 Tensor Core prefill emitters)
- GPU Kernels & Inference Host: `src/win64_gpu_llm_kernels.c`, `src/win64_gpu_llm.c`
- Real-Scale Benchmark Fixture & Oracle: `tools/generate_gguf_track10_bench.py`
- Track 10 Test Suite & Build Targets: `tests/test_win64_gpu_track10.py`, `Makefile`
- Bootstrap Ledger: `BOOTSTRAP_BASELINES.tsv` (`4061875f1c1162071c8a9c9ab0543d8b`)
- Ticket: `tickets/WIN64-SOVEREIGN-GPU-TRACK10-GATE-EVIDENCE.md`
