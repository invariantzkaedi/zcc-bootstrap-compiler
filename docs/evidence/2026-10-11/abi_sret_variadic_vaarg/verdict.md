# Gate Verification Verdict: ABI sret, Variadic Save Area & va_arg Structs

Date: 2026-10-11
Author: ZCC Antigravity Pairing
Subsystem: SystemV AMD64 ABI Codegen (`part4.c`)

## Verification Summary

| Gate | Description | Command | Status |
|---|---|---|---|
| Gate 1 | Self-Host Identity | `cmp zcc2.s zcc3.s` | **PASS** |
| Gate 2 | Cross-Toolchain Interoperability | `bash scripts/gate2_interop.sh` | **PASS** |
| Gate 3 | Diff Budget & Invariant Audit | `git diff --stat part4.c` | **PASS** |
| Gate 4 | Target Defect Regression Probes | Probe 1, Probe 2, Probe 3 | **PASS** |
| Gate 5 | Evidence Freshness | Full test harness replay | **PASS** |

## Detailed Findings

1. **Bug 1: `sret` Scratch Buffer Overflow**:
   - Fixed dynamic allocation in `part4.c:5854` using `g_max_call_sret_size`. Calls returning memory aggregates $> 224$ bytes no longer overflow into caller stack frames.
   - Probe: `tests/probe_large_sret_overflow.c` PASS (Exit code 0).

2. **Bug 2: Variadic `save_base` Scratch Space Collision**:
   - Relocated variadic register save area to `-stack_size` (`part4.c:6004`), placing it at the bottom of the stack frame without colliding with `cc->abi_scratch_offset`.
   - Probe: `tests/probe_sret_variadic_collision.c` PASS (Exit code 0).

3. **Bug 3: `ND_VA_ARG` Aggregate Pointer Return & Dereference**:
   - Updated `ND_VA_ARG` in `part4.c:3080-3135` to preserve struct addresses in `%rax` for assignment operations (`rep movsb`), advancing `gp_offset` or `overflow_arg_area` by `needed` (16 bytes).
   - Probe: `tests/probe_va_arg_struct16.c` PASS (Exit code 0).

4. **Self-Host Verification**:
   - Byte-identical assembly output between Stage 2 and Stage 3: `b20439d939d3932ad4ad89fbc86d3677`.
   - Exit code 0. Zero warnings, zero errors.
