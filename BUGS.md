## CG-FLOAT-RETURN-WIDEN-001: Missing System V Float Return Widening (FIXED - Oct 14, 2026)

**Status**: ✅ FIXED  
**Severity**: HIGH (Silent IEEE-754 precision loss / subnormal corruption)  
**Fix Date**: October 14, 2026  
**Commit**: `f3fbe86ba`  

### The Bug
When a C function declared to return `double` returned an expression of type `float` (e.g. `return (float)a;` inside `Math.fround`), `part4.c` (`case ND_RETURN:`) omitted System V AMD64 scalar widening (`cvtss2sd %xmm0, %xmm0`). Instead, it bitcast the 32-bit float into `%xmm0` with `movq %rax, %xmm0`, zero-extending the upper 32 bits into the 64-bit exponent/fraction fields and creating deep subnormals ($10^{-315}$).

### Fix (`part4.c:L5117-5132`)
Emitted `cvtss2sd %xmm0, %xmm0` when returning `TY_FLOAT` from a `TY_DOUBLE` function, and `cvtsd2ss %xmm0, %xmm0` when narrowing. Removed unqualified `"next_token"` from `ir_whitelisted()` per `E-LEARN-016`.

### Verification
- ✅ `tests/probe_float_return_widen.c`: bit-exact `0x4340000000000000`
- ✅ QuickJS ES2020 Engine: 15/15 PASS
- ✅ V8 Differential Gauntlet: 100/100 consensus (100% parity across Ignition, Maglev, TurboFan, and ZCC QuickJS)
- ✅ Bootstrap stable: `cmp stage2.s stage3.s` byte-identical (`8b4b28bc4cfbd9ff867ba66f5931fb68`)

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

**Status**: ✅ PERMANENTLY RESOLVED (Phase 2 Compile-Time ICP-to-Codegen Feedback Loop; zero runtime branch overhead)  
**Severity**: LOW / HIGH (Csmith `--no-safe-math` zero-denominator division handling)  
**Fix Date**: September 8, 2026 (Phase 2 closure)

### Resolution Architecture

1. **Static / ICP Constant Paths (commit `532bb4ae`)**:
   - `part4.c:5377`: Codegen binary-op fold
   - `part3.c:1004`: Parse-time case/array bounds
   - `part4.c:4776`: Global static initializer fold
   - ICP Oracle Substrate (`--icp-closed-world`, `--trace-constprop` flags)

2. **Compile-Time Feedback Loop (`CG-SIGFPE-003`, Phase 2)**:
   - Enhanced `part3.c` Interprocedural Constant Propagation to track:
     - Single-assigned local variables (assign_count <= 1, !addr_taken).
     - Immutable scalar static global variables (assign_count == 0, !addr_taken, is_integer, !volatile).
     - Guarded `solve_icp` and `rewrite_constant_vars_rec` against pointers/strings (`zcc_stddef_text`).
   - Fed proven zero constants into the codegen division path, folding proven zero denominators to 0 and suppressing `idiv`/`div` emission at compile time.
   - Eliminates Csmith `--no-safe-math` SIGFPE 136 crashes with zero runtime instructions/branching overhead.

### Verification
- ✅ Probe 1 (`/tmp/probe_divzero.c`): Local variable zero denominator folded cleanly to 0, zero `idiv` instructions in assembly, exit code 0.
- ✅ Probe 2 (`/tmp/csmith_nochecksum.c` Csmith seed 7475864): Static global zero denominator folded cleanly without `--safe-div`, exit code 0.
- ✅ Bootstrap fixed-point verified (`cmp zcc2.s zcc3.s` byte-identical, SHA-256 `3a95685ee95d7a2067f5b5bc490fdb8044e6dc49aabc5b0da702f1cab5f3903e`).
- ✅ Diff audit: `part3.c` 44 lines changed (< 50 line boundary), `part4.c` 0 lines.

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

## swarm-prove: Gate Cannot Fail (RESOLVED & VERIFIED - Sep 08, 2026)
**Status**: ✅ RESOLVED & RESTORED to release path (`release-v1.0`).
- Synchronized `Makefile` swarm-prove recipe to explicitly track `UNKNOWN` along with `HOLD`, `VIOLATED`, and `ERROR`.
- Upgraded pass condition from vacuous `((PROVED+VIOLATED) == TOTAL)` to strict `PROVED == TOTAL` with `UNKNOWN == 0`, `VIOLATED == 0`, and `ERROR == 0`.
- Added `make swarm-prove-fault-injection` negative control: verifies a known-barrier contract (`tests/evm/fault_injection_barrier.bin` containing `REVERT`) yields `UNKNOWN`, proving the gate produces a red verdict on non-holding contracts.
