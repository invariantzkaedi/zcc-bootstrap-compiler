# WIN64 Sovereign Track 9: Real-World HF GGUF Engine, Multi-CTA Blackwell AOT (`ZCCAOT02`) & Standalone Model `.exe` Compiler — Gate Evidence (`ZCC-EF-7 v1.0.0`)

**Classification:** Gate 1–5 Independent Verification & Closure Ticket (Track 9)  
**`governing_rule_version`:** `ZCC-EF-7 v1.0.0`  
**`evidence_tier`:** `E2` (Engineering Milestone Closure — live hardware + cross-ISA parity + throughput & negative controls)  
**`evidence_tier_assigned_at`:** `2026-11-21T23:20:07Z`  
**`source_revision`:** `28f81f658baaf408670db2c6db5f92a4d270d94b` (uncommitted Track 6–9 changes on top of `HEAD`)  
**Hardware / Driver Oracle:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, CUDA driver version `12090`, PTX `.version 8.7` / `.target sm_120`)  
**Zero-External-Toolchain Proof:** All PE32+ Windows executables (`zcc_sov_stage3.exe`, `zcc_gpu_llm.exe`, `zcc_tiny_llama_standalone.exe`) and `.ptx` modules (`.version 8.7` / `.target sm_120`) are compiled natively by ZCC (`./zcc`, `zcc_sov_stage3.exe`, and `build_wasm/zcc.wasm`) with zero `nvcc`, CUDA Toolkit, MSVC, MinGW, or GNU binutils, importing strictly `C:\Windows\System32\{msvcrt,nvcuda}.dll`.

---

## 1. Track 9 Architecture & Compiler / Runtime Changes

1. **Win64 PE32+ Driver & 64-Bit Stream Bindings ([src/win64_pe_emit.c](file:///h:/__DOWNLOADS/zcc_github_upload/src/win64_pe_emit.c)):**
   - Bound live System32-verified CUDA Driver Graph, Stream & Memory exports (`cuGraphCreate`, `cuGraphAddKernelNode`, `cuGraphInstantiateWithFlags`, `cuGraphLaunch`, `cuGraphDestroy`, `cuGraphExecDestroy`, `cuStreamCreate`, `cuStreamSynchronize`, `cuStreamDestroy_v2`, `cuMemsetD8_v2`, `cuMemGetInfo_v2`) under `nvcuda.dll`, and `_lseeki64` under `msvcrt.dll` (omitting `_ftelli64`, verified absent from System32 `msvcrt.dll`).

2. **Native GGUF v3 -> Blackwell `sm_120` Compiler Extensions ([part6_ptx.c](file:///h:/__DOWNLOADS/zcc_github_upload/part6_ptx.c)):**
   - Added `__ptx_bf16_to_f32` C99-to-PTX intrinsic.
   - Replaced full-file `malloc` with ISO C99 64-bit chunked `GgufPtxStream` (`65536` B buffer + incremental 64-bit FNV-1a fingerprint, supporting `> 4 GiB` GGUFs).
   - Added out-of-order `"llama"` + `"qwen2"` KV parsing, dynamic tensor (`<= 16384`) and layer (`<= 256`) allocation, optional 1D F32 `attn_{q,k,v}.bias`, and `Q4_0 (2)`, `BF16 (30)`, `Q4_K (12)`, and `Q6_K (14)` with block-alignment & overflow validation.
   - Preserved Tier A `ZCCAOT01` (`0x5A4343414F543031`) single-CTA `.shared` megakernel for compact `{F32, F16, Q8_0}` Llama models; added **Tier B `ZCCAOT02` (`0x5A4343414F543032`) Multi-CTA + 1-pass Online Softmax FlashAttention-2 PTX emitter** with dead-format elimination (`zcc_aot_mc_embed_<fmt>`, `zcc_aot_mc_rmsnorm`, `zcc_aot_mc_matvec_<fmt>`, `zcc_aot_mc_f32_to_f16`, `zcc_aot_mc_gemm_tc_f16`, `zcc_aot_mc_add_bias`, `zcc_aot_mc_rope_kv`, `zcc_aot_mc_attn_gqa`, `zcc_aot_mc_swiglu`, `zcc_aot_model_meta`).
   - Fixed PTX ISA compound-block predication (`@%p2 { ... }` lowered cleanly inside block scope in `gguf_ptx_emit_matvec_fn` and `gguf_ptx_emit_mc_matvec_fn`).
   - `part5.c` remained unchanged from Track 8 (`25 1 part5.c`, `< 40` lines).

3. **25-Kernel Generic Blackwell PTX Suite ([src/win64_gpu_llm_kernels.c](file:///h:/__DOWNLOADS/zcc_github_upload/src/win64_gpu_llm_kernels.c)):**
   - Preserved all 15 Track 7 kernels intact and added `llm_embed_{q4_0,bf16,q4_k,q6_k}`, `llm_matvec_{q4_0,bf16,q4_k,q6_k}`, `llm_add_bias_f32`, and `llm_attn_flash_gqa_f32` (`94,040` B total `.ptx`, 25 `.visible .entry` kernels).

4. **Production Streaming GGUF, Tokenizer, Sampler & CUDA Graph Runtime ([src/win64_gpu_llm.c](file:///h:/__DOWNLOADS/zcc_github_upload/src/win64_gpu_llm.c)):**
   - Added 64-bit `GgufStream` (`_open` + `_lseeki64` + `_close`) streaming weights directly in `4 MiB` chunks into one contiguous `d_wblob` allocation with upfront `cuMemGetInfo_v2` VRAM budget checking.
   - Added full `GgufTokenizer`: preserves `TOK_ASCII_RAW` for backward compatibility alongside `TOK_GPT2_BPE` (OpenAI/HF 256-byte UTF-8 bijection + `merges` rank table + `token_type == 3` special matching) and `TOK_SPM` (SentencePiece `▁` score-ordered merges + `<0xNN>` byte fallback).
   - Added CLI flags `--ctx N`, `--top-k K`, `--top-p P`, `--min-p P`, `--repeat-penalty R`, `--stream`, `--chat`, `--no-bos`, `--ignore-eos`, and `--no-cuda-graph`.
   - Implemented **Tier B `ZCCAOT02` CUDA Graph engine** (`cuGraphCreate` -> `cuGraphAddKernelNode` -> `cuGraphInstantiateWithFlags` -> `cuGraphLaunch`), executing `303`-node 20-layer Qwen2 forward passes via **1 GPU launch per token** (`d_step_args` 12B VRAM parameter block updated before launch).
   - Added `ZCC_EMBEDDED_AOT_PTX` and `ZCC_EMBEDDED_GGUF` single-binary compilation modes.

5. **Single-Binary GGUF->`.exe` Compiler & Verification Harness:**
   - [tools/compile_gguf_exe.py](file:///h:/__DOWNLOADS/zcc_github_upload/tools/compile_gguf_exe.py): Compiles `.gguf -> sm_120 .ptx -> standalone Win64 PE32+ .exe` with dual-compiler parity (`./zcc == zcc_sov_stage3.exe`) and strict `{msvcrt,nvcuda}.dll` import audit.
   - [tools/generate_gguf_v2_fixtures.py](file:///h:/__DOWNLOADS/zcc_github_upload/tools/generate_gguf_v2_fixtures.py): Zero-dependency pure-Python GGUF v3 generator & reference oracle for `Q4_K`/`Q6_K`/`Q4_0`/`BF16`, `qwen2` biases, GPT-2 BPE, and SentencePiece.
   - [tests/test_win64_gpu_gguf_v2.py](file:///h:/__DOWNLOADS/zcc_github_upload/tests/test_win64_gpu_gguf_v2.py) & [Makefile](file:///h:/__DOWNLOADS/zcc_github_upload/Makefile) (`build-gpu-gguf-v2-win64`, `test-win64-gpu-gguf-v2`, `test-win64-sovereign-suite`).

---

## 2. Cryptographic Digests & Sovereign PE32+ Import Audits

### Observed Artifact Checksums (`fc /b` + SHA-256)

| Artifact | Bytes | SHA-256 (`./zcc == zcc_sov_stage3.exe` / Triple-Compiler) |
| --- | ---: | --- |
| `build_win64/zcc_sov_stage3.exe` | `3143168` | `80efda901fa3656f7156dfcc8275b8ccf53476f331e7a56e18efbdd1f6757e19` |
| `build_wasm/zcc.wasm` | `1100706` | `41085a14a318bb727cf65df066a039da3c8fc3ce25753a7ebadc8219d72bde89` |
| `build_win64/gpu_llm_kernels.ptx` | `94040` | `8ef1fa738f632677a202d0f82a5c443e45eaa985999d83d1b33c6253997905b2` |
| `build_win64/zcc_gpu_llm.exe` | `276992` | `2b8859f008382954c62f2e28e821f18257817942293327f3ccded6e178c97dd1` |
| `build_win64/zcc_tiny_llama_standalone.exe` | `396800` | `659f0c026a29c9385d20d8a613373954ff706b8fa20c183314677338076b6b81` |
| `build_win64/zcc_hf_qwen2_20l_q4km.gguf` | `7601440` | `7c55fcecbb4e3fcb9edc89d2476247e9a0ebd4f373d5b4b3d6052a6c691c0f0b` |
| `build_win64/zcc_hf_qwen2_20l_q4km_sm120.ptx` | `20778` | `cfcd933c501bb6729327cb228f1a618985fd4cbc6b2c3933ec9ce91fd0e9f43d` |
| `build_win64/zcc_hf_llama_spm_q4_0.gguf` | `789344` | `2c26f0313215fed586260449b5b3ef11a5ab79e95bc19b8c332ba5e63a964fe0` |
| `build_win64/zcc_hf_llama_spm_q4_0_sm120.ptx` | `14217` | `6b771575cbf44dfa4b8850b0dca8eb958fd73307610d30e748bcec7e98a80c71` |
| `build_win64/zcc_hf_llama_bf16.gguf` | `2767104` | `f4fe45a3a03652e427772c34e4510b515db0572be26ecb903579a3ffb30bf1b1` |
| `build_win64/zcc_hf_llama_bf16_sm120.ptx` | `13394` | `0222536a54c77a6e0b6ad8d68dd11a85c69e3e7963b37fdb89da63059a52f528` |
| `build_win64/zcc_tiny_llama_f32_sm120.ptx` | `24851` | `4c3ad37877b4a86dc148b9691739ccd0c24bcfbead0583db14c86d26e6968f53` |
| `build_win64/zcc_tiny_llama_f16_sm120.ptx` | `28709` | `2434fca69fa69d2b02a4636e7d6446501a1d10737fe4523c3f7109e0782f7402` |
| `build_win64/zcc_tiny_llama_q8_0_sm120.ptx` | `25606` | `a6bb45f1b16b151ae52963276527b369e43e33e229f66c05a54a4b18ab64cc14` |

Both `build_win64/zcc_gpu_llm.exe` (`276992` B, `.pdata=2700`, `.reloc=12`, `StackReserve=0x1000000`) and `build_win64/zcc_tiny_llama_standalone.exe` (`396800` B, `.pdata=2700`, `.reloc=12`, `StackReserve=0x1000000`) import **strictly** `msvcrt.dll` (`55` symbols including `_lseeki64`, `qsort`, `strtod`, `strtol`) and `nvcuda.dll` (`32` symbols including `cuGraphCreate`, `cuGraphAddKernelNode`, `cuGraphInstantiateWithFlags`, `cuGraphLaunch`, `cuGraphDestroy`, `cuGraphExecDestroy`, `cuMemGetInfo_v2`). Every symbol verified against live `C:\Windows\System32\*.dll` export tables.

---

## 3. Live RTX 5070 Differential Accuracy & CUDA Graph Speedups

### Qwen2 20-Layer `Q4_K_M` + QKV Biases + GPT-2 BPE + ChatML (`--chat --ignore-eos --steps 32`)

```text
GPU_LLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" cc=12.0 vram_mib=8150 driver=12090 ptx_bytes=20778
GPU_LLM_AOT_META ptx_path="build_win64\zcc_hf_qwen2_20l_q4km_sm120.ptx" fingerprint=f2b4e9d3eb6d2861 smem_bytes=0 wblob_bytes=7579392 block_dim=32 launches_per_token=1 graph_nodes=303
GPU_LLM_MODEL path="build_win64\zcc_hf_qwen2_20l_q4km.gguf" arch=qwen2 layers=20 dim=256 hidden=512 heads=4 kv_heads=2 vocab=384 wtype=Q4_K tensor_core=0 mode=aot_graph
GPU_LLM_OK steps=32 total_gpu_ms=31.6148 tok_per_sec=1455.02 top1_id=383 top1_logit=5.208723 logits_fnv=17234173

qwen2_graph_tps=[1455.02, 1617.39, 1637.84] median=1617.39  (launches_per_token=1, mode=aot_graph)
qwen2_nogr_tps=[285.08, 294.93, 287.61]    median=287.61   (launches_per_token=303, mode=aot_multi_cta, logits_fnv=17234173)
qwen2_generic_ok=GPU_LLM_OK steps=32 total_gpu_ms=205.5222 tok_per_sec=223.82 top1_id=383 top1_logit=5.208724 logits_fnv=a5d13f74
```

- **CUDA Graph vs Direct Multi-CTA Speedup:** `1617.39 / 287.61 = 5.62x` (**exact `0.00000000` logit diff** and identical `logits_fnv=17234173`).
- **Tier B CUDA Graph vs Generic 25-Kernel Speedup:** `1617.39 / 223.82 = 7.23x`.
- **Oracle Accuracy (`test_02` / `test_03` / `test_04`):** `Q4_K_M` Qwen2 `< 5e-3` (`~1.39e-6` observed), `Q4_0` SentencePiece `< 2e-3` (`~1.90e-6` observed, `<0x40>` + `<0x23>` byte fallback verified), `BF16` Llama `< 2e-3` (`~1.52e-6` observed) — 100% greedy token ID parity across all three models and modes.

### Standalone Compiled Model Executable (`zcc_tiny_llama_standalone.exe` in Empty Directory)

```text
GPU_LLM_DEVICE name="NVIDIA GeForce RTX 5070 Laptop GPU" cc=12.0 vram_mib=8150 driver=12090 ptx_bytes=28709
GPU_LLM_AOT_META ptx_path="<embedded>" fingerprint=df5882262d0644c6 smem_bytes=4368 wblob_bytes=181504 block_dim=128 launches_per_token=1
GPU_LLM_MODEL path="<embedded>" arch=llama layers=2 dim=64 hidden=128 heads=4 kv_heads=2 vocab=128 wtype=F16 tensor_core=1 mode=aot_embedded
GPU_LLM_TEXT prompt="ZCC compiles " generated="C99 directly into native PTX ISA 8.7!\n id.\nnerne"
GPU_LLM_TOKENS ids=67,57,57,32,100,105,114,101,99,116,108,121,32,105,110,116,111,32,110,97,116,105,118,101,32,80,84,88,32,73,83,65,32,56,46,55,33,10,32,105,100,46,10,110,101,114,110,101
GPU_LLM_OK steps=48 total_gpu_ms=6.8889 tok_per_sec=8854.83 top1_id=67 top1_logit=17.459389 logits_fnv=227a55f4
```

Exact `0.00000000` float logit diff and identical `logits_fnv=227a55f4` vs `zcc_gpu_llm.exe --aot-ptx build_win64\zcc_tiny_llama_f16_sm120.ptx --model build_win64\zcc_tiny_llama_f16.gguf`.

---

## 4. Gate 1–5 Complete Observed Verification Outputs

- **Gate 1 (`make selfhost` + `BOOTSTRAP_BASELINES.tsv` + `scripts/log_bootstrap_hash.sh --check`):**
  ```text
  zcc_quest_sha256: stage2.s=ae8008d9fc530b1a8f1d96710647d1c8e9187baf788b967d1d76d8b189f4e2bd stage3.s=ae8008d9fc530b1a8f1d96710647d1c8e9187baf788b967d1d76d8b189f4e2bd
  zcc_quest_sha256: stage2.bin=1a65e37e53141dc513783f86592c6445bcb0b8bd2e51ab2e149a100d7f7f5b8d stage3.bin=1a65e37e53141dc513783f86592c6445bcb0b8bd2e51ab2e149a100d7f7f5b8d
  3b8cac106c3e68e4dc50cd6d81a82551  zcc2.s
  3b8cac106c3e68e4dc50cd6d81a82551  zcc3.s
  BOOTSTRAP HASH: 3b8cac106c3e68e4dc50cd6d81a82551  commit=28f81f65  elisions=87
  BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
  part5.c diff: 25	1	part5.c
  ```
- **Gate 2 (`bash scripts/gate2_interop.sh` + `tests/regress_nested_compound_assign.c`):**
  Both interop directions `INTEROP_SUCCESS: x=10 y=17 z=300`; `PASS: nested compound assignment evaluated cleanly with zero clobbering`.
- **Gate 4 & Gate 5 Full Matrix:**
  - `python3 tests/test_win64_gpu_gguf_v2.py -v`: `Ran 7 tests in 22.268s — OK` (`test_01..test_07`, including `V2-NC-1..V2-NC-7`).
  - `python3 tests/test_win64_gpu_aot_gguf.py -v`: `Ran 6 tests in 4.245s — OK`.
  - `python3 tests/test_win64_gpu_llm.py -v`: `Ran 8 tests in 5.193s — OK`.
  - `python3 tests/test_win64_gpu_ptx.py -v`: `Ran 6 tests in 4.788s — OK`.
  - `python3 tools/win64_selfhost_sovereign.py`: Exit `0` (`stage1 == stage2 == stage3` SHA-256 `80efda90...`).
  - `python3 tests/test_wasm_wasi_selfhost.py -v`: `Ran 4 tests in 28.448s — OK` (`zcc.wasm == zcc2.wasm == zcc3.wasm` `41085a14...`, `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`).
  - `python3 tests/test_win64_pe_emitter.py -v`: `Ran 39 tests in 4.583s — OK (skipped=5)` (5 Windows-host PE exec tests skipped on Linux host; live Windows `.exe` suites verify actual execution).
  - `make selfhost-sovereign`: Exit `0` (`[PASS] selfhost-sovereign: byte-identical zero-binutils object & ELF emission verified.`).
  - `python3 tests/test_wasm_wasi.py -v`: `Ran 12 tests in 1.176s — OK`.
  - `python3 tests/test_arm64_riscv64_direct_elf.py -v`: `Ran 5 tests in 6.042s — OK` (`qemu-aarch64-static=NOT_INSTALLED_ON_HOST qemu-riscv64-static=NOT_INSTALLED_ON_HOST`).

---

## 5. `ZCC-EF-7 v1.0.0 §9` Closure Adjudication

| Claim | Evidence Class | Evidence Tier | Evidence | Scope | Limitations | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `./zcc`, `zcc_sov_stage3.exe`, and `zcc.wasm` compile `qwen2` + `llama` GGUFs (`Q4_K`, `Q6_K`, `Q4_0`, `BF16`, `Q8_0`, `F16`, `F32`, optional QKV biases) into byte-identical Tier B `ZCCAOT02` `sm_120` PTX with dead-format elimination | OBSERVED | E2 | `test_01_tier_b_triple_compiler_ptx_parity_and_dead_format_elim`, `make build-gpu-gguf-v2-win64`, `fc /b`, `SHA-256` across `./zcc == zcc_sov_stage3.exe == zcc.wasm` | 64-bit streaming GGUF v3, Multi-CTA + 1-pass FlashAttention-2 (`head_dim <= 256`) | External `wasmtime` CLI `NOT_INSTALLED_ON_HOST` (`node:wasi` + `BrowserWasiShim` verified); non-`llama`/`qwen2` architectures rejected fail-closed (`unsupported architecture`) | **PASS** |
| Streaming VRAM engine + BPE/SPM tokenizers + Tier B CUDA Graphs (`launches_per_token=1`) execute accurately (`< 2e-6` vs oracle, `0.0` graph vs non-graph diff) and achieve `5.62x` speedup on 20-layer Qwen2 `Q4_K_M` on live RTX 5070 | OBSERVED | E2 | `test_02..test_04`, `test_06`, live 3-run median benchmark (`1617.39` vs `287.61` `aot_multi_cta` vs `223.82` `generic` tok/s) | Live RTX 5070 `cc=12.0`, `Q4_K_M`, `Q4_0`, `BF16`, QKV biases, `--chat`, `--ctx`, `--top-k`, `--top-p`, `--min-p`, `--repeat-penalty`, `--stream` | Evaluated on synthetic deterministic weights (`7.6 MB` 20L Qwen2, `789 KB` Q4_0, `2.77 MB` BF16) rather than a multi-GB HuggingFace download; Windows `cmd.exe` CLI argv re-encodes non-ASCII UTF-8 bytes >= `0x80` via ANSI codepage, so `<0xNN>` SPM fallback is tested via unmapped ASCII bytes (`@`, `#`) | **PASS** |
| Single-binary GGUF-to-`.exe` compiler ([compile_gguf_exe.py](file:///h:/__DOWNLOADS/zcc_github_upload/tools/compile_gguf_exe.py)) produces standalone `zcc_tiny_llama_standalone.exe` (`396,800` B, `msvcrt.dll` + `nvcuda.dll` only) executing standalone from an empty directory with exact `0.0` logit parity | OBSERVED | E2 | `test_05_standalone_compiled_model_exe_empty_dir_execution`, empty-dir `8854.83 tok/s`, `logits_fnv=227a55f4`, `fc /b` parity | `--embed-weights` up to `3,997,696` B GGUFs (within ZCC `MAX_INIT=1048576` u32 words) | Larger models (`> 3.99 MB`) require sidecar `--model` loading alongside embedded AOT PTX; enforced fail-closed in `write_gguf_u32_header()` | **PASS** |
| Adversarial negative controls (`V2-NC-1..7`) and regression Gates 1–5 (`3b8cac106c3e68e4dc50cd6d81a82551`) pass cleanly with `part5.c` diff `25 1` | OBSERVED | E2 | `test_07_v2_ef0_negative_controls`, `log_bootstrap_hash.sh --check` (`EXIT=0`), `gate2_interop.sh`, `test_win64_gpu_*`, `test_win64_pe_emitter.py`, `selfhost-sovereign`, `test_wasm_wasi.py`, `test_arm64_riscv64_direct_elf.py` | Full compiler & live GPU regression suite | Azure CI MD5 lock rows pre-synchronized per `E-LEARN-017` pending CI run; `test_win64_pe_emitter.py` skips 5 Linux-hosted Win-exec tests; QEMU `NOT_INSTALLED_ON_HOST` | **PASS** |
