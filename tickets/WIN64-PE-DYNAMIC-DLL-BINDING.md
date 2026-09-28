# Ticket: Win64 PE Import Directory & Dynamic DLL Binding (Track 2)

**Author**: ZCC Architecture & Toolchain Division  
**Date**: 2026-09-28  
**Status**: IN_PROGRESS (Verification Gates Running)  
**Target Architecture**: AMD64 PE32+ (`-target win64` / auto `.exe`)  
**Components**: `src/win64_pe_emit.h`, `src/win64_pe_emit.c`, `src/codegen.c`, `tests/test_win64_pe_emitter.py`  

---

## 1. Context & Objectives

In Track 1 (`4881e784b`), ZCC achieved autonomous PE32+ direct `.exe` compilation for standalone self-contained C code (arithmetic, control flow, functions, loops, recursion, and direct integer return codes).

Track 2 extends the autonomous binary cross-compiler to support dynamic linking against Windows runtime dynamic-link libraries (specifically `KERNEL32.dll` and standard Win32 / C-runtime APIs).

### Required Capabilities:
1. **PE32+ Import Directory Emission**:
   - Synthesizes `IMAGE_IMPORT_DESCRIPTOR` array in `.idata` section.
   - Populates `DataDirectory[1]` (Import Directory) pointing to descriptor table.
   - Populates `DataDirectory[12]` (IAT) pointing to the Import Address Table.
2. **Dual-Table Binding Architecture**:
   - Synthesizes Import Address Table (IAT) with 8-byte slots per imported API.
   - Synthesizes Import Lookup Table (ILT / `OriginalFirstThunk`) mirroring the IAT.
   - Synthesizes `IMAGE_IMPORT_BY_NAME` Hint/Name structures (2-byte Hint + ASCII name).
   - Synthesizes ASCII DLL name string (`KERNEL32.dll`).
3. **Automated ABI Shim / Import Thunks**:
   - For every unresolved external symbol called by user C code (`ExitProcess`, `GetStdHandle`, `WriteFile`, `VirtualAlloc`, `VirtualFree`, `Sleep`, etc.), `assemble()` automatically synthesizes a 32-byte Win64 ABI bridge stub in `.text`.
   - Bridges System V AMD64 argument registers (`%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`) to Microsoft x64 calling convention (`%rcx`, `%rdx`, `%r8`, `%r9`, and stack slot 5 at `32(%rsp)`).
   - Allocates 32-byte shadow stack space + 8-byte alignment pad (`subq $40, %rsp`).
   - Dispatches indirect call `call *disp32(%rip)` through the IAT slot.
   - Restores stack and returns cleanly with `%rax` return value intact.
4. **Zero-Diff Core Discipline**:
   - 0 lines modified in `part0_pp.c`, `part3.c`, `part4.c`.
   - Completely modular and self-contained within `src/win64_pe_emit.*` and driver assembler `src/codegen.c`.

---

## 2. Technical Design & Memory Layout

```text
[PE32+ Executable File on Disk]
+---------------------------------------------+
| DOS Header & Stub (0x0000 - 0x007F)         |
| PE Signature "PE\0\0" (0x0080 - 0x0083)     |
| COFF File Header (20 bytes)                 |
| PE32+ Optional Header (240 bytes)           |
|   DataDirectory[1]  -> Import Dir (.idata)  |
|   DataDirectory[12] -> IAT        (.idata)  |
| Section Headers (3 * 40 bytes)              |
|   .text, .idata, .data                      |
+---------------------------------------------+
| .text Section (RVA 0x1000)                  |
|   User Code (main, helpers, etc.)           |
|   32-byte Win64 Import Stubs:               |
|     ExitProcess:   call *__imp_ExitProcess  |
|     GetStdHandle:  call *__imp_GetStdHandle |
|     WriteFile:     call *__imp_WriteFile    |
+---------------------------------------------+
| .idata Section (RVA 0x2000)                 |
|   IAT Table (Import Address Table)          |
|   ILT Table (Import Lookup Table)           |
|   IMAGE_IMPORT_DESCRIPTOR array            |
|   DLL Name String ("KERNEL32.dll\0")        |
|   Hint/Name Entries (IMAGE_IMPORT_BY_NAME)  |
+---------------------------------------------+
| .data Section (RVA 0x3000)                  |
|   Initialized Data / Padding (0x0200 bytes) |
+---------------------------------------------+
```

---

## 3. Verification Strategy

1. **Unit Test Harness (`tests/test_win64_pe_emitter.py`)**:
   - `test_07_win64_pe_import_directory_headers`: Validates `DataDirectory[1]` & `DataDirectory[12]` RVAs and sizes.
   - `test_08_windows_host_dynamic_dll_binding_execution`: Live execution of `GetStdHandle(-11)`, `WriteFile(...)`, and `ExitProcess(42)`.
   - `test_09_multi_api_heap_and_sleep_execution`: Live execution of `VirtualAlloc`, `VirtualFree`, `Sleep`, and return 79.
2. **Compiler Self-Host (Gate 1)**:
   - 3-stage bootstrap `make selfhost` and byte-identical identity `cmp zcc2.s zcc3.s`.
3. **Multi-Suite Optimization Gauntlet (Gate 2)**:
   - `python3 tools/test_zcc_opt_passes.py`: 19/19 test suites bit-exact.
4. **Diff Budget (Gate 3)**:
   - 0 lines touched in `part0_pp.c`, `part3.c`, `part4.c`.
