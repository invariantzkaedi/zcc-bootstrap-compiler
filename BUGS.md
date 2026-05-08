
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
