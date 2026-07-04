# Bug Report: CG-ICP-002 — Wrong Constant Injected at ICP Div-by-Zero Fold Site

**Status**: 🔴 OPEN — new bug, confirmed real mismatch  
**Severity**: CRITICAL (silent wrong answer — ZCC runs, produces different output than reference)  
**Discovered**: 2026-07-04 (warzone campaign, seed 8055910)  
**Related**: CG-SIGFPE-002, CG-SIGFPE-003, commit d992e068

---

## Reproduction

**Seed**: 8055910, `--no-safe-math`

```bash
# GCC -O3 (reference):
gcc -O3 -I/usr/include/csmith seed_8055910_MISMATCH.c -o r_gcc -lm
./r_gcc
# Output: checksum = D8B34779

# GCC -O0 (crashes — division by zero):
gcc -O0 -I/usr/include/csmith seed_8055910_MISMATCH.c -o r_gcc0 -lm
./r_gcc0
# Output: (SIGFPE — no output)

# ZCC (runs but wrong answer):
./zcc -Izcc_sys_includes -I/usr/include/csmith seed_8055910_MISMATCH.c -o r.s
gcc r.s -o r_zcc -lm
./r_zcc
# Output: checksum = DD32DCC1  ← WRONG
```

ZCC emitted this during compilation:
```
seed_8055910_MISMATCH.c:2494: warning: division by zero proven at compile time
(CG-SIGFPE-003): divisor evaluates to 0
```

**So**: ICP correctly detected the div-by-zero at compile time (line 2494), folded it —
but injected the wrong constant value, producing an incorrect checksum.

---

## Root Cause Hypothesis

In `part4.c`, when ICP proves a division denominator is zero:
1. The warning is emitted correctly ✅
2. The division is folded (constant injected) ✅
3. **The injected constant value is wrong** ❌

The ICP fold likely injects 0 (or the LHS unchanged) as the result of `X / 0`,
but the correct behavior for ZCC should be:
- Treat `X / 0` as UB → inject **0** (or keep as undefined/uninitialized)
- The wrong constant propagated downstream causes checksum divergence

---

## Locating the Bug

Search in `part4.c` for the CG-SIGFPE-003 folding code path:
```c
// Around line 5377 (per BUGS.md):
// warning_at(...)  "division by zero proven at compile time"
// Then: what value is returned / stored?
```

Also check `compiler_passes.c` ICP pass — constant propagation may be injecting
the *denominator* value (0) instead of the *result* value (undefined → use 0).

---

## Fix

When ICP folds `X / 0` or `X % 0`:
1. Emit the warning (already done ✅)  
2. Replace the entire expression with constant **0** in the AST/codegen
3. Do NOT propagate the denominator value as the result

```c
// In the div-by-zero fold path:
if (proven_zero_denominator) {
    warning_at(...);
    // FIX: return 0 as the fold result, not some other value
    emit_constant(0LL, result_type);
    return;
}
```

---

## Files to Touch
- `part4.c` — ICP-driven const-fold site for `ND_DIV`/`ND_MOD` (around line 5377)
- `compiler_passes.c` — ICP pass constant injection for division nodes

---

## Gate Required After Fix
- Selfhost byte-identical: `cmp zcc2.s zcc3.s`
- Seed 8055910 must produce same output as GCC -O3: `checksum = D8B34779`
- 200-seed no-safe-math run: mismatch count must drop to 0
