# ZCC System Autopsy: Register Allocation Non-Determinism

## 1. Goal
Achieve bit-for-bit assembly output parity between `zcc2.s` and `zcc3.s` in the self-hosting compilation pipeline (`make selfhost`).

## 2. Outcome
Bit-for-bit parity achieved. `make selfhost` successfully validates the deterministic generation of identical assembly.

## 3. Root Cause Analysis
The compiler exhibited stage-dependent register assignment drift (`%r15` vs `%rbx`). This resulted in a divergent number of callee-saved registers pushed to the stack during the prologue. A different number of stack pushes led to a cascading shift of 8 bytes (e.g. `-1120(%rbp)` vs `-1128(%rbp)`) for all local variable stack offsets, causing thousands of lines of assembly divergence.

Three distinct sources of non-determinism were identified and patched:

### A. Uninitialized Allocator State (`regalloc.c`)
- **Bug**: `LiveInterval.is_float` was never explicitly initialized to `0`. It was only conditionally set to `1` for floating-point definitions.
- **Impact**: Uninitialized bits led to garbage edges in the interference graph. 
- **Fix**: Initialized `is_float = 0` in `get_or_create()`.

### B. Floating-Point Spill Heuristics (`regalloc.c`)
- **Bug**: The `chaitin_briggs` metric used `double` division (`cost = num / den`). Floating point evaluation (e.g., x87 80-bit vs SSE2 64-bit precision) can yield sub-ULP discrepancies across architectures or optimization levels.
- **Impact**: Subtle variations in the spill metric could re-order the spill priority stack.
- **Fix**: Replaced floating-point division with integer cross-multiplication (`num * max_den > max_num * den`), completely eliminating `double` from the selection logic.

### C. Insufficient Total-Order Tie-Breakers (`compiler_passes.c`, `part4.c`)
- **Bug**: The `live_interval_compare` sorting routine lacked tie-breakers. Similarly, the `allocate_registers` algorithm in `part4.c` utilized an insertion sort based strictly on `live_start`.
- **Impact**: When multiple variables became live simultaneously (which happens frequently in topologically parsed AST nodes), the sort order was undefined or depended on array insertion order. This caused identical `live_start` temps to randomly swap `%r15` and `%rbx`.
- **Fix**: Implemented strict, string-based tie-breakers using `strcmp(name)` in both `regalloc.c` and `part4.c`'s `allocate_registers()`. This guarantees canonical traversal order across all platforms.

## 4. Gate 1 Verification (`cmp zcc2.s zcc3.s`)

```
$ make selfhost
=== Stage 1: zcc compiles itself -> zcc2 ===
...
=== Stage 2: zcc2 compiles itself -> zcc3 ===
...
diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)
SELF-HOST VERIFIED (assembly identical)
```

## 5. Status
**BASELINE: GREEN**
The `zcc` register allocator is now completely deterministic across multiple stages of self-hosting.
