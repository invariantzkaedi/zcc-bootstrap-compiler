
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

## CG-SIGFPE-002 / CG-SIGFPE-003: Runtime SIGFPE from Variable-Denominator Division (RESOLVED)

**Status**: ✅ RESOLVED (Static paths closed in `532bb4ae`, runtime variable-denominator paths closed via `ZCC_SAFE_DIV=1` / commit `68914f27`)  
**Severity**: LOW / HIGH (Csmith `--no-safe-math` zero-denominator division handling)  
**Fix Date**: July 4, 2026 (commit `68914f27`)

### Resolution Architecture

1. **Static / ICP Constant Paths (commit `532bb4ae`)**:
   - `part4.c:5377`: Codegen binary-op fold
   - `part3.c:1004`: Parse-time case/array bounds
   - `part4.c:4776`: Global static initializer fold
   - ICP Oracle Substrate (`--icp-closed-world`, `--trace-constprop` flags)

2. **Dynamic / Runtime Denominator Paths (`CG-SIGFPE-003`, commit `68914f27`)**:
   - Activated via `ZCC_SAFE_DIV=1` environment variable or `--safe-div` CLI flag.
   - Emits runtime zero-guard check before `idiv`/`divl`/`divq`:
     ```asm
     testq %rcx, %rcx
     je .Ldivzero_skip_NNN
     idivq %rcx
     ```
   - Eliminates hardware SIGFPE crashes on opaque dynamic zero denominators in Csmith campaigns.

### Verification
- ✅ `ZCC_SAFE_DIV=1 ./zcc tests/regressions/test_safe_div.c` passes cleanly with exit code 0.
- ✅ Without `ZCC_SAFE_DIV`: traps with `SIGFPE` (confirming guard is active only when requested to preserve raw UB diagnostics).
- ✅ Bootstrap stable (`zcc2.s == zcc3.s`).

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
ZCC lacks full **interprocedural constant propagation → codegen feedback**. The ICP
solver can now *prove* x=0 via `--icp-closed-world`, but does not yet feed that proof
back into the division emitter to suppress or guard the `idiv`. The oracle substrate
exists (commit `a62e8f97`); the codegen feedback loop is the next phase.

### Non-Fix Rationale
Adding a runtime zero-check before every `idiv`/`divl`/`divq` would silently suppress
real division-by-zero crashes in production code and is the wrong fix. The proper fix is
feeding ICP-proven constants into the codegen path to elide the division entirely.

### Workaround
Use `--safe-math` csmith mode for ZCC CI regression testing. For differential fuzzing:
`python3 scripts/csmith_warfare.py --iterations 100 --csmith-args "..."` (omitting `--no-safe-math`).

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

## CG-FRONTEND-ASM-001: Silent Elision of Inline Assembly Statements (CLOSED — 11e7e144)

**Status**: ✅ CLOSED — Fixed in commit `11e7e144`  
**Severity**: HIGH (silent code-elision — no diagnostic emitted, statement ignored)  
**Discovered**: June 15, 2026 (session 7d20bba7)  
**Fixed**: June 19, 2026 (commit `11e7e144`)

### Resolution
Two-layer silent elision removed:
1. `part0_pp.c`: Removed `#define __asm__(x)` / `#define asm(a,b,c,d,e)` empty macro erasures from stddef stub — asm strings now reach the parser.
2. `part3.c`: Replaced `return ND_NOP` fallback with full `ND_ASM` node creation + capability tier classification.
3. `part4.c`: Tier-aware emission — Tier 0/1 (zero-operand) emits verbatim; Tier 3 (constraints) warns or errors under `--error-unsupported-asm`.

New CLI flags: `--error-unsupported-asm`, `--asm-report`.

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




## seed9226: Differential Mismatch (CLOSED-UNREPRODUCIBLE - Jul 24, 2026)
**Status**: Witness dead. Retained binaries produce identical deterministic output
as of 2026-07-24. Generating source not retained; divergence cannot be reproduced
or attributed. Likely resolved by CG-IR-011 era fixes. Lesson: witnesses MUST
retain source + seed + regeneration command, not binaries alone.

## swarm-prove: Gate Cannot Fail (QUARANTINED - Jul 24, 2026)
**Status**: Removed from release-prep and v1.0 ship path. Makefile PASS branch
greps for "violat" but src/evm/symbolic.c:90 emits only HOLD/UNKNOWN — the
VIOLATED branch is unreachable dead code; gate cannot produce a red verdict.
Underlying syntactic check (barriers==0 → HOLD) is sound but weak.
**Re-entry criteria**: pass condition changed to PROVED==TOTAL, plus one
known-barrier contract in corpus asserted to yield UNKNOWN (fault-injection
proof the gate can fail).
