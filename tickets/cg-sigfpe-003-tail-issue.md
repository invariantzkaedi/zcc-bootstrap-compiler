# Bug Report: CG-SIGFPE-003 (Tail) — Runtime Variable-Denominator SIGFPE

**Status**: ✅ RESOLVED in commit 68914f27 (runtime variable-denominator case resolved via --safe-div)  
**Severity**: HIGH (15.5% crash rate on --no-safe-math Csmith corpus)  
**Discovered**: 2026-07-04 (warzone campaign, 200 seeds)  
**Depends on**: CG-SIGFPE-002 (partial close), ICP codegen feedback (d992e068)

---

## Measured Failure Rate

Campaign: 200 Csmith seeds, `--no-safe-math`, seed=777  
**31 / 200 seeds = 15.5% crash rate (exit -8 = SIGFPE)**

Affected seeds: 7475864, 4527082, 1737824, 1797824, 8407850, 572193, 2479842, 9674050,
9409142, 8346404, 5757338, 9220799, 882690, 895893, 4807218, 8556665, 2656409, 7742959,
1772721, 3342528, 1025600, 7866115, 1166020, 3625545, 4556372, 6497250, 4885686, 206905,
8108931, 6783064, 6623661

---

## Root Cause

ZCC emits `idiv`/`divl`/`divq` for all division/modulo operations where the denominator
is not ICP-proven at compile time. When Csmith generates programs with `--no-safe-math`,
it produces division expressions where the denominator happens to be zero at runtime —
this is legal C (undefined behavior), which GCC -O3 eliminates via UB rules but ZCC
faithfully emits as a machine divide instruction, triggering SIGFPE on x86.

### Pattern (seed 1040492, already reduced):
```c
(int32_t)((uint8_t)0x28L << (uint8_t)0) % (int32_t)(l_48 = (l_61 |= (l_60 = g_55)))
```
`g_55` flows through several assignments and becomes 0 at runtime; ICP cannot prove
this without full alias/mutation tracking. ZCC emits `idiv`, signal 8 fires.

---

## Fix Options

### Option A: Runtime zero-guard (correct for production)
Before every `idiv`/`divl`/`divq` where denominator is NOT ICP-proven nonzero, emit:
```asm
    testq %rcx, %rcx        ; test denominator
    je .Ldivzero_skip_NNN   ; if zero, skip (ZCC treats as 0 result, UB anyway)
    idivq %rcx
    jmp .Ldivzero_end_NNN
.Ldivzero_skip_NNN:
    xorq %rax, %rax         ; result = 0 (UB — any value is valid)
    xorq %rdx, %rdx
.Ldivzero_end_NNN:
```
**Cost**: +3 instructions per division. Selfhost impact: TBD.

### Option B: CI workaround (non-fix)
Run Csmith CI only with `--safe-math`. Already in place.
Leaves M4 milestone incomplete for full parity.

---

## Files to Touch
- `part4.c` — `codegen_expr` binary op emission, `ND_DIV` / `ND_MOD` handlers
- Search: `idivq`, `divl`, `divq` emission sites

---

## Non-Fix Rationale for Option B
Division by zero is UB. GCC -O3 deletes the path. ZCC crashing is "correct" behavior
for UB. The only reason to fix is Csmith M4 parity — where we want ZCC to match GCC -O0
(which also crashes with SIGFPE). Fix is cosmetically correct but adds dead-code guards
around undefined behavior.

**Recommendation**: Implement Option A, guarded by `--safe-div` CLI flag (default off),
enabled automatically when running csmith warfare. This avoids perturbing selfhost.
