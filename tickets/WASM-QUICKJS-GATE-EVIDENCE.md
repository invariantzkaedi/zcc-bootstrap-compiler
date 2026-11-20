# Gate Evidence: Standalone Upstream QuickJS 2024-01-13 ES2023 WASI Module (`build_wasm/qjs.wasm`)

**Ticket / Deliverable:** Track 2 — Standalone Upstream QuickJS 2024-01-13 WASI (`build_wasm/qjs.wasm`) compiled by ZCC (`./zcc --target=wasm32`) with zero `env.*` imports, hardened for exact IEEE-754 big-int `strtod`/decimal formatting, pipe-safe I/O, fail-closed CLI parsing, and byte-identical in-WASI compilation (`./zcc == build_wasm/zcc.wasm`)  
**Governing Rule Version:** `ZCC-EF-7 v1.0.0` (and `ZCC-EF-0`)  
**Evidence Tier Assigned:** `E2` (`evidence_tier_assigned_at`: `2026-11-19T17:03:50-08:00`, predating post-hardening verification runs)  
**Host Environment:** Linux `6.18.58.1-microsoft-standard-WSL2` x86_64, `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, Node `v22.23.5` (`/home/ZKAEDI/.local/bin/node`)  
**Host Runtime Availability:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` | `QEMU_STATUS: NOT_INSTALLED_ON_HOST`

---

## 1. Summary of Changes & Root-Cause Compiler/Runtime Fixes

Zero edits were made to `part0_pp.c`..`part5.c` (`git diff --stat part0_pp.c part1.c part2.c part3.c part4.c part5.c` is empty), and all 16 official upstream QuickJS `2024-01-13` MIT source/header files in `tests/embedded_quickjs/` remain unmodified (`malloc_is_forbidden` intact in `quickjs.c`). Consistent with `tests/embedded_lua/` and `sqlite3.c`, `tests/embedded_quickjs/` is listed in `.gitignore` per `tools/pre-commit-binary-tripwire.py` (`1.00 MB` file limit vs `quickjs.c` `1.73 MB`) and is deterministically fetched/verified by `ensure_quickjs_sources()` in `tools/build_quickjs_wasm.py` against the upstream tarball SHA-256 (`3c4bf8f895bfa54beb486c8d1218112771ecfc5ac3be1036851ef41568212e03`) and 16-file SHA-256 manifest.

1. **`part6_wasm.c`:**
   - **Linear-stack `alloca` / `__builtin_alloca` lowering & Pass-4 frame enforcement:** Added 16-byte aligned bump allocation via WASM global stack pointer (`global 0`) and forced a non-zero `$fp` prologue/epilogue restore frame when a function calls `alloca`/`__builtin_alloca` (required by `JS_CallInternal` and `libregexp`). Added `"alloca"` to `wasm_is_runtime_reserved_name()`.
   - **Global initializer completion (`wasm_fill_ginit` & `wasm_eval_ginit_double_const`):** Added designated member lookup (`elem->member_name` via `find_struct_member`), packed global bitfields, 1-arg `ND_STR` `TY_ARRAY` init, single-element union/struct unwrapping (required by `JSCFunctionListEntry.u` dispatch tables for `Math.*`, `JSON.*`, `Array.prototype.*`, `BigInt.*`), constant `double` expr eval, and restricted pointer-size shortcut to `kind == TY_PTR` instead of `is_pointer()` (which also matched `TY_ARRAY` and truncated 2D tables such as `inv_log2_radix[35][2]` in `libbf.c`).
   - **Non-trapping float-to-int conversions (`wasm_emit_numeric_conversion`):** Switched `F32`/`F64` -> `I32`/`I64` casts from trapping `0xA8..0xB1` opcodes to standard WebAssembly `trunc_sat` (`0xFC 0x00..0x07`), fixing `JS_NewFloat64` (`quickjs.h:557`) traps on out-of-range doubles (e.g., `Number.MAX_SAFE_INTEGER`) and `NaN`/`Inf`.

2. **`src/wasm_wasi_rt.c`:**
   - Added C99 IEEE-754 helpers required by QuickJS: `FE_TONEAREST`..`FE_TOWARDZERO`, `fegetround`, `fesetround`, `_wasi_signbit`/`signbit` macro, `lrint`, `hypot`, `cbrt`, `expm1`, `log1p`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, and preserved IEEE-754 `-0.0` in `sin`, `atan`, `atan2`, `expm1`, `log1p`.
   - **Hardening Pass — Exact IEEE-754 `strtod` & `_wasi_rt_f64_exact_dec` (`%e`/`%E`/`%g`/`%G`):**
     - Replaced repeated `p10[16]` float scaling in `strtod` with exact Clinger fast-path plus zero-malloc 48-limb base-`2^32` big-int IEEE-754 round-to-nearest-even (`pow5_tab`, normal + subnormal rounding, case-insensitive `inf`/`infinity`/`nan`, `-0.0`).
     - Added `_wasi_rt_f64_exact_dec` (base-`10^9` stack multi-precision integer extraction + IEEE-754 round-to-nearest-even with sticky bit) for `%e`/`%E` and `%g`/`%G` formatting, eliminating subnormal/extreme exponent ULP drift (`1e-20`, `1e-300`, `1e300`, `Number.EPSILON`, `Number.MIN_VALUE`, `Number.MAX_VALUE`, `toPrecision(4)`).
     - Side-effect bonus verified after rebuild: **eliminated the long-standing glibc-vs-WASI float-literal parser delta**, making BOTH `qjs.wasm` and `lua.wasm` **100% byte-identical** whether compiled by native `./zcc` or in-WASI `build_wasm/zcc.wasm`.

3. **Driver, Builder, Suite, Playground & Makefile:**
   - `src/wasm_qjs_driver.c`: Standalone QuickJS CLI/VFS driver (`-e`/`--eval`, `-p`, `-h`/`--help`, `--`, `-` stdin, `JS_ExecutePendingJob` Promise draining, stderr `.stack` traces, `print`, `printErr`, `console.{log,error}`, `wasiReadFile`, `wasiWriteFile`); hardened `qjs_read_all_file` to loop until `fread(...) == 0` (short-pipe safe) and fail closed with exit code `2` on missing option arguments or unknown `-*` flags.
   - `tools/build_quickjs_wasm.py`: Bundles unmodified `tests/embedded_quickjs/{cutils,libunicode,libregexp,libbf,quickjs}.c` + `src/wasm_wasi_rt.c` + `src/wasm_qjs_driver.c` into `build_wasm/qjs_wasi_bundle.c` (`2,207,499` bytes), compiles via `./zcc --target=wasm32`, validates in V8 (`0` `env.*` imports), and verifies `--probe-wasm-selfcompile` with both V8 inspection and live BigInt WASI execution (`exec_ok=True`, `byte_identical=True`).
   - `tests/test_wasm_quickjs.py`: 6-test differential & dual-host verification suite against Node 22 V8 across `node:wasi`, `tools/browser_wasi_shim.js`, Playground `ZccPlaygroundEngine.runExternalWasm`, VFS file I/O + stdin, 5 ZCC-EF-0 negative controls (`NC-1..NC-5`), and `test_06_es2023_adversarial_stress_vs_node_v8` (26 edge IEEE-754 floats, generators, custom Error `try/catch/finally`, `DataView`/`ArrayBuffer`/`BigInt64Array`, 200-bit negative `BigInt`, lookbehind + named RegExp + Unicode NFC, `eval`/`Function`/`Reflect`/`WeakMap`, 30k-object GC churn, `async function*` + `for await` + `Promise.all`).
   - `tools/build_wasm_playground.py` & `Makefile`: Updated External WASI Runner UI labels for `qjs.wasm` and added `build-quickjs-wasm` and `test-wasm-quickjs` targets.

---

## 2. Final Post-Hardening Artifact Inventory & Digests (OBSERVED)

| Artifact | Size (bytes) | SHA-256 | Notes |
| --- | ---: | --- | --- |
| `build_wasm/qjs.wasm` | 1,195,627 | `62e925bd4507a9d7d39bedad9130caed9806e59f4e51144f224c3cb8fd017d18` | Compiled by `./zcc --target=wasm32` **and** byte-identical via `zcc.wasm`; 15 WASI imports, 0 `env.*` |
| `build_wasm/zcc.wasm` | 941,771 | `7c4dd78317a10a1b263f34cea085e797e2cf446990358634da28fb8d9f6175e8` | WASI self-host compiler |
| `build_wasm/zcc2.wasm` | 941,771 | `7c4dd78317a10a1b263f34cea085e797e2cf446990358634da28fb8d9f6175e8` | Stage 2 WASI self-host (`== zcc3.wasm`) |
| `build_wasm/zcc3.wasm` | 941,771 | `7c4dd78317a10a1b263f34cea085e797e2cf446990358634da28fb8d9f6175e8` | Stage 3 WASI self-host (`cmp` exit 0) |
| `build_wasm/lua.wasm` | 255,596 | `460d8285d66f7f918a4df32abe37c9832ab03f8d20e814951351f63acd0442a7` | **Byte-identical** across native `./zcc` and in-WASI `zcc.wasm` |
| `build_wasm/sqlite3.wasm` | 1,076,769 | `7d27eec0203d3956284fa5ae2d829a24ca657d8c9ffe111d47187eb91b74b62f` | Rebuilt & verified dual-host |
| `build_wasm/doom.wasm` | 466,651 | `453302b7a7b252cea1de0ca19fb6eb691a904f3830e83876ecbcb96c1f464324` | Rebuilt & verified (title + E1M1 3D motion) |
| `tools/zcc_wasm_playground.html` | 570,113 | `b4b6b6d6ae5bf8bca728ae219b881e0a49975647535dcf78024b56de5df556fb` | Regenerated zero-server HTML (`HEADLESS_EVAL_ERRORS: 0`) |
| `zcc2.s` / `zcc3.s` | 11,336,823 | `26a5c6409f0c848f29fad99f26bfd92b5f825911ac6bfced8831f650225236c1` | MD5 `be0ee1dafeaeb246e167229b9002ac2c` (`cmp` exit 0) |
| `stage2.bin` / `stage3.bin` | — | `4a288a42665a0484905e9b7585e3c7ad146f06286fb1a3ede1562367d4cc8c52` | Verified by `zcc_quest` Gate 1 |

---

## 3. Verbatim Post-Hardening Gate Execution Outputs

### Gate A — QuickJS WASI Build, V8 Validation & 6-Test Suite (`tests/test_wasm_quickjs.py`)

```text
[BUNDLE] build_wasm/qjs_wasi_bundle.c: 2207499 bytes (5 upstream QuickJS modules)
[BUILD] /mnt/h/__DOWNLOADS/zcc_github_upload/zcc --target=wasm32 -Itests/embedded_quickjs -Isrc -I. build_wasm/qjs_wasi_bundle.c -o build_wasm/qjs.wasm
[WASM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_wasm/qjs.wasm: 1195627 bytes, SHA256=62e925bd4507a9d7d39bedad9130caed9806e59f4e51144f224c3cb8fd017d18
[V8] valid=True compiled=True err=None
[IMPORTS] count=15 non_wasi=[]
[PASS] Standalone build_wasm/qjs.wasm validated with zero env.* imports!
test_01_v8_validation_zero_env_and_clean_upstream (__main__.TestWasmQuickJS.test_01_v8_validation_zero_env_and_clean_upstream) ... ok
test_02_es2023_differential_vs_node_v8_dual_host (__main__.TestWasmQuickJS.test_02_es2023_differential_vs_node_v8_dual_host) ... ok
test_03_wasi_vfs_script_and_file_io_dual_host (__main__.TestWasmQuickJS.test_03_wasi_vfs_script_and_file_io_dual_host) ... ok
test_04_playground_external_runner_runs_qjs_wasm (__main__.TestWasmQuickJS.test_04_playground_external_runner_runs_qjs_wasm) ... ok
test_05_ef0_negative_controls (__main__.TestWasmQuickJS.test_05_ef0_negative_controls) ... ok
test_06_es2023_adversarial_stress_vs_node_v8 (__main__.TestWasmQuickJS.test_06_es2023_adversarial_stress_vs_node_v8) ... ok

----------------------------------------------------------------------
Ran 6 tests in 2.046s

OK
```

### Gate 2 — WASM Self-Host & Byte-Identical In-WASI QuickJS Self-Compile Probe

```text
WASMTIME_STATUS: NOT_INSTALLED_ON_HOST
test_01_wasi_runtime_primitives_dual_host (__main__.TestWasmWasiSelfhost.test_01_wasi_runtime_primitives_dual_host) ... ok
test_02_wasi_selfhost_convergence (__main__.TestWasmWasiSelfhost.test_02_wasi_selfhost_convergence) ... ok
test_03_browser_wasi_shim_compile_wasm_and_asm (__main__.TestWasmWasiSelfhost.test_03_browser_wasi_shim_compile_wasm_and_asm) ... ok
test_04_ef0_negative_controls (__main__.TestWasmWasiSelfhost.test_04_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 4 tests in 26.027s

OK
[TEST_02 PASS] WASI Self-Host Converged:
  zcc.wasm : 941771B SHA256=7c4dd78317a10a1b263f34cea085e797e2cf446990358634da28fb8d9f6175e8
  zcc2.wasm: 941771B SHA256=7c4dd78317a10a1b263f34cea085e797e2cf446990358634da28fb8d9f6175e8
  zcc3.wasm: 941771B SHA256=7c4dd78317a10a1b263f34cea085e797e2cf446990358634da28fb8d9f6175e8
  cmp zcc2.wasm zcc3.wasm: EXIT 0
CMP_ZCC2_ZCC3_WASM=0
[PROBE-WASM] Invoking build_wasm/zcc.wasm on build_wasm/qjs_wasi_bundle.c...
[PROBE-WASM] rc=0 bytes=1195627 valid=True compiled=True non_wasi=[] exec_ok=True SHA256=62e925bd4507a9d7d39bedad9130caed9806e59f4e51144f224c3cb8fd017d18 byte_identical=True
```

### Synchronized Artifact Rebuilds (`lua.wasm`, `sqlite3.wasm`, `doom.wasm`, Playground)

```text
[PARITY] native=./zcc SHA256=460d8285d66f7f918a4df32abe37c9832ab03f8d20e814951351f63acd0442a7
[PARITY] in-WASI=zcc.wasm SHA256=460d8285d66f7f918a4df32abe37c9832ab03f8d20e814951351f63acd0442a7
[PASS] Byte-identical Lua 5.4.6 WASI module produced by BOTH ./zcc and build_wasm/zcc.wasm!
[WASM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_wasm/lua.wasm: 255596 bytes
[WASM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_wasm/sqlite3.wasm: 1076769 bytes
Ran 5 tests in 2.424s — OK (`tests/test_wasm_lua_sqlite.py`)

[WASM] /mnt/h/__DOWNLOADS/zcc_github_upload/build_wasm/doom.wasm: 466651 bytes
Ran 4 tests in 2.578s — OK (`tests/test_wasm_doom.py`)

[PLAYGROUND] Built zero-server HTML: /mnt/h/__DOWNLOADS/zcc_github_upload/tools/zcc_wasm_playground.html (570113 bytes)
[PLAYGROUND EF-7 PASS] {"HEADLESS_EVAL_ERRORS":0,"zccBytes":941771,"luaBytes":255596,"bareStdout":"ZCC Bare WASM Ready!\n55","stdioStdout":"ReadBack: WASI_VFS_LINE=42\nargc=2 argv0=vfs_demo.wasm","asmBytes":1707,"luaOut":"LUA_PLAYGROUND\t610\t42","sqliteOut":"zcc|42\npg|99","rayColors":460,"plasmaColors":128,"doomE1M1Colors":133,"doomHashes":9,"negRc":1}
Ran 3 tests in 2.121s — OK (`tests/test_wasm_playground.py`)
```

### Gate 1 & Gate 5 — x86-64 Self-Host & Cross-Target Regression Sweep

```text
✔    ✨ SELFHOST-MV1.3b PASS
zcc_quest: child exit=0 wrapper_exit=0 oracle_verified=True warnings=36 errors=0 elapsed=124.1s
zcc_quest_sha256: stage2.s=26a5c6409f0c848f29fad99f26bfd92b5f825911ac6bfced8831f650225236c1 stage3.s=26a5c6409f0c848f29fad99f26bfd92b5f825911ac6bfced8831f650225236c1
zcc_quest_sha256: stage2.bin=4a288a42665a0484905e9b7585e3c7ad146f06286fb1a3ede1562367d4cc8c52 stage3.bin=4a288a42665a0484905e9b7585e3c7ad146f06286fb1a3ede1562367d4cc8c52
BOOTSTRAP HASH: be0ee1dafeaeb246e167229b9002ac2c  commit=bc318842  elisions=2213
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
```

- `tests/test_wasm_wasi.py`: `Ran 12 tests in 1.364s — OK`
- `tests/test_wasm_emitter.py`: `Ran 2 tests in 0.343s — OK`
- `make selfhost-sovereign`: `[PASS] selfhost-sovereign: byte-identical zero-binutils object & ELF emission verified.`
- `tools/win64_selfhost_sovereign.py`: `[PASS] WIN64 SOVEREIGN ZERO-TOOLCHAIN SELF-HOST VERIFIED` (`stage1==stage2==stage3`, `2815488` B, SHA-256 `ccc61e687a4e9688ca0ac903f87a53491ef6503d8cef188a91f44e3b0faf7d65`, `FC: no differences encountered`, Probes 1–3 + EF-0 Negatives 1–2 PASS)
- `tests/test_arm64_riscv64_direct_elf.py`: `Ran 5 tests in 5.548s — OK` (`[QUAD-ELF-INFO] qemu-aarch64-static=NOT_INSTALLED_ON_HOST qemu-riscv64-static=NOT_INSTALLED_ON_HOST`)

---

## 4. EF-0 Negative Controls & EF-7 Adjudications

| Control | Proposition Tested | Observed Failure/Rejection | Verdict |
| --- | --- | --- | --- |
| `NC-1` | Uncaught JS `ReferenceError` exits non-zero cleanly | `returncode == 1`, `ReferenceError` + `boom` stack printed to `stderr` | PASS (`OBSERVED`) |
| `NC-2` | Differential comparison detects value mismatch | Node V8 `2+2` (`4`) != `qjs.wasm` `2+3` (`5`) | PASS (`OBSERVED`) |
| `NC-3` | Missing script path fails closed | `exitCode == 1`, `"cannot open script 'does_not_exist.js'"` on `stderr` | PASS (`OBSERVED`) |
| `NC-4` | Corrupted WASM magic (`\x00BAD`) fails V8 validation | `WebAssembly.validate() == false` | PASS (`OBSERVED`) |
| `NC-5` | Missing `-e` argument & unknown flag fail closed | `returncode == 2`, explicit diagnostic on `stderr` | PASS (`OBSERVED`) |

- **Claim -> Evidence Class -> Evidence Tier -> Evidence -> Scope -> Limitations -> Verdict:**
  - Claim: Standalone `build_wasm/qjs.wasm` compiled cleanly from unmodified upstream QuickJS 2024-01-13 by BOTH native `./zcc` and in-WASI `build_wasm/zcc.wasm` (byte-identical SHA-256 `62e925bd4507a9d7d39bedad9130caed9806e59f4e51144f224c3cb8fd017d18`) executes ES2023 + adversarial IEEE-754 parity with Node `v22.23.5` V8 across `node:wasi`, `BrowserWasiShim`, and `tools/zcc_wasm_playground.html`, with all Gate 1–5 compiler suites green.
  - Evidence Class: `OBSERVED`
  - Evidence Tier: `E2`
  - Scope: `tests/test_wasm_quickjs.py` (`6/6`), `tests/test_wasm_wasi_selfhost.py` (`4/4`), synchronized Lua/SQLite/DOOM/Playground suites, Gate 1 (`make selfhost`), and Gate 5 (`selfhost-sovereign`, `win64_selfhost_sovereign.py`, `test_arm64_riscv64_direct_elf.py`, `test_wasm_wasi.py`, `test_wasm_emitter.py`).
  - Limitations: `wasmtime`, `qemu-aarch64-static`, and `qemu-riscv64-static` are not installed on host (`NOT_INSTALLED_ON_HOST`).
  - Verdict: **PASS**
