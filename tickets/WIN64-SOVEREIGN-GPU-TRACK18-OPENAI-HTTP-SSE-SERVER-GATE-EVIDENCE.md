# WIN64-SOVEREIGN-GPU-TRACK18-OPENAI-HTTP-SSE-SERVER-GATE-EVIDENCE

**Classification:** Engineering Milestone Closure — Tier `E2`  
**Governing Rule Version:** `ZCC-EF-7 v1.0.0`  
**Evidence Tier Assigned At:** `2026-11-22T22:50:00-08:00` (`E2`, pre-execution Track 18 plan assignment)  
**Verified At:** `2026-11-23T07:11:38Z` (`2026-11-22T23:11:38-08:00`)  
**Git HEAD Revision:** `28f81f658baaf408670db2c6db5f92a4d270d94b`  
**Hardware / Driver Oracle:** `NVIDIA GeForce RTX 5070 Laptop GPU` (`cc=12.0`, `vram_mib=8150`, `driver=12090`)  
**Verdict:** **PASS** (Tier `E2`, independently verified across Win64 PE32+ `WS2_32.dll` import mapping, sovereign compiler parity, `zcc_gpu_llm_server.exe` & `zkaedi_prime_1_5b_server.exe` dual-compiler `FC /B` identity, headless Node.js EF-7 web UI verification, OpenAI `/v1/chat/completions` & `/v1/completions` REST + chunked SSE live RTX 5070 runs, empty-directory `986,565,728`-byte sealed overlay server execution + `TAMPER_DETECTED` negative control, HTTP `400`/`404`/`405`/`413` + mid-stream disconnect resilience checks, and Gate 1 / Gate 2 compiler invariants).

---

## 1. Acceptance Contract & Scope

1. **Target Capabilities (Track 18):**
   - **Native `WS2_32.dll` PE32+ Support (`src/win64_pe_emit.c`, `tools/build_gpu_win64.py`):**
     - Added Windows Sockets 2 API mapping (`WSAStartup`, `WSACleanup`, `WSAGetLastError`, `socket`, `bind`, `listen`, `accept`, `recv`, `send`, `closesocket`, `setsockopt`, `shutdown`, `getsockname`, `htons`, `htonl`, `ntohs`, `ntohl`, `inet_addr`, `select`, `ioctlsocket`) to `System32\WS2_32.dll` in ZCC's PE32+ emitter and added `ws2_32.dll` to `SYSTEM32_DLL_PATHS` in `tools/build_gpu_win64.py`.
   - **Sovereign Embedded Dark-Mode Chat UI (`docs/zcc_llm_server_ui.html`):**
     - Zero external CDN/network assets; XSS-safe DOM `.textContent` rendering, multi-turn `/v1/chat/completions` live SSE `ReadableStream` decoder, `/health` status badges (model ID, arch, AOT tier, KV type, SHA-256 overlay seal, live `tok/s`), and Rule EF-7 pointer-events/z-index interactive control isolation (`position: relative; z-index: 1000`).
   - **Pure C99 Winsock2 OpenAI HTTP/SSE GPU Server (`src/win64_gpu_llm_server.c`, `tools/build_gpu_llm_server_win64.py`, `tools/compile_gguf_exe.py --server`):**
     - Built `build_win64/zcc_gpu_llm_server.exe` (`574,976` bytes, IAT strictly `{'WS2_32.dll', 'msvcrt.dll', 'nvcuda.dll'}`) serving `GET /`, `GET /health`, `GET /v1/models`, `POST /v1/chat/completions`, `POST /v1/completions` (both JSON and `Transfer-Encoding: chunked` SSE), `OPTIONS` CORS preflight, and opt-in `POST /shutdown` (`--allow-shutdown`).
     - Added `--server` mode to `tools/compile_gguf_exe.py` producing `build_win64/zkaedi_prime_1_5b_server.exe` (`986,565,728` bytes single binary combining Tier-B AOT PTX, Winsock2 OpenAI server, embedded HTML UI, and FIPS 180-4 SHA-256 sealed `zkaedi-prime-dpo-q4_k_m.gguf` payload verified before opening any TCP socket).
2. **Compiler & Regression Scope Constraints:**
   - Zero edits to `part0_pp.c..part6_ptx.c` or `BOOTSTRAP_BASELINES.tsv`.
   - Gate 1 (`cmp zcc2.s zcc3.s`, bootstrap MD5 `8b28903bb66619f2c38397ae2a0ddfed`) and Gate 2 (`scripts/gate2_interop.sh`) remain unbroken.
3. **Exclusions:**
   - TLS termination (localhost plaintext HTTP/1.1 by design; place behind reverse proxy for public exposure) and concurrent parallel request interleaving across multiple clients simultaneously on one GPU queue (sequential accept loop).

---

## 2. Deliverable Artifacts & SHA-256 Ledger

| Artifact | Bytes | SHA-256 | Evidence Class |
|---|---:|---|---|
| `build_win64/zcc_gpu_llm_server.exe` | 574,976 | `3576f400038b09d3ef3ed27287521bff1be2d536287c95c05b5fbe830590480e` | OBSERVED (`FC /B` identical `./zcc` vs `zcc_sov_stage3.exe`, IAT `['WS2_32.dll', 'msvcrt.dll', 'nvcuda.dll']`) |
| `build_win64/zkaedi_prime_1_5b_server.exe` | 986,565,728 | `34fab51932cb1dda76e8d1d33e468119639d5aa555e1dd2772b54263a0ef4d2f` | OBSERVED (`pe_base=517,632B`, `SHA256=9f7a5a1b...`; sealed `986,048,032B` GGUF overlay + `64B` `ZCCGGUF1` footer) |
| `build_win64/gguf_exe_zkaedi_prime_1_5b_server/zkaedi_prime_1_5b_server_sm_120.ptx` | 137,346 | `49a8c186a359fc243288df59e21f5f81c1f71198be854581deb15af408427677` | OBSERVED (`FC /B` byte-identical `./zcc` vs `zcc_sov_stage3.exe`) |
| `docs/zcc_llm_server_ui.html` | 17,236 | `4cf9698f3841335d344050d1a89c1b28a969df1d25e4c5914bb9d25e0a7821c5` | OBSERVED (`ui_ok=true`, `errors=0`, `scripts=1`) |

---

## 3. Sovereign PE32+ Builds & Audits (Raw Output Excerpts)

### 3.1 `python3 tools/build_gpu_llm_server_win64.py`
```text
[UI-EF7:SERVER] {"ui_ok":true,"errors":0,"html_bytes":17236,"scripts":1}
[UI-HEADER:SERVER] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/server_ui_html.h (17236 bytes -> g_server_ui_html)
[FC /B:SERVER] 574976B SHA256=3576f400038b09d3ef3ed27287521bff1be2d536287c95c05b5fbe830590480e Comparing files BUILD_WIN64\zcc_gpu_llm_server_host_zcc.exe and BUILD_WIN64\ZCC_GPU_LLM_SERVER.EXE
FC: no differences encountered
[PE-AUDIT:SERVER] bytes=574976 imports=['WS2_32.dll', 'msvcrt.dll', 'nvcuda.dll']
```

### 3.2 `python3 tools/compile_gguf_exe.py --model /mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf -o build_win64/zkaedi_prime_1_5b_server.exe --embed-overlay --server --timeout 240`
```text
[GGUF-EXE:FC-PTX] 137346B SHA256=49a8c186a359fc243288df59e21f5f81c1f71198be854581deb15af408427677 Comparing files BUILD_WIN64\GGUF_EXE_ZKAEDI_PRIME_1_5B_SERVER\zkaedi_prime_1_5b_server_sm_120_host.ptx and BUILD_WIN64\GGUF_EXE_ZKAEDI_PRIME_1_5B_SERVER\ZKAEDI_PRIME_1_5B_SERVER_SM_120.PTX
FC: no differences encountered
[GGUF-EXE:FC-EXE] 517632B SHA256=9f7a5a1b71d3a3cdacd5fdb6ded94e50ad3098822f19f0723a2bb737d5e66728 Comparing files BUILD_WIN64\zkaedi_prime_1_5b_server_host_zcc.exe and BUILD_WIN64\ZKAEDI_PRIME_1_5B_SERVER.EXE
FC: no differences encountered
[GGUF-EXE:PE-AUDIT] build_win64/zkaedi_prime_1_5b_server.exe imports=['WS2_32.dll', 'msvcrt.dll', 'nvcuda.dll'] bytes=517632
[GGUF-EXE:OVERLAY-SEAL] pe_base=517632B + pad=0B + gguf=986048032B + footer=64B -> total=986565728B off=517632 sha256=f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8
```

---

## 4. Live RTX 5070 OpenAI Server Evidence (Empty-Dir Sealed Overlay)

Executed from a fresh empty `%TEMP%` directory (`--port 0 --kv-type f16 --ctx 512 --allow-shutdown`):
```text
READY_OUTPUT:
GPU_LLM_SEAL verified=1 mode=pe_overlay sha256=f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8 bytes=986048032
ZCC_SERVER_READY url=http://127.0.0.1:52804 model=zkaedi_prime_1_5b_server.exe mode=aot_overlay seal=1 aot_tier=2 kv=f16 ctx=512
CHAT_STATUS=200 CHAT_BODY={"id":"chatcmpl-zcc-1","object":"chat.completion","created":1795417898,"model":"zkaedi_prime_1_5b_server.exe","choices":[{"index":0,"message":{"role":"assistant","content":"The capital of France is Paris."},"finish_reason":"stop"}],"usage":{"prompt_tokens":15,"completion_tokens":7,"total_tokens":22}}
HEALTH_AFTER_CHAT={"status":"ok","model":"zkaedi_prime_1_5b_server.exe","arch":"qwen2","n_layers":28,"dim":1536,"vocab_size":151936,"aot_tier":2,"kv_type":"f16","eff_max_seq":512,"seal_verified":true,"seal_sha256":"f7cc9b0b2acbe1902e4e72fdaddef4f3dfd58d2acd875bcaff32798431a465a8","requests_served":1,"last_tok_per_sec":226.47,"last_gen_tokens":7}
```

---

## 5. Verification Matrix (Sequential Suites & Compiler Gates)

| Gate / Suite | Command | Outcome | Evidence Class |
|---|---|---|---|
| Track 18 OpenAI HTTP/SSE Server Suite | `python3 tests/test_win64_gpu_track18_openai_server.py -v` | `Ran 6 tests in 103.024s — OK` (`test_01..test_06`, incl. empty-dir `1.5B` SSE, `1.5B` tamper `NC`, HTTP `400/404/405/413`, mid-stream TCP disconnect) | OBSERVED |
| Track 17 Overlay & DLL Suite | `python3 tests/test_win64_gpu_track17_overlay_dll.py -v` | `Ran 6 tests in 119.434s — OK` | OBSERVED |
| Win64 PE Emitter Suite | `python3 tests/test_win64_pe_emitter.py -v` | `Ran 39 tests in 4.899s — OK (skipped=5)` (WSL skips host `.exe` subcases; Win64 live execution covered across Tracks 17/18 + `win64_selfhost_sovereign.py`) | OBSERVED |
| Track 7 Generic GPU LLM Suite | `python3 tests/test_win64_gpu_llm.py -v` | `Ran 8 tests in 7.709s — OK` | OBSERVED |
| Gate 1 Self-Host & Bootstrap Ledger | `cmp zcc2.s zcc3.s && bash scripts/log_bootstrap_hash.sh --check` | `CMP_ZCC2_ZCC3_IDENTICAL`, `BOOTSTRAP HASH: 8b28903bb66619f2c38397ae2a0ddfed`, `BOOTSTRAP_HASH_EXIT=0` | OBSERVED |
| Sovereign Win64 3-Stage Self-Host | `python3 tools/win64_selfhost_sovereign.py` | `stage2.s == stage3.s` (`987ef5ed...`), `stage2.bin == stage3.bin` (`c3e347e8...`), exit `0` | OBSERVED |
| Gate 2 SystemV Inter-op | `bash scripts/gate2_interop.sh` | Both directions -> `INTEROP_SUCCESS: x=10 y=17 z=300` | OBSERVED |

---

## 6. ZCC-EF-7 §9 Formal Adjudication

- **Claim ->** ZCC compiles a pure C99 Winsock2 + Blackwell SM 12.0 CUDA OpenAI-compatible HTTP/1.1 & chunked-SSE inference server (`zcc_gpu_llm_server.exe`, `574,976` bytes) AND a single-binary `986,565,728`-byte SHA-256 self-verifying PE overlay server (`zkaedi_prime_1_5b_server.exe`) importing strictly `{'WS2_32.dll', 'msvcrt.dll', 'nvcuda.dll'}` with zero MSVC, Python, MinGW, or CUDA Toolkit dependencies, passing all REST/SSE, multi-turn, web UI EF-7, tamper fail-closed, HTTP negative control, and mid-stream disconnect tests.
- **Evidence Class ->** **OBSERVED**
- **Evidence Tier ->** **E2** (Engineering Milestone Closure)
- **Evidence ->** `FC /B` byte-identical dual builds, System32 PE32+ IAT audits, headless Node.js EF-7 web UI gate (`ui_ok=true`, `errors=0`), live RTX 5070 REST/SSE responses (`"The capital of France is Paris."`, `"Tokyo"`, `"es C99 directly"`, `226.47 tok/s`), `6/6` OK in `test_win64_gpu_track18_openai_server.py`, `6/6` OK in `test_win64_gpu_track17_overlay_dll.py`, `39` PE emitter tests (`OK, skipped=5`), `8/8` GPU LLM tests (`OK`), and clean Gate 1 / Win64 self-host / Gate 2 passes.
- **Scope ->** Windows x64 (`-target win64 -mabi=ms`) loopback/IPv4 HTTP/1.1 + SSE inference servers on NVIDIA Blackwell (`sm_120`, RTX 5070 Laptop GPU).
- **Limitations ->** Plaintext HTTP/1.1 over TCP (use TLS reverse proxy for untrusted networks) and sequential single-client inference loop.
- **Verdict ->** **PASS**
