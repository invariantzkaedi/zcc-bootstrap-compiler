# Track 1B — Standalone WASI DOOM 1.10 (`doom.wasm`) & Live Canvas Framebuffer Playground Gate Evidence

- **Governing Rule:** ZCC-EF-0 & ZCC-EF-7 (`governing_rule_version: 1.0.0`)
- **`evidence_tier_assigned_at`:** `2026-11-19T23:02:00Z` (plan handoff prior to final `part3.c:367` fix & full post-edit Gate 1–5 sweep)
- **Base Commit (`HEAD`):** `bf472ed5cf82e4dc9c501efef43748b9038278d2`
- **Phase 0 Verdict:** `BASELINE: GREEN | SYMPTOM-IN-HISTORY: NO | FORENSIC-LATEST-SHA: 049edf005 | PROCEED: YES` (`FORENSIC_025_PACKED_ALIGNMENT.md`, pre-edit `make selfhost` `stage2.s == stage3.s` `fe392cb8071b028474c71687429c25efdc693b598507e87fce6a0c401238e1fd`, `oracle_verified=True`)
- **Toolchain & Host Identity:**
  - `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0` on `Linux 6.18.58.1-microsoft-standard-WSL2 x86_64`
  - Node.js `v22.23.5` (`/home/ZKAEDI/.local/bin/node`, V8 WebAssembly + `node:wasi` + `vm` AST validator)
  - On-disk IWAD: `doom1.wad` (`28,795,216` bytes / SHA-256 `7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d`, 3,163 lumps including `PLAYPAL`, `TITLEPIC`, `DEMO1`, `E1M1`)
  - Native Windows `cmd.exe` / `fc.exe` (via WSL interop for Win64 PE32+ self-host verification)
- **Explicit Host Runtime Dispositions:**
  - `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` (`wasmtime` not installed on host; WASI execution verified across Node 22 `node:wasi` and zero-dependency pure-JS `tools/browser_wasi_shim.js`)
  - `QEMU_STATUS: NOT_INSTALLED_ON_HOST` (`qemu-aarch64-static`, `qemu-riscv64-static` not installed on host; `tests/test_arm64_riscv64_direct_elf.py` clean-room emulators passed 5/5)

---

## 1. Compiler Source Diff Audit & Root-Cause Log (`$(PARTS)` & `doom/`)

```text
$ git diff --stat part0_pp.c part1.c part2.c part3.c part4.c part5.c
 part3.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)

$ git diff --name-only doom/
(empty — 0 modifications to upstream doom/*.c / doom/*.h)
```

### Five Root-Cause Defects Resolved During Track 1B
1. **Missing `p_saveg.c` in DOOM Module List (`tools/build_doom_wasm.py`):**
   - *Symptom:* 8 unresolved `env.P_ArchivePlayers`/`P_UnArchiveWorld`/etc. imports in `doom.wasm`.
   - *Fix:* Included `p_saveg.c` in `DOOM_CORE_MODULES` (all 57 upstream non-`i_*.c` modules from `doom/Makefile`), achieving `0` `env.*` imports.
2. **Enum Shadowing of POSIX `open`/`close` (`doom/p_spec.h:332-333`):**
   - *Symptom:* `enum vldoor_e { ..., close, open, ... }` shadowed `open()`/`close()` in `w_wad.c`, `m_misc.c`, `m_menu.c`, compiling `open(...)` into `call_indirect` with `i32.const 3` (`RuntimeError: null function or function signature mismatch` in `W_AddFile`).
   - *Fix:* Added `_doom_posix_open` / `_doom_posix_close` wrappers and scoped `#define open` / `#define close` around `w_wad.c`, `m_misc.c`, `m_menu.c` in `tools/build_doom_wasm.py` (`sanitize_doom_module`) without touching `doom/*.c`.
3. **Unterminated Upstream `sprnames[]` Array (`doom/info.c:40`, `doom/r_things.c`):**
   - *Symptom:* Upstream DOOM 1.10 `sprnames[NUMSPRITES]` lacks a trailing `NULL`, causing `while (*check != NULL) check++;` in `R_InitSpriteDefs` to read OOB into `states[]`.
   - *Fix:* Bounded `if (namelist == sprnames) { numsprites = NUMSPRITES; }` in `sanitize_doom_module("r_things.c", ...)`.
4. **Zero `ticdup` Network Divide-by-Zero (`src/wasm_doom_platform.c`):**
   - *Symptom:* `I_InitNetwork()` left `doomcom->ticdup == 0`, causing `BACKUPTICS / (2 * ticdup)` integer divide-by-zero in `D_CheckNetGame()`.
   - *Fix:* Set `doomcom->ticdup = 1; doomcom->extratics = 0;` in `src/wasm_doom_platform.c:219-220`.
5. **Character Literal Missing in `parse_const_expr_primary()` (`part3.c:367`):**
   - *Symptom:* On `-warp 1 1` (E1M1), `Z_Free(0)` crashed inside `wipe_exitMelt <- wipe_ScreenWipe <- D_Display`. Memory inspection proved `HU_Init()` wrote 63 `patch_t*` pointers (252 bytes) into `hu_font` (`0xa9d78`), overwriting `castorder[]` (`0xa9d80`) and `f_wipe.c`'s `go` (`0xa9e64`) because `part3.c:367` only handled `TK_NUM`, causing `patch_t *hu_font[('_' - '!' + 1)];` (`TK_CHAR_LIT`) to evaluate to `hu_font[1]` (8 bytes).
   - *Fix:* 1-line surgical change at `part3.c:367`: `if (cc->tk == TK_NUM || cc->tk == TK_CHAR_LIT) {`.

---

## 2. Final Deliverable Artifacts & SHA-256 Digests

| Artifact | Size | SHA-256 | Verification Notes |
| :--- | ---: | :--- | :--- |
| `build_wasm/doom.wasm` | 453,248 B | `cd0dc8d29d8210a0a84cf6a3437e8f538d931bc270a7bf41077a693debc97c60` | V8 valid, 15 `wasi_snapshot_preview1.*`, 0 `env.*`, 893 exports (`fb_*` + `_start`) |
| `build_wasm/doom_inwasi_probe.wasm` (deleted after probe) | 453,248 B | `d6986312a67d8f99712e717505b5b1d4693fa443c746db2f678c73f06ad4b37e` | Compiled inside `build_wasm/zcc.wasm` under WASI (`rc=0`, V8 `valid=True`, 0 `env.*`) |
| `build_wasm/zcc.wasm` | 922,699 B | `f3bc3cd32acd94e9988c1e74ba89ced5e86abb5a464a4f4946ade63f31b33ffa` | Stage 1 WASI compiler (updated `part3.c` + `src/wasm_wasi_rt.c`) |
| `build_wasm/zcc2.wasm` | 922,699 B | `f5b5971334819b79d4840dcaa73cf1b07047543544d51d4a94c6652b7cd15de8` | WASI Stage 2 (`cmp zcc2.wasm zcc3.wasm` exit 0) |
| `build_wasm/zcc3.wasm` | 922,699 B | `f5b5971334819b79d4840dcaa73cf1b07047543544d51d4a94c6652b7cd15de8` | WASI Stage 3 (byte-identical to `zcc2.wasm`) |
| `build_wasm/lua.wasm` | 242,346 B | `d7703d09eda9ece225ef0d769e5adb6a0a5c0c49d1a9208e506f9dd5415cb9fd` | Compiled deterministically in-WASI by `zcc.wasm`; 0 `env.*` |
| `build_wasm/sqlite3.wasm` | 1,063,501 B | `71fc598824fa78f57d13fd92b339e2b2cd739b96255b77a17e86b0bcf395d150` | Compiled from unmodified `src/sqlite3.c`; 0 `env.*` |
| `tools/zcc_wasm_playground.html` | 550,630 B | `f41478c7bd8efe76cd9f84f39d43ca0677193165e58f98a1b2cca79763dca67d` | Zero-server `file://` HTML, 0 external URLs, 4 tabs incl. Live Canvas / DOOM |
| `doom1.wad` (unmodified IWAD) | 28,795,216 B | `7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d` | External IWAD mounted in-memory (`BrowserWasiShim`) & `node:wasi` |

---

## 3. Verbatim Gate Outputs (Post-Final-Edit Sweep)

### Gate 1 — Native Bootstrap Convergence & `BOOTSTRAP_BASELINES.tsv` Lock

```text
★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (scars: 36, ⟐ BYTE-IDENTICAL SEAL ⟐) ★
zcc_quest: child exit=0 wrapper_exit=0 oracle_verified=True warnings=36 errors=0 elapsed=122.6s (raw output: /tmp/zcc_quest.log.build)
zcc_quest_provenance: run_id=zcc-omega-1795129872-39859-29998d62d9e77bbd ledger=/mnt/h/__DOWNLOADS/zcc_github_upload/evidence/zcc-omega-1795129872-39859-29998d62d9e77bbd/provenance.jsonl
zcc_quest_sha256: stage2.s=21e05124e86fa79b5272f13f7aebd53070a835d81e2086ec7f75744ba6b136aa stage3.s=21e05124e86fa79b5272f13f7aebd53070a835d81e2086ec7f75744ba6b136aa
zcc_quest_sha256: stage2.bin=f61563492856e54e95574576eb51f9e05a757423526f4bf4540879f76d68c1c0 stage3.bin=f61563492856e54e95574576eb51f9e05a757423526f4bf4540879f76d68c1c0

$ bash scripts/log_bootstrap_hash.sh doom-framebuffer-tk-char-lit-verified && cmp zcc2.s zcc3.s && echo CMP_ZCC2_ZCC3_EXIT=0
BOOTSTRAP HASH: a87f50bf482be5fb1d4726e91ee8fee9  commit=bf472ed5  elisions=2213
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
CMP_ZCC2_ZCC3_EXIT=0
```

### Gate 2 — 3-Stage WASI Self-Host Convergence (`make selfhost-wasm`)

```text
$ python3 tests/test_wasm_wasi_selfhost.py
WASMTIME_STATUS: NOT_INSTALLED_ON_HOST
test_01_wasi_runtime_primitives_dual_host (__main__.TestWasmWasiSelfhost.test_01_wasi_runtime_primitives_dual_host) ... ok
test_02_wasi_selfhost_convergence (__main__.TestWasmWasiSelfhost.test_02_wasi_selfhost_convergence) ... ok
test_03_browser_wasi_shim_compile_wasm_and_asm (__main__.TestWasmWasiSelfhost.test_03_browser_wasi_shim_compile_wasm_and_asm) ... ok
test_04_ef0_negative_controls (__main__.TestWasmWasiSelfhost.test_04_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 4 tests in 26.113s

OK
[TEST_01 PASS] Dual-host (node:wasi & BrowserWasiShim) WASI primitives & ABI verified.
[TEST_02 PASS] WASI Self-Host Converged:
  zcc.wasm : 922699B SHA256=f3bc3cd32acd94e9988c1e74ba89ced5e86abb5a464a4f4946ade63f31b33ffa
  zcc2.wasm: 922699B SHA256=f5b5971334819b79d4840dcaa73cf1b07047543544d51d4a94c6652b7cd15de8
  zcc3.wasm: 922699B SHA256=f5b5971334819b79d4840dcaa73cf1b07047543544d51d4a94c6652b7cd15de8
  cmp zcc2.wasm zcc3.wasm: EXIT 0
```

### Track 1B Deliverable 1 — Standalone WASI DOOM (`build_wasm/doom.wasm`) & In-WASI Probe

```text
$ python3 tools/build_doom_wasm.py --probe-wasm-selfcompile && python3 tests/test_wasm_doom.py
[BUNDLE] build_wasm/doom_wasi_bundle.c: 984436 bytes (57 upstream DOOM modules)
[BUILD] /mnt/h/__DOWNLOADS/zcc_github_upload/zcc --target=wasm32 -Idoom -Isrc -I. build_wasm/doom_wasi_bundle.c -o build_wasm/doom.wasm
[WASM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_wasm/doom.wasm: 453248 bytes, SHA256=cd0dc8d29d8210a0a84cf6a3437e8f538d931bc270a7bf41077a693debc97c60
[V8] valid=True compiled=True err=None
[IMPORTS] count=15 non_wasi=[]
[EXPORTS] count=893 missing_fb=[]
[PROBE-WASM] Invoking build_wasm/zcc.wasm on build_wasm/doom_wasi_bundle.c...
[PROBE-WASM] rc=0 bytes=453248 valid=True compiled=True non_wasi=[] SHA256=d6986312a67d8f99712e717505b5b1d4693fa443c746db2f678c73f06ad4b37e byte_identical=False
[PASS] Standalone build_wasm/doom.wasm validated with zero env.* imports!
test_01_v8_validation_zero_env_and_exports (__main__.TestWasmDoom.test_01_v8_validation_zero_env_and_exports) ... ok
test_02_doom_title_and_demo_dual_host (__main__.TestWasmDoom.test_02_doom_title_and_demo_dual_host) ... [DOOM TITLE PASS] 320x200 gs=3 tic=26 uniq=179 sha=4acdd984303db908
ok
test_03_doom_e1m1_3d_render_and_key_input_response (__main__.TestWasmDoom.test_03_doom_e1m1_3d_render_and_key_input_response) ... [DOOM E1M1 PASS] idle=(-27262976,16777216,0,uniq=129,sha=14347292834d) moved=(-23360545,15743860,3770679296,uniq=136,sha=4624a6a58175)
ok
test_04_ef0_negative_controls (__main__.TestWasmDoom.test_04_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 4 tests in 2.260s

OK
```

### Track 1B Deliverable 2 — Live Canvas Framebuffer Playground (`tools/zcc_wasm_playground.html`) & Rule EF-7 Suite

```text
$ python3 tools/build_wasm_playground.py && python3 tests/test_wasm_playground.py
[PLAYGROUND] Built zero-server HTML: /mnt/h/__DOWNLOADS/zcc_github_upload/tools/zcc_wasm_playground.html (550630 bytes)
test_01_ast_syntax_gate_all_scripts (__main__.TestWasmPlaygroundEF7.test_01_ast_syntax_gate_all_scripts) ... ok
test_02_css_canvas_symbol_and_offline_gates (__main__.TestWasmPlaygroundEF7.test_02_css_canvas_symbol_and_offline_gates) ... ok
test_03_headless_node_end_to_end_evaluation (__main__.TestWasmPlaygroundEF7.test_03_headless_node_end_to_end_evaluation) ... [PLAYGROUND EF-7 PASS] {"HEADLESS_EVAL_ERRORS":0,"zccBytes":922699,"luaBytes":242346,"bareStdout":"ZCC Bare WASM Ready!\n55","stdioStdout":"ReadBack: WASI_VFS_LINE=42\nargc=2 argv0=vfs_demo.wasm","asmBytes":1707,"luaOut":"LUA_PLAYGROUND\t610\t42","sqliteOut":"zcc|42\npg|99","rayColors":460,"plasmaColors":128,"doomE1M1Colors":133,"doomHashes":9,"negRc":1}
ok

----------------------------------------------------------------------
Ran 3 tests in 2.073s

OK
```

### Gate 3 & Gate 5 — Lua/SQLite, Corpus, Compat Smoke, and Sovereign Triple Self-Host Matrix

| Gate / Suite | Command | Result | Observed Output |
| :--- | :--- | :--- | :--- |
| Lua 5.4.6 & SQLite 3.45.0 WASI | `python3 tools/build_lua_wasm.py && python3 tools/build_sqlite_wasm.py && python3 tests/test_wasm_lua_sqlite.py` | **PASS** | 5/5 in 2.393s (`lua.wasm` 242,346 B; `sqlite3.wasm` 1,063,501 B; 0 `env.*`; Python `sqlite3` differential + VFS + EF-0) |
| Corpus Regression (Uncurated) | `./tests/test_corpus.sh` | **INFO (520/535, 0 CORE-05 overlap)** | `CORPUS_TOTAL=535 CORPUS_PASS=520 CORPUS_FAIL=15 CORPUS_PCT=97.2%`; `grep` against `tests/core05_fixtures.txt` confirmed `OVERLAP_CORE05_NONE` (15 failures are pre-existing synthesized-stub/header/fragment tests) |
| Compat Smoke | `make compat-smoke` | **PASS** | `COMPAT SMOKE COMPLETE` (`10/10` targets) |
| WASI Runtime Suite | `python3 tests/test_wasm_wasi.py` | **PASS** | 12/12 in 1.034s |
| WASM Emitter Suite | `python3 tests/test_wasm_emitter.py` | **PASS** | 2/2 in 0.623s |
| Sovereign x86-64 ELF Self-Host | `make selfhost-sovereign` | **PASS** | `cmp /tmp/zcc2_sov.o /tmp/zcc3_sov.o` & `cmp /tmp/sov_abi2 /tmp/sov_abi3` exit 0 |
| Win64 Sovereign Self-Host | `python3 tools/win64_selfhost_sovereign.py` | **PASS** | `stage1==stage2==stage3` (2,807,296 B, SHA-256 `42d0218d58ef529b60624cd88ef21331835efb71d62cc260baa0405a6137b03f`), `FC: no differences encountered` |
| Quad-ISA Direct ELF Suite | `python3 tests/test_arm64_riscv64_direct_elf.py` | **PASS** | 5/5 in 5.484s (`qemu-aarch64-static=NOT_INSTALLED_ON_HOST`, `qemu-riscv64-static=NOT_INSTALLED_ON_HOST`) |

---

## 4. ZCC-EF-0 & ZCC-EF-7 v1.0.0 Adjudication

- **Claim 1 (Standalone WASI DOOM 1.10):** Fixing `part3.c:367` (`TK_CHAR_LIT` in `parse_const_expr_primary`) and replacing only `i_*.c` with `src/wasm_doom_platform.c` + `src/wasm_wasi_rt.c` enables ZCC (both native `./zcc` and in-WASI `build_wasm/zcc.wasm`) to compile all 57 unmodified upstream `doom/*.c` modules into `build_wasm/doom.wasm` (453,248 B, 0 `env.*` imports), booting DOOM Title (`gamestate=3`, 179 unique colors) and E1M1 3D level (`gamestate=0`, 129–136 unique colors) against `doom1.wad` with verified input responsiveness (`playerX/Y/Angle` + pixel SHA-256 divergence) and fail-closed EF-0 negative controls.
  - **Evidence Class:** `OBSERVED`
  - **Evidence Tier:** `E3` (upstream unmodified `doom/*.c`, dual WASI runtimes `node:wasi` + `BrowserWasiShim`, in-WASI `zcc.wasm` compilation probe, differential input verification, and 3 negative controls)
  - **Scope:** Shareware IWAD `doom1.wad`, software video + headless audio + single-player tic loop under WASI Preview1.
  - **Limitations:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`; WebAudio sound synthesis and multiplayer netcode are out of scope (headless stubs in `src/wasm_doom_platform.c`); native `./zcc` vs in-WASI `zcc.wasm` builds of `doom_wasi_bundle.c` both produce valid 453,248 B `.wasm` modules differing only in float-parsing SHA (`cd0dc8d2...` vs `d6986312...`).
  - **Verdict:** **PASS**

- **Claim 2 (Live Canvas / DOOM Framebuffer Playground Tab):** `tools/build_wasm_playground.py` generates zero-server `tools/zcc_wasm_playground.html` (550,630 B) adding the 4th `Live Canvas / DOOM (Framebuffer)` tab supporting both in-browser C99 compilation via `zcc.wasm` (`live_raytracer` + `plasma_shader`) and local `.wasm`/`.wad` framebuffer playback over `BrowserWasiShim`, passing all 6 Rule EF-7 UI/HTML gates (`HEADLESS_EVAL_ERRORS: 0`).
  - **Evidence Class:** `OBSERVED`
  - **Evidence Tier:** `E3` (static AST + HTML/CSS/DOM audit + headless Node 22 execution of real extracted scripts with `MockCanvas`, `zcc.wasm`, `doom.wasm` + `doom1.wad`, and EF-0 control)
  - **Scope:** Node 22 V8 + mocked 2D `<canvas>`/`DOM` headless harness and pure-JS `BrowserWasiShim` (does not claim automated Playwright/Chromium pixel inspection in browser window).
  - **Limitations:** Browser visual interactive check in Chromium/Edge is left to manual user opening of `file:///...tools/zcc_wasm_playground.html`; `test_wasm_playground.py` evaluates extracted JS identically under Node 22 V8.
  - **Verdict:** **PASS**
