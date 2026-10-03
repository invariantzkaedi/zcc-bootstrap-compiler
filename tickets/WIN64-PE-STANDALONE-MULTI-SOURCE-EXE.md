# Ticket: Win64 PE Track 5 — Standalone Native Multi-Source .exe Autonomous Emission & Hardening

**Author**: ZCC Architecture & Toolchain Division  
**Date**: 2026-10-02  
**Status**: VERIFIED & HARDENED FOR PRODUCTION CLOSURE  
**Target Architecture**: AMD64 PE32+ (`-target win64` / standalone `.exe`)  
**Components**: `src/codegen.c`, `tests/test_win64_pe_emitter.py`, `BOOTSTRAP_BASELINES.tsv`  

---

## 1. Context & Objectives

In Tracks 1 through 4, ZCC implemented direct PE32+ emission, dynamic import binding for single/multiple DLLs, base relocation tables (`.reloc`), and Structured Exception Handling (SEH) unwind tables (`.pdata`).

Track 5 delivers full autonomy and industrial hardening for standalone Win64 `.exe` emission:
- **Autonomous Multi-Source `.c` Compilation**:
  Synthesizes multi-source inputs into a unified compilation unit with `#line` source provenance. Assembles directly into a standalone PE32+ `.exe` without requiring external linkers (`link.exe`, `lld-link`, or GNU `ld`).
- **Direct Multi-Assembly-to-PE Emission**:
  Supports compiling single or multiple `.s` assembly files directly into a PE32+ executable (`zcc file1.s file2.s -target win64 -o app.exe`).
- **Mixed C + Hand-Written Assembly Compilation**:
  Seamlessly compiles mixed projects containing both C sources and hand-written `.s` assembly kernels (`zcc main.c kernel.s -target win64 -o app.exe`) into a single coherent binary.
- **Windows `.obj` Argument Support**:
  Recognizes Windows `.obj` object files as valid link inputs alongside `.o` and `.a`.
- **Dynamic `.pdata` Extents Allocation**:
  Eliminates the previous fixed 256-function extent buffer; dynamically sizes `win_extents` and `func_offsets` to `label_count + 1`, supporting arbitrary codebase scale with zero function extent truncation.
- **PID-Isolated Concurrency-Safe Temporary Management**:
  Prefixes internal multi-source synthesis files with PID (`.tmp_codegen_multi_win64_<pid>_<base>.c`) and registers them with `atexit(cleanup_temp_files)` for leak-free parallel compilation.
- **Preprocessor Line Directive Path Normalization**:
  Sanitizes Windows path separators in `#line` directives, converting `\` to `/` to prevent escape sequence collisions.
- **Zero-Diff Core Discipline**:
  0 lines modified in `part0_pp.c`, `part3.c`, `part4.c`.

---

## 2. Verification Summary

- **Self-Host Bootstrap**: `cmp zcc2.s zcc3.s` byte-identical (`fa9ecd3469b95b99f9f8d934f56bda96b88e4b6c9046bc4f2ab429c8d5c3f1e1`).
- **Opt-Gauntlet**: 19/19 test suites bit-exact; 2,067 net instructions elided.
- **PE Test Suite**: 19/19 tests passing in `tests/test_win64_pe_emitter.py` on Windows native host.
- **Probe Verifications**:
  - `probe_multi.exe`: Multi-source return value 82 verified.
  - `probe_asm.exe`: Direct assembly return value 82 verified.
  - `probe_dyn_run.exe`: Multi-source with dynamic `msvcrt.dll` `printf` and `return val` verified returning exit code 53 and stdout `DYN_RESULT: 53`.
  - `probe_mixed_run.exe`: Mixed C (`main.c`) + assembly (`asm_mult`) returning exit code 63 (9*7).
  - `probe_m_s_run.exe`: Multi-assembly (`probe_m_s1.s` + `probe_m_s2.s`) returning exit code 77.
  - `probe_300_funcs.exe`: Large-scale `.pdata` unwind records with 301 functions verified exceeding 3,600 bytes and correctly sorted ascending.
