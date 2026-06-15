
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

## CG-ASM-XMM-001: Built-in Assembler Silent Miscompilation of SSE Register Operands (RESOLVED)

**Status**: ✅ RESOLVED (Fixed via XMM & SSE support, memory push/pop, and strict mnemonic verification)  
**Severity**: CRITICAL (Score 8.2, CWE-682 — Silent Data Corruption)  
**Fix Date**: June 15, 2026  

### The Bug
ZCC's built-in assembler (implemented in `src/codegen.c`) contained a register parser (`parse_reg`) that lacked support for `%xmm0`–`%xmm15` registers. When compiling code containing float/double SSE operands with direct object output (`zcc -c file.c -o file.o`), the assembler failed to parse the `%xmm` registers, returning `-1` as the register ID.

Because the binary instruction encoder applied a bitwise mask `reg & 7` to encode register parameters in instructions, `-1 & 7 = 7`, which silently mapped the register reference to register index 7 (corresponding to `%r15`/`%r15d`). 

Additionally, float/double instructions (such as `movss`, `movsd`, `cvtss2sd`, `ucomiss`, `ucomisd`, etc.) were entirely unrecognized by the built-in assembler's parser and were skipped without emitting a compilation error. This led to silent binary generation of corrupted instruction streams.

### Resolution
- **Extended `parse_reg`**: Added full mapping for `%xmm0`–`%xmm15` (returning 16–31) and `%rip` (returning 32).
- **Added SSE instruction and memory encoders**: Implemented encoding for SSE binary and memory operations (`movss`, `movsd`, `cvttss2si`, etc.).
- **Added memory operand pushq/popq**: Enabled `pushq mem` / `popq mem` support inside the built-in assembler.
- **Implemented `movabsq`/`movabs`**: Encoded full 64-bit immediate loads into GP registers.
- **Strict Error Handling**: Added a compilation abort trigger if any parsed register evaluates to `-1` or if the instruction mnemonic is unrecognized, severing silent miscompilation cascades.

### Verification
- ✅ Stage 2/Stage 3 bootstrap remains completely byte-identical (`SELF-HOST VERIFIED`).
- ✅ Golden ABI lane differential fuzzer campaign passes 31/31 test shapes compiling directly to ELF objects (`zcc -c`).

## CG-FRONTEND-ASM-001: Silent Elision of Inline Assembly Statements (OPEN)

**Status**: 🔴 OPEN — Known Limitation  
**Severity**: HIGH (silent code-elision — no diagnostic emitted, statement ignored)  
**Discovered**: June 15, 2026 (session 7d20bba7)

### The Bug
ZCC silently accepts `__asm__ __volatile__`, `asm`, or `__asm__` syntax in source code (presumably parsing it as a statement without returning syntax errors) but completely discards the block during code generation. It emits no compiler warning, diagnostic, or assembly instructions for the inline assembly blocks.

For example, compiling:
```c
int main() {
    asm("nop");
    asm volatile("mov $42, %rax");
    return 0;
}
```
yields:
```assembly
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
    movq $0, %rax
    jmp .Lfunc_end_100
```
This is a critical silent failure mode, whose risk/severity profile depends entirely on the presence of output operands:
- **Asm with output operands (`: "=r"(var)`)**: The compiler silently elides the statement, leaving `var` holding whatever uninitialized/garbage value happens to be in the allocated register. This causes severe, hard-to-diagnose data corruptions downstream (as seen in `read_cr3()` where the returned PML4 base pointer resolved to the end of BSS).
- **Side-effect-only asm with no output operands (`hlt`, `nop`, `cli`/`sti`, memory barriers)**: The instruction simply vanishes. The correctness of subsequent computations is preserved, but CPU power state, execution timing, or interrupt synchronization behavior is altered.

### Affected Code
- `kernel/kmain.c`: The infinite halt loops (`__asm__ __volatile__("hlt");`) at lines 514 and 656 are silently skipped. Since these are side-effect-only statements inside infinite loops, the omission results in a busy-wait loop rather than low-power CPU halting, which is functionally benign.
- `src/zkernel/main.c` / `src/zkernel/uart.c`: Handled similarly under old kernel source structures.

### Non-Fix Rationale / Workaround
Properly supporting GCC-style inline assembly (parsing inline constraint lists, register allocations, clobber lists, and splicing template strings into output assembly) requires a major frontend parsing and register-mapping extension. 
For low-level operations (like `read_cr3` and `invlpg`), the pragmatic workaround is to encapsulate the operations in native `.S` assembly files (e.g. `kernel/boot.S`), export them as functions, and declare them as `extern` in C.



