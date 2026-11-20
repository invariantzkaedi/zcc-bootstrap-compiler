# Gate Evidence: Track 3A (Native Wasm EH `setjmp`/`longjmp` + Lua 5.4 `pcall`) & Track 3B (Standalone Upstream MicroPython `v1.24.1` WASI)

**Tickets / Deliverables:**
- **Track 3A:** Native WebAssembly Exception Handling (`Tag` Section `id=13`, `try_table` `0x1F`, `throw` `0x08`) for C99 `setjmp`/`_setjmp`/`sigsetjmp` + `longjmp`/`_longjmp`/`siglongjmp` and real Lua 5.4 `pcall()`/`xpcall()`/`error()` stack unwinding (`build_wasm/lua.wasm`) with zero `env.*` imports and MVP purity when unused.
- **Track 3B:** Standalone Upstream MicroPython `v1.24.1` Python 3 WASI VM (`build_wasm/micropython.wasm`) compiled from unmodified upstream C sources by both `./zcc --target=wasm32` and in-WASI `build_wasm/zcc.wasm` (**byte-identical**), using `MICROPY_NLR_SETJMP (1)` over Track 3A's native Wasm EH.
- **Compiler Hardening Discovered & Fixed Along the Way:** Nested `ND_MEMBER` evaluation in `wasm_eval_ginit_ptr_const` (`sys.argv`, `sys.modules`), Flexible Array Member (FAM) global sizing/initialization (`qstr_pool_t.qstrs[]`, `mp_obj_type_t.slots[]`), and local `struct`/`union` zero-initializer 4B-vs-8B ILP32 stride normalization (`wasm_fixup_local_struct_zero_inits` in `part6_wasm.c` — zero edits to `part0_pp.c`..`part5.c`).

**Governing Rule Version:** `ZCC-EF-7 v1.0.0` (and `ZCC-EF-0`)  
**Evidence Tier Assigned:** `E2` (`evidence_tier_assigned_at`: `2026-11-19T21:16:00-08:00`, predating final verification runs)  
**Host Environment:** Linux `6.18.58.1-microsoft-standard-WSL2` x86_64, `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, Node `v22.23.5` (`/home/ZKAEDI/.local/bin/node`)  
**Host Runtime Availability:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` | `QEMU_STATUS: NOT_INSTALLED_ON_HOST`  
**Base Commit (`HEAD`):** `d4b90a9ddf6e9734514536330f6d740fa4f835b8`

---

## 1. Summary of Changes

Zero edits were made to `part0_pp.c`..`part5.c` (`git diff --stat -- part0_pp.c part1.c part2.c part3.c part4.c part5.c` is empty), and all 174 official upstream MicroPython `v1.24.1` C/H/script/license files extracted to `tests/embedded_micropython/` remain unmodified (`tests/embedded_micropython/` is gitignored alongside `tests/embedded_lua/`, `tests/embedded_quickjs/`, and `sqlite3.c`, and deterministically verified by SHA-256 manifest in `tools/build_micropython_wasm.py` and `test_01` of `tests/test_wasm_micropython.py`).

1. **`src/wasm_emit.h` & `src/wasm_emit.c`:**
   - Added `WASM_SEC_TAG = 13`, `WASM_OP_THROW = 0x08`, `WASM_OP_TRY_TABLE = 0x1F`, `WASM_CATCH_TAG = 0x00`, `WasmLocalMap.sj_val_local_idx`, and `WasmModule.{has_longjmp_tag, longjmp_tag_type_idx}`.
   - `wasm_module_emit_binary` emits Section `13` (`Tag`) between `WASM_SEC_MEMORY` (`5`) and `WASM_SEC_GLOBAL` (`6`) only when `mod->has_longjmp_tag && mod->longjmp_tag_type_idx >= 0` (preserving pure WASM MVP binary output when `setjmp`/`longjmp` are unused).

2. **`part6_wasm.c`:**
   - **Track 3A Wasm EH `setjmp`/`longjmp` lowering:** Added `wasm_is_setjmp_name`/`wasm_is_longjmp_name`, `wasm_ast_has_setjmp`/`wasm_ast_has_longjmp` module-level scanning (ignoring `wasm_wasi_rt.c` wrapper bodies), `WASM_CFG_SETJMP`, `wasm_cfg_extract_setjmp` + left-hand `ND_COMMA_EXPR` side-effect hoisting (e.g., MicroPython's `nlr_push_tail`), CFG `try_table (catch 0 $caught)` dispatcher with `$fp` frame disambiguation (rethrow on non-matching frame), `__stack_pointer` (`global 0`) + `$sj_val` + `$pc` restoration, `frame_size >= 16` enforcement for `setjmp` callers, and direct `throw 0` emission in `longjmp`/`__zcc_wasm_longjmp`.
   - **Global Initializer Fixes (`wasm_register_global`, `wasm_fill_ginit`, `wasm_eval_ginit_ptr_const`):** Added Flexible Array Member (FAM) global struct sizing and tail-element emission (supporting `qstr_pool_t.qstrs[]` and `mp_obj_type_t.slots[]`), plus top-level recursive `ND_MEMBER` resolution in `wasm_eval_ginit_ptr_const` so multi-level global member addresses (`&mp_state_ctx.vm.mp_sys_argv_obj`, `&mp_state_ctx.vm.dict_locals_top`) emit exact non-NULL static offsets.
   - **Local Struct/Union Zero-Init Stride Normalization (`wasm_fixup_local_struct_zero_inits`):** Normalized compiler-synthesized 8-byte `TY_LONG` zero-fill writes emitted by `part3.c` into `cc->ty_longlong` (`i64.store 0`) on `--target=wasm32` (ILP32), fixing half-zeroed odd 4-byte stack words on local structs such as `compiler_t comp_state = {0}` without touching `part3.c`.

3. **`src/wasm_wasi_rt.c`:**
   - Delegated `longjmp`, `_longjmp`, and `siglongjmp` to `extern void __zcc_wasm_longjmp(jmp_buf env, int val);` and added `#ifndef SSIZE_MAX`, IEEE-754 `nan(const char *)`, `nearbyint(double)`, and `nearbyintf(float)`.

4. **Track 3B Driver, Builder, Playground, Regression Suites & Baseline Ledger:**
   - `src/wasm_mpy_driver.c`: Standalone MicroPython WASI CLI/VFS driver (`-c`, `<script.py>`, `-`/stdin, `-v`/`--version`, WASI VFS `mp_lexer_new_from_file` / `mp_import_stat` / `open()` `TextIOWrapper`/`FileIO`, 8 MB static GC heap + 32 KB `pystack`, `gc_collect()` linear C-stack scanning, fail-closed `SystemExit` & stderr traceback handling).
   - `tools/build_micropython_wasm.py`: Fetches and SHA-256 verifies upstream MicroPython `v1.24.1` tarball (`a291173de1d680786a66dd2381805b6d134defaecea670a560dc7e7d8de540ad`) and 174-file subset (`py/`, `extmod/mod{heapq,json,re,random}.c`, `lib/re1.5/`, `shared/readline/readline.h`), generates `genhdr/` via `./zcc --pp-only -DNO_QSTR` + upstream Python generators (`makeqstrdata.py`, `makemoduledefs.py`, `make_root_pointers.py`), bundles 96 modules + 439 QSTRs + 15 builtin modules (`1,584,617` bytes), builds `build_wasm/micropython.wasm` (`410,303` bytes, 15 WASI imports, 0 `env.*`), and verifies `--probe-wasm-selfcompile`.
   - `tests/test_wasm_setjmp_pcall.py`: 5-test C99 `setjmp`/`longjmp` + Lua 5.4 `pcall`/`xpcall` + local struct zero-init & nested global member regression suite (Node `WASI` + `BrowserWasiShim` + negative controls + MVP Section-13 omission proof).
   - `tests/test_wasm_lua_sqlite.py`: Added `test_02b_lua54_pcall_xpcall_error_recovery`.
   - `tests/test_wasm_micropython.py`: 6-test Python 3 differential & dual-host verification suite against host `python3`, GC stress (`30,000` iterations), WASI VFS (`import`, `open()`, `stdin`), Playground `ZccPlaygroundEngine.runExternalWasm`, and 6 ZCC-EF-0 negative controls.
   - `tools/build_wasm_playground.py`, `tools/zcc_wasm_playground.html`, `Makefile`, `.gitignore`, `BOOTSTRAP_BASELINES.tsv`.

---

## 2. Synchronized Artifact Inventory & Digests (OBSERVED)

| Artifact | Size (bytes) | SHA-256 | Notes |
| --- | ---: | --- | --- |
| `build_wasm/zcc.wasm` | 952,449 | `b68f9f17cac7fb9f343fcf0c911128e04f1b38ede3eb442edea3c8fe85b793b0` | WASI self-host compiler (`== zcc2.wasm == zcc3.wasm`, `cmp` exit 0) |
| `build_wasm/lua.wasm` | 256,095 | `ff0aab5360a1ab454ad655dcdb62fbed17a881a1a12878432a3611f3e3c252c6` | **Byte-identical** across `./zcc` and `zcc.wasm`; Section `13` (`Tag`) enabled; 0 `env.*` |
| `build_wasm/sqlite3.wasm` | 1,077,021 | `1e3737ae757b0f49c0388387df26c3490bd6c1fbdc07f84ae7b98dce4d35cf20` | Rebuilt & verified dual-host; 0 `env.*` |
| `build_wasm/qjs.wasm` | 1,195,889 | `50bf65c9a5797a0a4937da24feaf3d8a610777f54f3a6f7608717982be7e7187` | **Byte-identical** across `./zcc` and `zcc.wasm`; `exec_ok=True`; 0 `env.*` |
| `build_wasm/doom.wasm` | 466,901 | `812b59a0e9bb0e326251fc7478f81d640128f38a7ea742c82f7f7a91a2df4c40` | Rebuilt & verified (title + E1M1 3D motion); 0 `env.*` |
| `build_wasm/micropython.wasm` | 410,303 | `3be0021cebeda56390e0ee9450e7051d9c3264244541a13a4dc856434aeb4a18` | **Byte-identical** across `./zcc` and `zcc.wasm`; Section `13` (`Tag`), 15 WASI imports, 0 `env.*` |
| `tools/zcc_wasm_playground.html` | 573,626 | `a3e636182d616c246e0619b137ec68869ed028dea7506bb7f0056e69a613be43` | Zero-server HTML playground (`HEADLESS_EVAL_ERRORS: 0`) |
| `zcc2.s` / `zcc3.s` | 11,503,745 | `11bbad5f75271bb8f74ad0090e196fd3837370c279ea3c83a09cd87f4e316633` | MD5 `50984b68bdc50f46620638375e9c0472` (`cmp -l` exit 0) |
| `stage2.bin` / `stage3.bin` | — | `a9763f140ea92d236ce478cee718219ab8c8119206c1694475b240b4eea22d30` | Verified by `zcc_quest` Gate 1 |

---

## 3. Verbatim Gate Execution Outputs

### Gate 1 — Native x86-64 Self-Host Bootstrap & Baseline Ledger Lock

```text
 ✔    ✨ SELFHOST-MV1.3b PASS
 │    Bootstrap fixed-point, semantic-canary, and full SystemV ABI constraints satisfied.
 ★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (scars: 36, ⟐ BYTE-IDENTICAL SEAL ⟐) ★
zcc_quest: child exit=0 wrapper_exit=0 oracle_verified=True warnings=36 errors=0 elapsed=128.5s (raw output: /tmp/zcc_quest.log.build)
zcc_quest_sha256: stage2.s=11bbad5f75271bb8f74ad0090e196fd3837370c279ea3c83a09cd87f4e316633 stage3.s=11bbad5f75271bb8f74ad0090e196fd3837370c279ea3c83a09cd87f4e316633
zcc_quest_sha256: stage2.bin=a9763f140ea92d236ce478cee718219ab8c8119206c1694475b240b4eea22d30 stage3.bin=a9763f140ea92d236ce478cee718219ab8c8119206c1694475b240b4eea22d30
CMP_RC=0
50984b68bdc50f46620638375e9c0472  zcc2.s
50984b68bdc50f46620638375e9c0472  zcc3.s
11bbad5f75271bb8f74ad0090e196fd3837370c279ea3c83a09cd87f4e316633  zcc2.s
11bbad5f75271bb8f74ad0090e196fd3837370c279ea3c83a09cd87f4e316633  zcc3.s

BOOTSTRAP HASH: 50984b68bdc50f46620638375e9c0472  commit=d4b90a9d  elisions=2213
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
```

### Gate 2 — WASI Self-Host Convergence (`tests/test_wasm_wasi_selfhost.py`)

```text
WASMTIME_STATUS: NOT_INSTALLED_ON_HOST
test_01_wasi_runtime_primitives_dual_host (__main__.TestWasmWasiSelfhost.test_01_wasi_runtime_primitives_dual_host) ... ok
test_02_wasi_selfhost_convergence (__main__.TestWasmWasiSelfhost.test_02_wasi_selfhost_convergence) ... ok
test_03_browser_wasi_shim_compile_wasm_and_asm (__main__.TestWasmWasiSelfhost.test_03_browser_wasi_shim_compile_wasm_and_asm) ... ok
test_04_ef0_negative_controls (__main__.TestWasmWasiSelfhost.test_04_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 4 tests in 27.047s

OK
[TEST_01 PASS] Dual-host (node:wasi & BrowserWasiShim) WASI primitives & ABI verified.
[TEST_02 PASS] WASI Self-Host Converged:
  zcc.wasm : 952449B SHA256=b68f9f17cac7fb9f343fcf0c911128e04f1b38ede3eb442edea3c8fe85b793b0
  zcc2.wasm: 952449B SHA256=b68f9f17cac7fb9f343fcf0c911128e04f1b38ede3eb442edea3c8fe85b793b0
  zcc3.wasm: 952449B SHA256=b68f9f17cac7fb9f343fcf0c911128e04f1b38ede3eb442edea3c8fe85b793b0
  cmp zcc2.wasm zcc3.wasm: EXIT 0
[TEST_03 PASS] Pure-JS BrowserWasiShim: fib(10)=55 (.wasm 271B) & -S 2281B byte-identical to native ./zcc.
[TEST_04 PASS] EF-0 Negative Controls verified: invalid C rc=1 (no .wasm), 1-byte mutation caught by cmp/SHA-256, corrupt magic caught by WebAssembly.validate()=False.
```

### Track 3B Build & In-WASI Self-Compile Probe + Dual-Host Suite (`build_micropython_wasm.py`, `test_wasm_micropython.py`)

```text
[BUNDLE] build_wasm/micropython_wasi_bundle.c: 1584617 bytes (96 upstream MicroPython C modules, 439 QSTRs, 15 builtin modules)
[BUILD] /mnt/h/__DOWNLOADS/zcc_github_upload/zcc --target=wasm32 -Ibuild_wasm/mpy_genhdr -Itests/embedded_micropython -Isrc -I. build_wasm/micropython_wasi_bundle.c -o build_wasm/micropython.wasm
[WASM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_wasm/micropython.wasm: 410303 bytes, SHA256=3be0021cebeda56390e0ee9450e7051d9c3264244541a13a4dc856434aeb4a18
[V8] valid=True compiled=True sections=[1, 2, 3, 4, 5, 13, 6, 7, 9, 10, 11, 0, 0] err=None
[IMPORTS] count=15 non_wasi=[]
[PROBE-WASM] Invoking build_wasm/zcc.wasm on build_wasm/micropython_wasi_bundle.c...
[PROBE-WASM] rc=0 bytes=410303 valid=True compiled=True non_wasi=[] exec_ok=True SHA256=3be0021cebeda56390e0ee9450e7051d9c3264244541a13a4dc856434aeb4a18 byte_identical=True
[PASS] Standalone build_wasm/micropython.wasm validated with zero env.* imports!

test_01_v8_validation_tag13_zero_env_and_clean_upstream (__main__.TestWasmMicroPython.test_01_v8_validation_tag13_zero_env_and_clean_upstream) ... ok
test_02_python3_differential_vs_cpython_dual_host (__main__.TestWasmMicroPython.test_02_python3_differential_vs_cpython_dual_host) ... ok
test_03_gc_collect_stress_and_random (__main__.TestWasmMicroPython.test_03_gc_collect_stress_and_random) ... ok
test_04_wasi_vfs_script_import_open_and_stdin_dual_host (__main__.TestWasmMicroPython.test_04_wasi_vfs_script_import_open_and_stdin_dual_host) ... ok
test_05_playground_external_runner_runs_micropython_wasm (__main__.TestWasmMicroPython.test_05_playground_external_runner_runs_micropython_wasm) ... ok
test_06_ef0_negative_controls (__main__.TestWasmMicroPython.test_06_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 6 tests in 1.808s

OK
```

### Track 3A Suite (`tests/test_wasm_setjmp_pcall.py`) & Synchronized Regression / Sovereign Sweep

```text
test_c99_setjmp_longjmp_suite (__main__.TestWasmSetjmpAndLuaPcall.test_c99_setjmp_longjmp_suite) ... ok
test_local_struct_zero_init_and_nested_global_member (__main__.TestWasmSetjmpAndLuaPcall.test_local_struct_zero_init_and_nested_global_member) ... ok
test_lua_real_pcall_xpcall_both_hosts (__main__.TestWasmSetjmpAndLuaPcall.test_lua_real_pcall_xpcall_both_hosts) ... ok
test_mvp_purity_when_setjmp_unused (__main__.TestWasmSetjmpAndLuaPcall.test_mvp_purity_when_setjmp_unused) ... ok
test_uncaught_longjmp_negative_control (__main__.TestWasmSetjmpAndLuaPcall.test_uncaught_longjmp_negative_control) ... ok

----------------------------------------------------------------------
Ran 5 tests in 2.676s

OK
```

- `python3 tools/build_lua_wasm.py`: `[PARITY] native=./zcc == in-WASI=zcc.wasm SHA256=ff0aab5360a1ab454ad655dcdb62fbed17a881a1a12878432a3611f3e3c252c6`, `count=15 non_wasi=[]`, exit `0`.
- `python3 tools/build_sqlite_wasm.py`: `1,077,021` bytes, `SHA256=1e3737ae757b0f49c0388387df26c3490bd6c1fbdc07f84ae7b98dce4d35cf20`, `non_wasi=[]`, exit `0`.
- `python3 tools/build_quickjs_wasm.py --probe-wasm-selfcompile`: `[PROBE-WASM] rc=0 bytes=1195889 valid=True compiled=True non_wasi=[] exec_ok=True SHA256=50bf65c9a5797a0a4937da24feaf3d8a610777f54f3a6f7608717982be7e7187 byte_identical=True`, exit `0`.
- `python3 tools/build_doom_wasm.py`: `466,901` bytes, `SHA256=812b59a0e9bb0e326251fc7478f81d640128f38a7ea742c82f7f7a91a2df4c40`, `non_wasi=[]`, `missing_fb=[]`, exit `0`.
- `python3 tests/test_wasm_lua_sqlite.py`: `Ran 6 tests in 2.469s — OK` (includes `test_02b_lua54_pcall_xpcall_error_recovery`).
- `python3 tests/test_wasm_quickjs.py`: `Ran 6 tests in 2.269s — OK`.
- `python3 tests/test_wasm_doom.py`: `Ran 4 tests in 2.441s — OK` (`[DOOM TITLE PASS] 320x200 gs=3 tic=26 uniq=179 sha=4acdd984303db908`, `[DOOM E1M1 PASS] idle=(-27262976,16777216,0,uniq=129,sha=14347292834d) moved=(-23360545,15743860,3770679296,uniq=136,sha=4624a6a58175)`).
- `python3 tests/test_wasm_playground.py`: `Ran 3 tests in 2.376s — OK` (`[PLAYGROUND EF-7 PASS] {"HEADLESS_EVAL_ERRORS":0,"zccBytes":952449,"luaBytes":256095,...}`).
- `python3 tests/test_wasm_wasi.py`: `Ran 12 tests in 0.972s — OK`.
- `python3 tests/test_wasm_emitter.py`: `Ran 2 tests in 0.309s — OK`.
- `make selfhost-sovereign`: `cmp /tmp/zcc2_sov.o /tmp/zcc3_sov.o`, `cmp /tmp/sov_fp2 /tmp/sov_fp3`, `cmp /tmp/sov_abi2 /tmp/sov_abi3`, `[PASS] selfhost-sovereign: byte-identical zero-binutils object & ELF emission verified.`
- `python3 tools/win64_selfhost_sovereign.py`: `[PASS] WIN64 SOVEREIGN ZERO-TOOLCHAIN SELF-HOST VERIFIED` (`stage1==stage2==stage3`, `2845184` B, `SHA256=447a8f7342931c165f4b2980c768bab6d0fa8486cd6943e7ed334029c43f277d`, `FC: no differences encountered`, Probes 1–3 + EF-0 Negatives 1–2 PASS).
- `python3 tests/test_arm64_riscv64_direct_elf.py`: `Ran 5 tests in 5.816s — OK` (`[QUAD-ELF-INFO] qemu-aarch64-static=NOT_INSTALLED_ON_HOST qemu-riscv64-static=NOT_INSTALLED_ON_HOST`).

---

## 4. ZCC-EF-0 Negative Controls & ZCC-EF-7 v1.0.0 Adjudications

| Control | Proposition Tested | Observed Failure / Rejection | Evidence Class | Verdict |
| --- | --- | --- | --- | --- |
| Track 3A `uncaught_longjmp` | Uncaught `longjmp` fails closed in both hosts | `node:wasi` `returncode != 0`, `BrowserWasiShim` `trap`/`exitCode != 0`, `"SHOULD_NOT_REACH"` absent | `OBSERVED` | **PASS** |
| Track 3A `mvp_purity` | Section `13` omitted when `setjmp`/`longjmp` unused | `13 not in sectionIds`, V8 valid, `MVP_NO_TAG` printed | `OBSERVED` | **PASS** |
| Track 3A Lua uncaught `error()` | Uncaught Lua error propagates non-zero exit | `neg.returncode != 0`, `"uncaught_boom"` in stderr/stdout | `OBSERVED` | **PASS** |
| Track 3B `NC-1` | Python uncaught `RuntimeError` exits non-zero | `returncode == 1`, `"RuntimeError: nc1_boom"` on stderr | `OBSERVED` | **PASS** |
| Track 3B `NC-2` | Python `SyntaxError` fails closed | `returncode == 1`, `"SyntaxError"` on stderr | `OBSERVED` | **PASS** |
| Track 3B `NC-3` | `sys.exit(7)` preserves exact non-zero exit code | `returncode == 7` | `OBSERVED` | **PASS** |
| Track 3B `NC-4` | `--bogus-flag` and bare `-c` fail closed | `returncode == 2`, explicit diagnostics on stderr | `OBSERVED` | **PASS** |
| Track 3B `NC-5` | Missing VFS script fails closed in `BrowserWasiShim` | `exitCode == 1`, `"can't open file"` on stderr | `OBSERVED` | **PASS** |
| Track 3B `NC-6` | 1-byte mutation fails V8 validation | `WebAssembly.validate() == false` | `OBSERVED` | **PASS** |

- **Adjudication 1 (Track 3A):**
  - **Claim:** ZCC natively emits WebAssembly Exception Handling (`Tag` Section `13`, `try_table` `0x1F`, `throw` `0x08`) for C99 `setjmp`/`longjmp` and upstream Lua `5.4.6` `pcall`/`xpcall`/`error` with zero `env.*` imports and MVP Section-13 omission when unused.
  - **Evidence Class:** `OBSERVED`
  - **Evidence Tier:** `E2`
  - **Evidence:** `tests/test_wasm_setjmp_pcall.py` (`5/5 OK`), `tests/test_wasm_lua_sqlite.py` (`6/6 OK`, incl. `test_02b_lua54_pcall_xpcall_error_recovery`), `tools/build_lua_wasm.py` byte-identical `./zcc == zcc.wasm` parity (`ff0aab5360a1ab454ad655dcdb62fbed17a881a1a12878432a3611f3e3c252c6`).
  - **Scope:** Verified under Node `v22.23.5` (`node:wasi` and `tools/browser_wasi_shim.js`).
  - **Limitations:** `wasmtime` is not installed on host (`WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`).
  - **Verdict:** **PASS**

- **Adjudication 2 (Track 3B + Compiler Hardening):**
  - **Claim:** Standalone upstream MicroPython `v1.24.1` (`build_wasm/micropython.wasm`) compiles cleanly and byte-identically from unmodified upstream C sources via both `./zcc --target=wasm32` and in-WASI `build_wasm/zcc.wasm` (`3be0021cebeda56390e0ee9450e7051d9c3264244541a13a4dc856434aeb4a18`), matching CPython `python3` across dual hosts (`node:wasi` + `BrowserWasiShim` + `tools/zcc_wasm_playground.html`), with all Gate 1–5 compiler and sovereign verification suites passing.
  - **Evidence Class:** `OBSERVED`
  - **Evidence Tier:** `E2`
  - **Evidence:** `tools/build_micropython_wasm.py --probe-wasm-selfcompile`, `tests/test_wasm_micropython.py` (`6/6 OK`), `tests/test_wasm_wasi_selfhost.py` (`4/4 OK`), `make selfhost` (`cmp zcc2.s zcc3.s` exit `0`), `scripts/log_bootstrap_hash.sh --check` (`BOOTSTRAP_HASH_EXIT=0`), `tests/test_wasm_quickjs.py` (`6/6`), `tests/test_wasm_doom.py` (`4/4`), `tests/test_wasm_playground.py` (`3/3`), `tests/test_wasm_wasi.py` (`12/12`), `tests/test_wasm_emitter.py` (`2/2`), `make selfhost-sovereign`, `tools/win64_selfhost_sovereign.py`, `tests/test_arm64_riscv64_direct_elf.py` (`5/5`).
  - **Scope:** Upstream `v1.24.1` subset (`py/`, `extmod/mod{heapq,json,re,random}.c`, `lib/re1.5/`, `shared/readline/readline.h`) with fixed `8 MB` GC heap and `32 KB` `pystack`, verified under Node `v22.23.5`.
  - **Limitations:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` and `QEMU_STATUS: NOT_INSTALLED_ON_HOST`.
  - **Verdict:** **PASS**
