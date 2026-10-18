# Ticket: Upgraded Native C Peephole Optimization Engine (part5.c)

**Subsystem:** Codegen / Peephole Optimization  
**Date:** 2026-10-17  
**Status:** VERIFIED & HARDENED FOR PRODUCTION CLOSURE  
**Components:** `part5.c`, `tests/probe_peephole_upgrade.c`, `BOOTSTRAP_BASELINES.tsv`

---

## 1. Context & Motivation
Following the completion of the 100-case Tri-Compiler Metamorphic Gauntlet, Phase 5 Native C Peephole Optimization (`part5.c:peephole_optimize`) was upgraded with 5 foundational assembly transformations:
1. **Redundant Jump to Next Label Elision**:
   Eliminates `jmp .Ltarget` immediately preceding `.Ltarget:`, allowing control flow to fall through naturally without executing redundant branch opcodes. Intervening `.loc` and `.file` debug directives are transparently skipped during lookahead.
2. **Shift-by-Zero Identity Elision**:
   Eliminates no-op `shlq $0`, `shrq $0`, `shll $0`, `shrl $0` instructions that previously survived code generation.
3. **Dead Move Overwrite Elision**:
   Eliminates dead `movq %r11, %rax` instructions when the subsequent instruction unconditionally defines `%rax` or `%eax` without reading `%rax` (e.g. `leaq ...(%rbp)`, `movslq ...(%rbp)`, `movq ...(%rbp)`, `xorl %eax, %eax`).
4. **32-Bit Zero Register Idiom**:
   Lowers `movl $0, %reg` (5 bytes) into `xorl %reg, %reg` (2 bytes), eliminating execution pipeline latency, breaking register dependency chains, and properly zero-extending into the 64-bit parent register.
5. **Power-of-Two Multiplications to Single-Cycle Shifts**:
   Extends `imulq $16, $32, $64` into single-cycle `shlq $4, $5, $6`.

---

## 2. Pre-Patch Reproduction Probe (Rule EF-3: Probe Before Patch)
Before patch, `tests/probe_peephole_upgrade.c` emitted:
- `shlq $0, %rax` at `.loc 1 464`.
- Redundant `jmp .Lfunc_end_103` immediately before `.Lfunc_end_103:`.
- `movl $0, %eax` before variadic function calls.
- Redundant `jmp .Lfunc_end_104` immediately before `.Lfunc_end_104:`.

After patch:
- All 4 redundant instructions were completely elided.
- Probe executed with exit code 0 (`PEEPHOLE_PROBE_RESULT: 324`).

---

## 3. Scope Audit (Rule EF-5)
```text
part5.c | 40 ++++++++++++++++++++++++++++++++++++++++
1 file changed, 40 insertions(+)
```
Strictly within the `< 50 lines` limit for foundational PARTS files.

---

## 4. Verification Gates (Rule EF-1: Raw Output in Same Turn)

### Gate 1 — Self-Host Identity (Mandatory)
```text
be3f48a24a069361ace8d50ea2bb49303522bce53372ea6a52eee1b4d69a136e  zcc2.s
be3f48a24a069361ace8d50ea2bb49303522bce53372ea6a52eee1b4d69a136e  zcc3.s
CMP EXACT BYTES IDENTICAL
```
**Gate 1 Verdict:** PASS

### Gate 2 — Bidirectional Cross-Toolchain Interop
```text
=== 1. ZCC LIB + GCC MAIN ===
INTEROP_SUCCESS: x=10 y=17 z=300
=== 2. GCC LIB + ZCC MAIN ===
INTEROP_SUCCESS: x=10 y=17 z=300
```
**Gate 2 Verdict:** PASS

### Gate 3 — Optimization Pass Gauntlet (19 Suites)
```text
Suites Evaluated:          19
Bit-Exact Passes:          19/19
Semantic Divergences:      0
Total Assembly (Baseline): 91,730 lines
Total Assembly (Optimized):82,639 lines
Net Instructions Elided:   9,091 lines (9.91% net reduction)
```
**Gate 3 Verdict:** PASS

### Gate 4 — 100-Case Tri-Compiler Metamorphic Gauntlet
```text
Total Cases Executed: 100
Passed Parity:        90
Divergences Caught:   0
Crashes Caught:       0
UB Cases Discarded:   10
Tickets Emitted:      0
Total Elapsed Time:   15.31s (6.5 cases/sec)
FINAL VERDICT:        PASS
```
**Gate 4 Verdict:** PASS

### Gate 5 — Multi-Target Cross-Compilation Sentinel
```text
MULTI-TARGET GAUNTLET VERDICT: PASS
Gate 1:        PASS
Target 1 WASM: PASS (14/14 + 3/3 Negative Controls)
Target 2 WIN64:PASS (33/33 + NC-WIN64-1)
Target 3 ARM64:PASS (3/3 + NC-ARM64-1)
Target 4 RISCV:PASS (5/5 + NC-RISCV-1)
```
**Gate 5 Verdict:** PASS
