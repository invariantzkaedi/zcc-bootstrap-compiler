# Artifacts Registry — Track 1 Win64 PE32+ Direct Cross-Compiler

**Date**: 2026-09-28  
**Topic**: Track 1 — Autonomous Windows PE32+ Direct `.exe` Cross-Compiler (`-target win64` / `src/win64_pe_emit.c`)  

## Verified Source Artifacts
- `src/win64_pe_emit.h`: Windows PE32+ emitter header declaring `zcc_emit_win64_pe_file` and `zcc_emit_win64_pe_file_ex`.
- `src/win64_pe_emit.c`: PE32+ COFF binary synthesizer with MS-DOS stub (`MZ`), PE signature (`PE\0\0`), COFF header (`0x8664`), PE32+ Optional Header (`0x20b`), `.text` section with combined code/data execution flags (`0xE0000020`), and dynamic entry point offset resolution (`0x1000 + entry_offset`).
- `src/codegen.c`: CLI target argument parser recognizing `-target win64`, `--target=win64`, `-win64`, `--win64`, `-pe64`, and auto-detection on `-o *.exe`; binary in-memory assembler generating PE32+ executables directly.
- `part5.c`: Integrated `src/win64_pe_emit.c`, `src/arm64_codegen.c`, and `src/riscv_codegen.c` into stage2 link command string, preserving self-host compiler bootstrap chain.
- `tests/test_win64_pe_emitter.py`: Comprehensive 6-test suite covering PE structures, headers, arithmetic, recursion, host execution, and `.exe` auto-detection.
- `BOOTSTRAP_BASELINES.tsv`: Appended verified bootstrap hash `1946222d441f3e038f7bcb2d43fb855c` for WSL2 and Azure runner environments.

## Intermediate & Binary Checksums
| File | Size (Bytes) | MD5 Checksum | Notes |
|---|---|---|---|
| `zcc2.s` | ~45,000,000 | `1946222d441f3e038f7bcb2d43fb855c` | Stage 2 Bootstrap Assembly |
| `zcc3.s` | ~45,000,000 | `1946222d441f3e038f7bcb2d43fb855c` | Stage 3 Bootstrap Assembly (Byte-Identical Fixed Point) |
| `zcc` | ~1,200,000 | N/A | Stripped Native Compiler Executable |
