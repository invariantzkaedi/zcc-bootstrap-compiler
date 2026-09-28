# Ticket: Win64 PE Multi-DLL Import Directory & Dynamic Binding (Track 3)

**Author**: ZCC Architecture & Toolchain Division  
**Date**: 2026-09-28  
**Status**: VERIFIED & READY FOR CLOSURE  
**Target Architecture**: AMD64 PE32+ (`-target win64` / auto `.exe`)  
**Components**: `src/win64_pe_emit.h`, `src/win64_pe_emit.c`, `src/codegen.c`, `tests/test_win64_pe_emitter.py`, `BOOTSTRAP_BASELINES.tsv`  

---

## 1. Context & Objectives

In Track 2 (`bcd03ee1b`), ZCC implemented the PE32+ dynamic DLL import architecture for single-DLL bindings (`KERNEL32.dll`), including automated 32-byte Win64 ABI bridge stubs and live host execution on Windows.

Track 3 expands the direct binary PE emitter to support **multi-DLL dynamic import binding**:
- Automatic symbol routing across DLL boundaries (`KERNEL32.dll`, `msvcrt.dll`, `USER32.dll`, `GDI32.dll`).
- Synthesizing an arbitrary number $D$ of `IMAGE_IMPORT_DESCRIPTOR` structures in `.idata` (one per referenced DLL + null terminator).
- Contiguous multi-slice Import Address Table (IAT) and Import Lookup Table (ILT) layouts, with per-DLL null terminators (`0ULL`).
- Verified live execution of mixed multi-DLL C programs on the Windows host (e.g. C standard library functions `malloc`, `free`, `printf`, `puts`, `strlen`, `strcpy`, `strcmp`, `memcpy` from `msvcrt.dll`, UI metrics `GetDesktopWindow`, `GetSystemMetrics` from `USER32.dll`, and process control `ExitProcess`, `Sleep` from `KERNEL32.dll`).

### Required Capabilities:
1. **Dynamic Multi-DLL Import Directory Emission**:
   - Synthesizes an array of `IMAGE_IMPORT_DESCRIPTOR` structures in `.idata` sized $(D + 1) \times 20$ bytes.
   - Sets `DataDirectory[1].Size` to $(D + 1) \times 20$ bytes.
   - Sets `DataDirectory[12].Size` to the cumulative size of all IAT slices (each slice sized $(N_d + 1) \times 8$ bytes).
2. **Autonomous Symbol Classification**:
   - `zcc_win64_pe_resolve_dll_for_symbol(sym)` routes symbols deterministically:
     - `USER32.dll`: `MessageBox*`, `GetMessage*`, `CreateWindow*`, `ShowWindow*`, `GetDesktopWindow`, `GetSystemMetrics`, `MessageBeep`, etc.
     - `GDI32.dll`: `CreateSolidBrush`, `SelectObject`, `DeleteObject`, `TextOut`, etc.
     - `msvcrt.dll`: `printf`, `fprintf`, `sprintf`, `snprintf`, `puts`, `malloc`, `calloc`, `realloc`, `free`, `exit`, `strlen`, `strcmp`, `strcpy`, `memcpy`, `memset`, math APIs (`sin`, `cos`, `sqrt`, `pow`, etc.).
     - `KERNEL32.dll`: `ExitProcess`, `GetStdHandle`, `WriteFile`, `VirtualAlloc`, `VirtualFree`, `Sleep`, and all fallback system services.
3. **Dual-Table Multi-DLL Layout**:
   - For each DLL group $g \in [0, D-1]$:
     - `OriginalFirstThunk` points to ILT slice $g$.
     - `FirstThunk` points to IAT slice $g$.
     - `Name` points to DLL ASCII string $g$.
     - Each slice is cleanly terminated by a 64-bit zero.
4. **Zero-Diff Core Discipline**:
   - 0 lines modified in `part0_pp.c`, `part3.c`, `part4.c`.

---

## 2. Technical Memory Layout

```text
[PE32+ .idata Section Layout with D DLLs]
+-------------------------------------------------------+
| IAT Group 0 (DLL 0): [RVA_HN0, RVA_HN1, ..., 0ULL]    | <- DataDirectory[12] (IAT Base)
| IAT Group 1 (DLL 1): [RVA_HN0, RVA_HN1, ..., 0ULL]    |
| ...                                                   |
| IAT Group D-1      : [RVA_HN0, ..., 0ULL]             |
+-------------------------------------------------------+
| ILT Group 0 (DLL 0): [RVA_HN0, RVA_HN1, ..., 0ULL]    |
| ILT Group 1 (DLL 1): [RVA_HN0, RVA_HN1, ..., 0ULL]    |
| ...                                                   |
| ILT Group D-1      : [RVA_HN0, ..., 0ULL]             |
+-------------------------------------------------------+
| IMAGE_IMPORT_DESCRIPTOR 0 (DLL 0)                     | <- DataDirectory[1] (Import Dir Base)
| IMAGE_IMPORT_DESCRIPTOR 1 (DLL 1)                     |
| ...                                                   |
| IMAGE_IMPORT_DESCRIPTOR D-1 (DLL D-1)                 |
| IMAGE_IMPORT_DESCRIPTOR (Terminating NULL struct)     |
+-------------------------------------------------------+
| ASCII DLL Names (DLL 0\0, DLL 1\0, ...)               |
+-------------------------------------------------------+
| IMAGE_IMPORT_BY_NAME structures (Hint + Name\0)       |
+-------------------------------------------------------+
```

---

## 3. Verification Gates

1. **Gate 1: Self-Host Identity**: `cmp zcc2.s zcc3.s` byte-identical (`1946222d441f3e038f7bcb2d43fb855c`).
2. **Gate 2: Optimization Gauntlet**: 19/19 test suites bit-exact via `python3 tools/test_zcc_opt_passes.py`.
3. **Gate 3: Core Diff Budget**: 0 lines modified in `part0_pp.c`, `part3.c`, `part4.c`.
4. **Gate 4: Target Unit Test Suite**: Extended `tests/test_win64_pe_emitter.py` (11/11 tests pass in 6.045s), verifying simultaneous dynamic calls across `KERNEL32.dll`, `msvcrt.dll`, and `USER32.dll`.
5. **Gate 5: Evidence Freshness**: Preserved under `docs/evidence/2026-09-28/win64_pe_multi_dll/`.
