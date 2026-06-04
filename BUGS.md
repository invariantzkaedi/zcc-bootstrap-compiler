
## CG-IR-011: Callee-Saved Register Mismatch (FIXED - Apr 15, 2026)

**Status**: ✅ FIXED  
**Severity**: CRITICAL (Score 8.2, CWE-682)  
**Fix Date**: April 15, 2026  

### The Bug
AST prologue statically saves registers based on AST allocation. IR backend's linear-scan allocator independently uses callee-saved registers (`rbx, r12-r15`) that were never saved, destroying caller state on return.

### Cascades Severed
- **A**: Memory collision (→ CG-IR-008)
- **B**: Recursive state demolition
- **C**: 16-byte alignment violations (→ CG-IR-015/007)
- **D**: Phantom push hallucinations (→ CG-IR-004)

### Fix (part4.c:L3050)
```c
used_regs = allocate_registers(func);
if (backend_ops) {
    used_regs = 0x1F;  /* Force all 5 callee-saved regs for IR */
}
```

### Verification
- ✅ fib(10) = 55 correct
- ✅ Aggressive reproducer passed
- ✅ Bootstrap stable (zcc2.s == zcc3.s)
- ✅ Graphics experiments: 5/5 passed

## CG-AST-012: Local Multi-dim Array Decay Initialization Smash (DISCOVERED - Apr 23, 2026)

**Status**: ✅ RESOLVED (Fixed via AST CAST Proxy unrolling)
**Severity**: CRITICAL (Out of bounds stack overwrite)
**Discover Date**: April 23, 2026

### The Bug
During local scope initialization of multidimensional arrays (e.g. `int local_matrix[2][2]`), ZCC processes the flattened array via `var + idx` assignment. Because `int[2][2]` has a base type of `int[2]` (8 bytes), the offset mathematical pointer arithmetic advances 8-bytes horizontally per scalar iteration, violently obliterating adjacent execution stack boundaries instead of contiguous 4-byte traversal.

### Resolution Strategy
Fixed surgically in `part3.c` without altering `part4.c` ABI behavior by unrolling dimensions to scalar boundaries and mapping to explicitly emitted `ND_CAST` proxy pointers, ensuring pointer arithmetic correctly maps out exactly `1 x scalar` boundaries rather than dimensional decays.

## CG-SIGFPE-002: Runtime SIGFPE from Variable-Denominator Division in --no-safe-math Programs (OPEN)

**Status**: 🔴 OPEN — Known Limitation  
**Severity**: LOW (only affects `--no-safe-math` Csmith programs with provably-zero variable denominators)  
**Discovered**: May 31, 2026 (session d2100a3e)

### The Pattern
Csmith programs generated with `--no-safe-math` contain raw `/` operators on variables
that are provably zero at compile time (e.g., `int l_7 = 0; ... / l_7`). GCC exploits
integer division by zero as Undefined Behavior and eliminates the entire computation via
dead-code / constant-propagation passes. ZCC, as a non-optimizing compiler, emits `idiv`
or `divl` for all non-constant denominators, triggering SIGFPE at runtime on x86.

### Affected Seeds
Seeds where ZCC crashes with exit code 136 (SIGFPE): 2915565, 5655137, 999611, 674304,
862616, 715931, 9131349, 2746786, 5900524, 5964344, 6030850 (from warfare-harness run,
seed=42, 100 iterations).

### Root Cause
ZCC lacks **interprocedural constant propagation**. A variable initialized to zero and
passed as a function parameter remains opaque to the callee — ZCC cannot prove it is zero
and therefore emits live division. GCC inlines or traces the value interprocedurally.

### Non-Fix Rationale
Adding a runtime zero-check before every `idiv`/`divl`/`divq` would silently suppress
real division-by-zero crashes in production code and is the wrong fix. The proper fix is
a local/interprocedural constant propagation pass, which is deferred as a future milestone.

### Workaround
Use `--safe-math` csmith mode for ZCC CI regression testing. The `csmith_warfare.py`
harness accepts `--csmith-args` to override. For meaningful differential fuzzing against
GCC, run: `python3 scripts/csmith_warfare.py --iterations 100 --csmith-args "--no-bitfields --no-unions --no-volatiles --no-inline-function --no-longlong --no-pointers --no-structs --no-arrays --no-comma-operators --no-math64"` (omitting `--no-safe-math`).

## CG-MISMATCH-1003697: Wrong Checksum in Seed 1003697 (FIXED - May 30, 2026)

**Status**: ✅ FIXED  
**Severity**: HIGH (silent miscompilation — wrong answer without crash)  
**Fix Date**: May 30, 2026 (commit `d52bca27`)

### The Bug
ZCC emitted signed setl/setg for comparisons against unsuffixed hex literals whose value exceeds `INT_MAX` (e.g. `0xA6D0CABD`). C99 standard specifies that unsuffixed hex literals exceeding `INT_MAX` but fitting in `UINT_MAX` are of type `unsigned int`. ZCC stored them as `ty_long` (signed), leading to signed comparisons (e.g. `l_1441 < 0xA6D0CABD` where `l_1441 = -9`). Under unsigned rules, `-9` (as a uint32) is larger than `0xA6D0CABD`, so the comparison should be false. ZCC's signed comparison evaluated it as true, leading to divergence in global `g_792`.

### Fix (part4.c)
Added a large-literal unsigned heuristic in the `uns` flag evaluation for `ND_LT/GT/LE/GE` comparison operators:
```c
uns = (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) ||
      (node->rhs && node->rhs->type && is_unsigned_type(node->rhs->type)) ||
      (node->lhs && node->lhs->kind == ND_NUM && node->lhs->int_val > 2147483647LL && node->lhs->int_val <= 4294967295LL) ||
      (node->rhs && node->rhs->kind == ND_NUM && node->rhs->int_val > 2147483647LL && node->rhs->int_val <= 4294967295LL);
```
This correctly forces unsigned machine instructions (`setb`/`setbe`/`seta`/`setae`) for comparisons involving large unsuffixed hex/octal constant boundaries without destabilizing overall symbol parsing.

### Verification
- ✅ Seed 1003697 checksum converges: GCC = ZCC = `F95B7AD7` (and reduced work output matches `E45D4330`)
- ✅ Bootstrap stable (zcc2.s == zcc3.s)
- ✅ All regression tests passed (36/36)
