# Gate Evidence: Track 4 — Sovereign Zero-Toolchain Win64 PE32+ Lua 5.4.6 (`build_win64/lua.exe`) & SQLite 3.45.0 (`build_win64/sqlite3.exe`)

**Ticket / Deliverable:** Track 4 — Compile unmodified upstream Lua 5.4.6 (`tests/embedded_lua/*.c`) and unmodified upstream SQLite 3.45.0 (`src/sqlite3.c`) into standalone native Windows PE32+ console executables using ZCC directly (`-target win64 -mabi=ms --reproducible`) with **zero MSVC, zero MinGW, zero GCC, and zero binutils**, achieving bit-identical parity between host Linux `./zcc` and native Windows sovereign `build_win64/zcc_sov_stage3.exe`.

**Governing Rule Version:** `ZCC-EF-7 v1.0.0` (and `ZCC-EF-0`)  
**Evidence Tier Assigned:** `E2` (`evidence_tier_assigned_at`: `2026-11-20T04:32:59-08:00` / `2026-11-20T12:32:59Z`, predating final verification runs)  
**Host Environment:** Windows 11 native PE32+ execution + WSL2 Linux `6.18.58.1-microsoft-standard-WSL2 x86_64`, `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, Node `v22.23.5` (`/home/ZKAEDI/.local/bin/node`)  
**Host Runtime Availability:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` | `QEMU_STATUS: NOT_INSTALLED_ON_HOST`  
**Base Commit (`HEAD`):** `f01651e4cfb59227a06099a4bc1b11531235fc8c`

---

## 1. Summary of Changes & Upstream Immutability

- **Upstream Immutability Verified (`OBSERVED`):** `git diff -- tests/embedded_lua/ src/sqlite3.c` is `0` lines.
- **`src/win64_pe_emit.c`:** Added 35 `C:\Windows\System32\msvcrt.dll`-verified C99 symbols (`abs`, `labs`, `isprint`, `iscntrl`, `isgraph`, `ispunct`, `strcoll`, `strpbrk`, `strspn`, `strcspn`, `localtime`, `_localtime64`, `gmtime`, `_gmtime64`, `_time64`, `difftime`, `mktime`, `strftime`, `localeconv`, `setlocale`, `ungetc`, `setvbuf`, `rename`, `tmpfile`, `modf`, `sinh`, `cosh`, `tanh`, `signal`, `raise`, `_access`, `_unlink`, `_setjmp`, `setjmp`, `longjmp`) to `zcc_win64_pe_resolve_dll_for_symbol`.
- **`src/codegen.c`:**
  - Mapped `__errno_location -> _errno`, `access -> _access`, and `unlink -> _unlink` in `win64_remap_msvcrt_sym`.
  - Fixed `.quad sym + addend` / `.quad sym - addend` parsing in the internal assembler (lines `3045–3075`) and made `add_reloc` (`187–215`) whitespace-tolerant around `+`/`-` so pointer-offset globals such as `sqlite3aLTb`/`sqlite3aEQb`/`sqlite3aGTb` (`&sqlite3UpperToLower[204..216]`) preserve their exact addends rather than silently zeroing `r.addend`.
- **`part4.c` (6 lines, lines `5830–5835`):** In `compute_liveness`, disqualified `ND_COMPOUND_ASSIGN` LHS local symbols from register allocation whenever `ct != lt` (matching `ND_COMPOUND_ASSIGN`'s `ct == lt` fast-path guard landed in `f13148fcd`).
- **`src/win64_app_crt.c` & `src/win64_setjmp.s`:** Standalone `msvcrt.dll` Win64 console CRT (`__iob_func()` `stdin`/`stdout`/`stderr`, `__getmainargs()` `mainCRTStartup`, `strtoll`/`strtoull`/`atoll`/`getpid`/`getcwd`/`__errno_location`) plus Microsoft x64 ABI `setjmp`/`_setjmp` and `longjmp`/`_longjmp` (saving/restoring callee-saved registers within `jmp_buf[25]`).
- **`src/win64_sqlite_driver.c`:** Standalone `msvcrt.dll`-backed SQLite VFS (`"zcc_win64"`) and CLI driver (`:memory:` / on-disk DB, inline SQL, `-f`, `stdin`).
- **`tools/build_lua_win64.py` & `tools/build_sqlite_win64.py`:** Reproducible dual-compiler builders (`./zcc` and `build_win64/zcc_sov_stage3.exe`), `fc /b` + SHA-256 identity enforcement, and PE32+ + System32 `msvcrt.dll` export validation.
- **`tests/test_win64_lua_sqlite.py`:** 6-test end-to-end Win64 Lua 5.4.6 + SQLite 3.45.0 native execution, differential (`lua.wasm` / CPython `sqlite3`), persistent `.db` interop, and ZCC-EF-0 negative control suite.
- **`.gitignore`, `Makefile`, `BOOTSTRAP_BASELINES.tsv`:** Ignored `build_win64/`, added `build-lua-win64`, `build-sqlite-win64`, and `test-win64-lua-sqlite` targets, and locked the updated `zcc2.s` MD5 (`11ad5e09945d64f90e1608308bb0ddfa`) across WSL2 and Azure rows per `E-LEARN-017`.

---

## 2. Synchronized Artifact Inventory & Digests (OBSERVED)

| Artifact | Size (bytes) | SHA-256 | Notes |
| --- | ---: | --- | --- |
| `build_win64/zcc_sov_stage1.exe` | 2,852,352 | `b1b3244f061442bbb9daa63d4c4849829977d847c348f833fe8b7f5dbab463d3` | Sovereign Win64 Stage 1 (`Imports=msvcrt.dll(64)`) |
| `build_win64/zcc_sov_stage2.exe` | 2,852,352 | `b1b3244f061442bbb9daa63d4c4849829977d847c348f833fe8b7f5dbab463d3` | Stage 2 (`== stage1`) |
| `build_win64/zcc_sov_stage3.exe` | 2,852,352 | `b1b3244f061442bbb9daa63d4c4849829977d847c348f833fe8b7f5dbab463d3` | Stage 3 (`fc /b` no differences vs Stage 2) |
| `build_win64/lua.exe` | 475,648 | `ebfa7cfdd8cee2e10a9bb9116d32965597ee81f649cb0a0e02f34807980945aa` | Built by `zcc_sov_stage3.exe`; `Imports=msvcrt.dll(52)` |
| `build_win64/lua_host_zcc.exe` | 475,648 | `ebfa7cfdd8cee2e10a9bb9116d32965597ee81f649cb0a0e02f34807980945aa` | Built by `./zcc`; `fc /b` identical to `lua.exe` |
| `build_win64/sqlite3.exe` | 2,207,744 | `eeb7a3111db2cd5cfe83823625de39007c351d24c3c140e9d03191819cddc468` | Built by `zcc_sov_stage3.exe`; `Imports=msvcrt.dll(37)` |
| `build_win64/sqlite3_host_zcc.exe` | 2,207,744 | `eeb7a3111db2cd5cfe83823625de39007c351d24c3c140e9d03191819cddc468` | Built by `./zcc`; `fc /b` identical to `sqlite3.exe` |
| `zcc2.s` / `zcc3.s` | 11,507,291 | `8ca3a9afac96fe3fd722c831d9df3310aee0d7b1007c8e39edcc69c9e806f144` | MD5 `11ad5e09945d64f90e1608308bb0ddfa` (`cmp -l` 0 diffs) |
| `build_wasm/zcc.wasm` (`zcc2`, `zcc3`) | 952,747 | `fe82436b212a688ce83990f22abceef6c7d2977cdce0ee535c80445ab1cd1849` | WASI self-host (`cmp zcc2.wasm zcc3.wasm` exit 0) |

---

## 3. Forensic Bugs Caught & Fixed Mid-Gate

1. **`src/codegen.c` `.quad sym + addend` truncation (`SQLITE_CORRUPT` on `CREATE TABLE`):**
   - `sqlite3.c:22651-22653` defines `sqlite3aLTb`, `sqlite3aEQb`, `sqlite3aGTb` as `&sqlite3UpperToLower[256 - OP_Ne]` (+204/+210/+216), emitted as `.quad sqlite3UpperToLower + 204`.
   - Previously, `src/codegen.c:3038` read only `sscanf(arg, "%127s", target)` with `r.addend = 0`, silently dropping `+ 204/+210/+216`. SQLite comparison opcodes (`OP_Ne`/`OP_Eq`) then mis-indexed `sqlite3UpperToLower`, causing `WHERE 1=1` to return 0 rows and `OP_ParseSchema` to fail with `SQLITE_CORRUPT`.
   - Fixed `.quad` addend scanning and `add_reloc` whitespace handling; validated on Linux inter-op (`g_ptr_off = &g_tbl[5]`) and SQLite.
2. **`part4.c:5829` register allocator vs `f13148fcd` `ct == lt` fast-path (`0xC0000374 STATUS_HEAP_CORRUPTION` on `CREATE TRIGGER ... NEW.cat`):**
   - Commit `f13148fcd` added `&& ct == lt` to `ND_COMPOUND_ASSIGN`'s register-allocated fast-path (`part4.c:1427`), routing mixed-type compound assignments to the stack-address path (`part4.c:1572`), without disqualifying `n->lhs->sym` in `compute_liveness` (`part4.c:5827`).
   - In `dupedExprNodeSize` (`src/sqlite3.c:109848`), `int nByte` was assigned to `%r12`, while `nByte += sqlite3Strlen30NN(p->u.zToken) + 1` (`size_t` RHS -> `ct != lt`) wrote to an uninitialized stack slot (`-24(%rbp)`), leaving `%r12` short by `nToken` and triggering heap overflow in `exprDup(EXPRDUP_REDUCE)` on both Windows (`0xC0000374`) and Linux glibc (`malloc(): unaligned tcache chunk detected`).
   - Disqualified `ct != lt` `ND_COMPOUND_ASSIGN` LHS symbols in `compute_liveness` (`part4.c:5830-5835`), fixing both Linux and Windows execution cleanly while keeping `tests/compiler/track3_compound_assign_c99_regressions.c` at `11/11 PASS`.

---

## 4. Verbatim Gate Execution Outputs

### Gate 1 — Self-Host Byte-Identical (`make selfhost`) & Baseline Lock (`log_bootstrap_hash.sh --check`)

```text
 ✔    ✨ SELFHOST-MV1.3b PASS
 │    Bootstrap fixed-point, semantic-canary, and full SystemV ABI constraints satisfied.
 ★ ZKAEDI PRIME FIXED POINT REPO - H0 CONVERGED - exit 0 (scars: 36, ⟐ BYTE-IDENTICAL SEAL ⟐) ★
zcc_quest_sha256: stage2.s=8ca3a9afac96fe3fd722c831d9df3310aee0d7b1007c8e39edcc69c9e806f144 stage3.s=8ca3a9afac96fe3fd722c831d9df3310aee0d7b1007c8e39edcc69c9e806f144
zcc_quest_sha256: stage2.bin=845a651af9bc7f7f6fddaaf3b4ffa39724b48451b3082dc3ac2994e66077d330 stage3.bin=845a651af9bc7f7f6fddaaf3b4ffa39724b48451b3082dc3ac2994e66077d330
CMP_EXIT=0
11ad5e09945d64f90e1608308bb0ddfa  zcc2.s
11ad5e09945d64f90e1608308bb0ddfa  zcc3.s
8ca3a9afac96fe3fd722c831d9df3310aee0d7b1007c8e39edcc69c9e806f144  zcc2.s
8ca3a9afac96fe3fd722c831d9df3310aee0d7b1007c8e39edcc69c9e806f144  zcc3.s

BOOTSTRAP HASH: 11ad5e09945d64f90e1608308bb0ddfa  commit=f01651e4  elisions=2213
TOOLCHAIN: gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0 | Linux 6.18.58.1-microsoft-standard-WSL2 x86_64
BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)
```

### Gate 2 — Linux x86-64 Inter-Op (Both Directions) & Track 3c C99 Regressions

```text
DIR_A_OUT: PAIR 344 12.00 MC 23 PTR 60 RC: 0
DIR_B_OUT: PAIR 344 12.00 MC 23 PTR 60 RC: 0
TRACK3_COMPOUND_ASSIGN_C99_REGRESSIONS: 11/11 PASS
T3C_EXIT=0
```

### Gate 4 — Sovereign Win64 PE32+ Lua 5.4.6 & SQLite 3.45.0 Builds & Suite (`test_win64_lua_sqlite.py`)

```text
[HOST-PE] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/lua_host_zcc.exe: 475648 bytes, SHA256=ebfa7cfdd8cee2e10a9bb9116d32965597ee81f649cb0a0e02f34807980945aa
[WIN64-PE] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/lua.exe: 475648 bytes, SHA256=ebfa7cfdd8cee2e10a9bb9116d32965597ee81f649cb0a0e02f34807980945aa
[FC /B] Comparing files BUILD_WIN64\lua_host_zcc.exe and BUILD_WIN64\LUA.EXE
FC: no differences encountered
[AUDIT] PE32+ OK: bytes=475648 .pdata=10200B .reloc=284B StackReserve=0x1000000 Imports=msvcrt.dll(52)

[HOST-PE] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/sqlite3_host_zcc.exe: 2207744 bytes, SHA256=eeb7a3111db2cd5cfe83823625de39007c351d24c3c140e9d03191819cddc468
[WIN64-PE] /mnt/h/__DOWNLOADS/zcc_github_upload/build_win64/sqlite3.exe: 2,207,744 bytes, SHA256=eeb7a3111db2cd5cfe83823625de39007c351d24c3c140e9d03191819cddc468
[FC /B] Comparing files BUILD_WIN64\sqlite3_host_zcc.exe and BUILD_WIN64\SQLITE3.EXE
FC: no differences encountered
[AUDIT] PE32+ OK: bytes=2207744 .pdata=28236B .reloc=1968B StackReserve=0x1000000 Imports=msvcrt.dll(37)

test_01_pe32plus_audit_and_system32_msvcrt_export_check (__main__.TestWin64LuaSqlite.test_01_pe32plus_audit_and_system32_msvcrt_export_check) ... ok
test_02_sovereign_cross_vs_native_byte_identity (__main__.TestWin64LuaSqlite.test_02_sovereign_cross_vs_native_byte_identity) ... ok
test_03_lua54_native_win64_pcall_and_differential_vs_wasm (__main__.TestWin64LuaSqlite.test_03_lua54_native_win64_pcall_and_differential_vs_wasm) ... ok
test_04_sqlite3_native_win64_memory_differential_vs_cpython (__main__.TestWin64LuaSqlite.test_04_sqlite3_native_win64_memory_differential_vs_cpython) ... ok
test_05_sqlite3_native_win64_persistent_db_and_python_interop (__main__.TestWin64LuaSqlite.test_05_sqlite3_native_win64_persistent_db_and_python_interop) ... ok
test_06_ef0_negative_controls (__main__.TestWin64LuaSqlite.test_06_ef0_negative_controls) ... ok

----------------------------------------------------------------------
Ran 6 tests in 2.812s

OK
```

### Gate 5 — Full Prior Regression & Sovereign Sweep

- `python3 tests/test_win64_pe_emitter.py`: `Ran 39 tests in 8.480s — OK (skipped=5)` (`RC=0`)
- `python3 tools/win64_selfhost_sovereign.py`: `stage1==stage2==stage3` (`2852352` B, `SHA256=b1b3244f061442bbb9daa63d4c4849829977d847c348f833fe8b7f5dbab463d3`), `FC: no differences encountered`, Probes 1–3 + EF-0 Negatives 1–2 `PASS` (`RC=0`)
- `make selfhost-sovereign`: `[PASS] selfhost-sovereign: byte-identical zero-binutils object & ELF emission verified.` (`RC=0`)
- `python3 tests/test_wasm_wasi_selfhost.py`: `Ran 4 tests in 26.305s — OK` (`zcc.wasm == zcc2.wasm == zcc3.wasm`, `952747B`, `SHA256=fe82436b212a688ce83990f22abceef6c7d2977cdce0ee535c80445ab1cd1849`, `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST`)
- `python3 tests/test_wasm_setjmp_pcall.py`: `Ran 5 tests in 2.517s — OK`
- `python3 tests/test_wasm_micropython.py`: `Ran 6 tests in 3.282s — OK`
- `python3 tests/test_wasm_lua_sqlite.py`: `Ran 6 tests in 2.521s — OK`
- `python3 tests/test_wasm_quickjs.py`: `Ran 6 tests in 2.347s — OK`
- `python3 tests/test_wasm_playground.py`: `Ran 3 tests in 2.245s — OK` (`HEADLESS_EVAL_ERRORS: 0`)
- `python3 tests/test_arm64_riscv64_direct_elf.py`: `Ran 5 tests in 5.592s — OK` (`[QUAD-ELF-INFO] qemu-aarch64-static=NOT_INSTALLED_ON_HOST qemu-riscv64-static=NOT_INSTALLED_ON_HOST`)

---

## 5. ZCC-EF-0 Negative Controls & ZCC-EF-7 v1.0.0 Adjudication

| Control | Proposition Tested | Observed Failure / Rejection | Evidence Class | Verdict |
| --- | --- | --- | --- | --- |
| Track 4 `NC-1` | Lua syntax error fails closed | `returncode == 2`, `"LUA_SYNTAX_ERROR"` on stderr | `OBSERVED` | **PASS** |
| Track 4 `NC-2` | Uncaught Lua `error()` propagates failure | `returncode == 3`, `"LUA_RUNTIME_ERROR"` + `"ef0_lua_boom"` on stderr | `OBSERVED` | **PASS** |
| Track 4 `NC-3` | Missing `.lua` file fails closed | `returncode == 1`, `"LUA_ERROR"` on stderr | `OBSERVED` | **PASS** |
| Track 4 `NC-4` | Invalid SQL fails closed | `returncode == 1`, `"SQLITE_ERROR"` on stderr | `OBSERVED` | **PASS** |
| Track 4 `NC-5` | Missing `-f` SQL script fails closed | `returncode == 1`, `"SQLITE_ERROR"` on stderr | `OBSERVED` | **PASS** |
| Track 4 `NC-6` | 1-byte PE mutation detected by SHA-256 & `fc /b` | SHA-256 mismatch + Windows `fc /b` `returncode != 0` | `OBSERVED` | **PASS** |

- **Adjudication (Track 4 — Sovereign Zero-Toolchain Win64 Lua 5.4.6 & SQLite 3.45.0):**
  - **Claim:** Unmodified upstream Lua 5.4.6 (`build_win64/lua.exe`, 475,648 B) and unmodified upstream SQLite 3.45.0 (`build_win64/sqlite3.exe`, 2,207,744 B) compile with zero MSVC/MinGW/GCC/binutils into byte-identical Windows PE32+ executables across host `./zcc` and native Windows sovereign `build_win64/zcc_sov_stage3.exe` (`fc /b` identical, `msvcrt.dll`-only imports verified against `C:\Windows\System32\msvcrt.dll`), execute natively on Windows 11 with parity against `build_wasm/lua.wasm` and host CPython `sqlite3`, and pass Gate 1–5 regression suites.
  - **Evidence Class:** `OBSERVED`
  - **Evidence Tier:** `E2`
  - **Evidence:** Gate 1 (`cmp zcc2.s zcc3.s` exit 0, `BOOTSTRAP_HASH_EXIT=0`), Gate 2 (bidirectional Linux inter-op `PAIR 344 12.00 MC 23 PTR 60`), Track 3c regression (`11/11 PASS`), `tools/win64_selfhost_sovereign.py`, `tools/build_lua_win64.py`, `tools/build_sqlite_win64.py`, `tests/test_win64_lua_sqlite.py` (`6/6 OK`), and all 9 prior Gate 5 suites listed above.
  - **Scope:** Standalone console PE32+ builds (`-target win64 -mabi=ms --reproducible`, `-DLONGDOUBLE_TYPE=double`, `LUA_32BITS=1`, `"zcc_win64"` `msvcrt.dll` SQLite VFS) executed natively on Windows 11 and verified on WSL2 Ubuntu/Node `v22.23.5`.
  - **Limitations:** `WASMTIME_STATUS: NOT_INSTALLED_ON_HOST` and `QEMU_STATUS: NOT_INSTALLED_ON_HOST`; `tests/test_win64_pe_emitter.py` reported `skipped=5` for optional host tools not installed.
  - **Verdict:** **PASS**
