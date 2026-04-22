# ZCC — A Self-Hosting C Compiler with an Optimizing IR Backend

ZCC is a self-hosting C compiler targeting x86-64 Linux (System V AMD64 ABI). It compiles a substantial subset of C, including structs, pointers, arrays, function pointers, enums, switch statements, and the full complement of arithmetic, bitwise, logical, and comparison operators. ZCC compiles itself — the bootstrap chain produces byte-identical output across stages.

The compiler has two codegen paths: a direct AST-to-assembly emitter for rapid compilation, and a 3-address SSA IR backend with a full optimization pipeline. Both paths produce correct, self-hosting output.

```
GCC → zcc₁ → zcc₁ compiles itself → zcc₂.s
                                      ↓
                              gcc assembles → zcc₂
                                      ↓
                              zcc₂ compiles itself → zcc₃.s
                                      ↓
                              cmp zcc₂.s zcc₃.s → IDENTICAL ✓
```

## Status (Apr 10 2026)

| Metric | Result |
|--------|--------|
| Self-hosting | ✅ `zcc2.s == zcc3.s` (byte-identical) |
| Regression tests | ✅ 21/21 pass |
| Fuzz suite | ✅ 53/53 pass |
| SQLite 3.45.0 | ✅ Full SQL round-trip verified |
| IR backend tests | ✅ 21/21 pass |
| Peephole elisions (self-compile) | 5,363 |

### DOOM 1.10 (id Software Engine)

ZCC successfully compiles, links, and runs the entire `linuxdoom-1.10` source code (45,000 lines). The engine reliably boots, loads WAD format assets, initializes the core subsystems, and renders 3D frames into an X11 shared memory framebuffer using a 32-bit TrueColor synthesized palette LUT without any runtime segment faults. This stresses and validates the robustness of ZCC's structural alignment, global state management, and memory pointer arithmetic.

### SQLite Round-Trip Verification

ZCC compiles SQLite 3.45.0 (85,000 lines) and produces a working binary:

```
SQLite 3.45.0 compiled by ZCC
open rc=0
1 = 1
SELECT 1 rc=0 err=none
CREATE TABLE rc=0 err=none
INSERT rc=0 err=none
x = 42
SELECT rc=0 err=none
```

All operations return rc=0. No segfaults. The B-tree allocator, LALR(1) parser, memory allocator, and page cache all function correctly under ZCC-compiled code. Bugs fixed to reach this milestone include a full System V AMD64 `va_list` implementation, global struct initializer emission, negative array initializer constants, struct-by-value ABI, octal escape sequences, and `sizeof` on string literal arrays.

### Lua 5.4.6 Compilation (Working)

ZCC compiles Lua 5.4.6 (30,000 lines) to zero errors.
The VM boots, loads all core libraries, and manages numeric state reliably.
Minimum runtime handshake (print + arithmetic + version) verified.

Fixes applied to reach this point:
- Preprocessing script strips GCC #line directives, computed goto tables, _Float128 types
- Multidimensional array stride corrected (ARRAY-001)
- va_arg register ordering corrected (ABI-003)
- Call-site implicit argument coercion for mixed int/float params (ABI-INT-TO-DOUBLE-001)

### Metacompiler Chain

ZCC can compile a working C compiler (`zcc-level4/tools/tinycc.c`) which itself emits correct x86-64 assembly:

```
ZCC → compile tinycc.c → tcc binary
tcc → compile "int main() { return 42; }" → binary
binary: exit code 42 ✓
```

## Quick Start

```bash
# Clone
git clone https://github.com/zkaedii/zcc.git
cd zcc

# Bootstrap: GCC → zcc → zcc₂ → zcc₃ → verify
make selfhost

# Run the regression test suite
bash zcc_battle_phase3.sh

# Compile a program
./zcc2 hello.c -o hello.s
gcc -o hello hello.s
./hello

# Run fuzz suite
python3 fuzz_host.py --seeds seeds --zcc ./zcc2
```

## What ZCC Compiles

**Types:** `char`, `short`, `int`, `long`, `long long` (signed and unsigned), `void`, `enum`, `struct`, `union`, pointers, arrays, function pointers, `_Bool`

**Statements:** `if`/`else`, `while`, `do`/`while`, `for`, `switch`/`case`/`default`, `break`, `continue`, `return`, `goto`, labels, compound statements, expression statements

**Expressions:** full arithmetic (`+`, `-`, `*`, `/`, `%`), bitwise (`&`, `|`, `^`, `~`, `<<`, `>>`), logical (`&&`, `||`, `!`), comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`), assignment (`=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`), increment/decrement (`++`, `--`, pre and post), ternary (`?:`), comma, `sizeof`, casts, address-of (`&`), dereference (`*`), member access (`.`, `->`), array subscript (`[]`), function calls, variadic functions (`va_list`, `va_start`, `va_arg`)

**Declarations:** local variables, global variables, global arrays, string literals, function definitions, forward declarations, `extern`, `static`, `typedef`

**Preprocessor:** ZCC does not include a preprocessor. Use `cpp` or `gcc -E` for preprocessing before feeding source to ZCC.

## Architecture

ZCC is split across multiple source files that are concatenated into a single `zcc.c` for compilation:

```
┌─────────────────────────────────────────────────────────┐
│                    zcc.c (concatenated)                  │
│                                                         │
│  part1.c ─── Types, nodes, symbols, scope, allocator    │
│  part2.c ─── Lexer (tokenizer)                          │
│  part3.c ─── Recursive descent parser                   │
│  ir.h    ─── IR instruction set, data structures        │
│  ir_emit_dispatch.h ─ IR emission macros                │
│  ir_bridge.h ─────── AST ↔ IR translation layer         │
│  part4.c ─── x86-64 codegen, register allocator         │
│  part5.c ─── main(), peephole optimizer, globals        │
│  ir.c    ─── IR module, standalone lowerer              │
│  ir_to_x86.c ─────── Standalone IR-to-x86 lowerer      │
│                                                         │
├─────────────────────────────────────────────────────────┤
│              Linked separately:                         │
│                                                         │
│  compiler_passes.c ── IR optimization pipeline +        │
│                       body emitter (7,317 lines)        │
│  compiler_passes_ir.c ── IR pass helpers (570 lines)    │
└─────────────────────────────────────────────────────────┘
```

### Dual-Emission Codegen

ZCC has two code generation paths, selectable per-function:

**AST-Direct Path** (`codegen_expr` / `codegen_stmt` in `part4.c`)

- Tree-walk code generation directly from the AST
- Emits x86-64 assembly with no intermediate representation
- Includes strength reduction (multiply by 2/3/5/9 → `lea`), signed/unsigned dispatch, and a peephole optimizer
- This is the proven path — self-hosting and SQLite compilation have been verified through this backend

**IR Backend** (`compiler_passes.c`)

- Translates AST → 3-address SSA IR via `ir_bridge.h`
- Runs a full optimization pipeline (see below)
- Emits optimized x86-64 assembly from the IR
- Operates inside the AST's stack frame (`body_only` mode — AST owns prologue/epilogue, IR owns the function body)

The IR backend is gated by `ir_whitelisted()` in `part4.c`. When enabled, `codegen_func()` emits the AST prologue, delegates the body to `zcc_run_passes_emit_body_pgo()`, then emits the AST epilogue. This hybrid design allows incremental migration from AST to IR per-function.

### IR Instruction Set

The IR uses a register-machine model with unlimited virtual registers. Instructions:

**Arithmetic:** `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`
**Bitwise:** `AND`, `OR`, `XOR`, `SHL`, `SHR`, `NOT`
**Comparison:** `EQ`, `NE`, `LT`, `LE`, `GT`, `GE`
**Memory:** `LOAD`, `STORE`, `ALLOCA`, `ADDR`
**Control:** `BR`, `CONDBR`, `RET`, `CALL`, `PHI`
**Other:** `CONST`, `COPY`, `CAST`, `NOP`, `PGO_COUNTER_ADDR`

### Optimization Pipeline

`run_all_passes()` in `compiler_passes.c` executes these passes in order:

1. **Reachability Analysis** — marks unreachable blocks
2. **Parameter Escape Marking** — protects function parameters from Mem2Reg
3. **PGO Instrumentation** — optional counter probe injection
4. **Constant Folding** — evaluates compile-time constant expressions
5. **Strength Reduction** — simplifies expensive operations (multiply → shift)
6. **Copy Propagation** — eliminates redundant register copies
7. **Peephole Optimization** — pattern-matched simplifications at IR level
8. **Redundant Load Elimination** — removes duplicate memory loads
9. **Dead Code Elimination (SSA)** — removes instructions with no live uses
10. **Escape Analysis** — determines which allocations can be stack-promoted
11. **Scalar Promotion (Mem2Reg)** — promotes stack variables to SSA registers
    - Single-block fast path for variables used within one block
    - Multi-block path using dominance frontiers and iterated DF for PHI placement
    - SSA rename via recursive dominator tree walk
12. **PGO Block Reordering** — reorders basic blocks by execution frequency for icache locality

Measured results on ZCC compiling itself: 5,363 peephole instructions elided, 10.8% dead code eliminated across 175+ functions, 11 single-block allocas promoted to registers.

### Register Allocation

The IR backend uses a linear scan register allocator with liveness intervals computed from instruction numbering. Physical registers are assigned from the System V ABI callee-saved set (`rbx`, `r12`-`r15`). Spills go to stack slots below the AST frame via the `slot_base` offset.

### Peephole Optimizer

The AST-direct backend includes a post-emission peephole optimizer (`peephole_optimize` in `part5.c`) that scans the assembly output for redundant instruction patterns:

- Redundant `mov` (same source and destination)
- Push/pop of the same register
- Dead stores overwritten before use
- Redundant loads after stores to the same location
- Strength reduction patterns in emitted assembly

The peephole pass eliminates 5,363 instructions when ZCC compiles itself.

## Build System

```makefile
# Standard build (GCC bootstrap)
make

# Full self-hosting verification
make selfhost

# Compile SQLite and run the test harness
make sqlite

# Clean all build artifacts
make clean
```

**Requirements:** GCC (or any C compiler for initial bootstrap), GNU Make, x86-64 Linux (native or WSL)

**Bootstrap chain:**

1. `gcc` compiles `zcc.c` + `compiler_passes.c` + `compiler_passes_ir.c` → `zcc` (stage 0)
2. `zcc` compiles `zcc.c` → `zcc2.s` (stage 1)
3. `gcc` assembles `zcc2.s` + passes → `zcc2` (stage 1 binary)
4. `zcc2` compiles `zcc.c` → `zcc3.s` (stage 2)
5. `cmp zcc2.s zcc3.s` — must be byte-identical (self-host invariant)

## Testing

### Regression Test Suite

`zcc_battle_phase3.sh` contains 21 targeted tests across 10 categories:

| Category | Tests | What It Covers |
|----------|-------|----------------|
| Basic Operations | 3 | Return values, arithmetic, if-branches |
| Loops / Mem2Reg | 3 | For-loops, while-loops, multiple local variables |
| Pointers / Memory | 2 | Pointer dereference, array pointer arithmetic |
| Function Calls | 3 | Simple calls, 6-arg calls, recursion (fibonacci) |
| Structs | 1 | Struct member access |
| Switch | 1 | Switch/case/default |
| Globals | 1 | Global variables, cross-function mutation |
| Complex Control Flow | 3 | Nested loops, ternary expressions, logical operators |
| Register Pressure | 2 | High register count, 8+ local variables (spill stress) |
| cc_alloc Pattern | 1 | Malloc + zero-fill loop |

```bash
bash zcc_battle_phase3.sh
```

### Fuzz Suite

`fuzz_host.py` compiles and runs 53 seed programs through ZCC and verifies correct output:

```bash
python3 fuzz_host.py --seeds seeds --zcc ./zcc2
```

Seeds include targeted regression tests for:
- Octal escape sequences (`\100`, `\040`)
- `sizeof` on string literal arrays
- Negative values in static `signed char` arrays
- Unsigned arithmetic, shift operations, signed/unsigned comparisons
- Nested ternary expressions, multi-function checksum accumulation

### IR Backend Verification

`verify_ir_backend.sh` performs a 7-stage semantic equivalence check between the AST and IR backends. Both must produce identical output on the same input.

## Bug Corpus

ZCC's development produced a compiler bug corpus with ground-truth fixes, CWE classifications, and PRIME energy scores. All bugs were discovered through self-hosting failure analysis, GDB register-level tracing, and SQLite compilation.

| Bug ID | Title | CWE | Severity |
|--------|-------|-----|----------|
| CG-IR-003 | stdout pointer corruption (stale binary sign extension) | CWE-704 | Critical |
| CG-IR-004 | Phantom callee-save push/pop in body_only mode | CWE-682 | Critical |
| CG-IR-005 | PHI liveness inversion, CONDBR copies, serial lost-copy | CWE-682 | Critical |
| CG-IR-006 | Stack frame too small for IR spill slots | CWE-121 | Critical |
| CG-IR-007 | movslq width — OP_LOAD emitting movq for 32-bit loads | CWE-704 | Critical |
| CG-IR-008 | AST/IR stack slot collision — parameter overwrite | CWE-787 | Critical |
| CG-IR-009 | Pre-scan frame depth missing alloca bytes | CWE-131 | High |
| CG-IR-010 | 4-byte movl for pointer load/store truncation | CWE-704 | Critical |
| CG-IR-011 | Callee-saved register mismatch between AST and IR | CWE-682 | Critical |
| CG-IR-012b | 33 hollow accessor stubs returning zero | CWE-476 | Critical |
| CG-IR-013 | ZND_CALL missing from stmt handler | CWE-839 | Critical |
| CG-IR-014 | ZND_ASSIGN missing from expr handler | CWE-839 | Critical |
| LEX-001 | Unsigned literal suffix U/L discarded in lexer | CWE-704 | Critical |
| LEX-002 | Octal escape sequences not implemented | CWE-704 | High |
| INIT-001 | ND_NEG — negative array initializers emitted as zero | CWE-682 | Critical |
| INIT-002 | sizeof(char_array) returning 8 instead of string length | CWE-131 | Critical |
| ABI-001 | System V AMD64 va_list not implemented (3 phases) | CWE-704 | Critical |
| ABI-002 | Struct-by-value parameter passing (Token ABI) | CWE-704 | Critical |
| CODEGEN-001 | cltq sign-extending pointer arithmetic results (8 sites) | CWE-704 | Critical |
| CODEGEN-002 | Global struct initializer emitting 1-byte fields for all types | CWE-787 | Critical |
| ARRAY-001 | Multidimensional array [N][M] parsed as [M][N], wrong stride — 22KB buffer overrun in Lua strcache | CWE-131 | Critical |
| ABI-003 | va_arg register order inverted — addq should be subq for ZCC's downward register spill layout | CWE-704 | Critical |
| CG-IR-019 | SysV Aggregate ABI: split spills and stack offset drift on register exhaustion | CWE-682 | Critical |

## Source Statistics

| Component | Lines | Description |
|-----------|-------|-------------|
| part1.c | ~1,200 | Types, nodes, symbols, scope, memory allocator |
| part2.c | ~800 | Lexer — tokenization, keywords, string/char literals, octal/hex escapes |
| part3.c | ~1,500 | Parser — recursive descent, all C expressions and statements |
| part4.c | ~2,700 | x86-64 codegen, IR gate, register allocator, va_list, struct-by-value |
| part5.c | ~1,100 | main(), peephole optimizer, global emission |
| ir.h | ~200 | IR instruction set definition |
| ir_bridge.h | 186 | AST ↔ IR translation layer |
| ir.c | ~400 | IR module creation, standalone lowerer |
| ir_to_x86.c | ~300 | Standalone IR-to-x86 |
| compiler_passes.c | 7,317 | IR optimization pipeline + body emitter |
| compiler_passes_ir.c | 570 | IR pass helper functions |
| **Total** | **~16,300** | |

## Environment Variables

| Variable | Effect |
|----------|--------|
| `ZCC_IR_BACKEND=1` | Enable IR backend for all functions |
| `ZCC_IR_LOWER=1` | Alternative IR enable flag |
| `ZCC_PGO_INSTRUMENT=1` | Inject PGO counter probes into IR |
| `ZCC_DUMP_PGO_BLOCKS=1` | Dump PGO block info to stderr |
| `ZCC_GEN_PROFILE=<path>` | Write branch probabilities to file |

## Platform

- **Target:** x86-64 Linux (System V AMD64 ABI)
- **Host:** Any system with GCC and Make (including WSL)
- **Register args:** 6 (`rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`)
- **Callee-saved:** `rbx`, `r12`, `r13`, `r14`, `r15`
- **Stack alignment:** 16-byte at call sites
- **Pointer width:** 8 bytes (LP64)
- **No external dependencies** beyond libc and libm

## Limitations

- No preprocessor (use `gcc -E` or `cpp`)
- No bitfields in structs
- No VLA (variable-length arrays)
- No `_Complex`, `_Atomic`, `_Generic`
- No inline assembly
- No multi-file compilation (concatenate or preprocess into one file)
- No debug info emission (no DWARF)
- x86-64 Linux only (no ARM, no macOS, no Windows native)

## Related Projects

- [**zcc-compiler-bug-corpus**](https://huggingface.co/datasets/zkaedi/zcc-compiler-bug-corpus) — real codegen bugs with ground-truth fixes, CWE classifications, PRIME energy scores
- [**zkaedi-cc**](https://huggingface.co/spaces/zkaedi/zkaedi-cc) — 18-agent Hamiltonian C static vulnerability analyzer
- [**ZKAEDI-MINI**](https://huggingface.co/zkaedi/ZKAEDI-MINI-GGUF) — 7B parameter model for compiler and smart contract analysis
- [**zkaedi.ai**](https://zkaedi.ai) — Smart contract auditing platform powered by ZKAEDI PRIME

## How It Works (for compiler enthusiasts)

### The Lexer (part2.c)

Hand-written lexer producing tokens for all C keywords, operators, literals (integer, string, char), and identifiers. Handles escape sequences including full octal (`\100`, `\040`), hex literals, integer suffix flags (`U`, `L`, `UL`, `LL`), and multi-character operators. Keywords are recognized via a lookup table with hash-based fast path. Suffix metadata is preserved and propagated to the parser for correct unsigned type assignment.

### The Parser (part3.c)

Recursive descent parser implementing C's operator precedence through separate functions per precedence level: `parse_assign` → `parse_ternary` → `parse_logor` → `parse_logand` → `parse_bitor` → ... → `parse_unary` → `parse_postfix` → `parse_primary`. Produces an AST of `Node` structures with type annotations resolved during parsing. Handles `__builtin_va_arg` as a dedicated parse node (like `sizeof`) since it takes a type argument, not an expression.

### The Codegen (part4.c)

Tree-walk code generator that emits x86-64 assembly. `codegen_expr()` handles all expression nodes, `codegen_stmt()` handles all statement nodes, `codegen_func()` orchestrates function-level emission including prologue, parameter stores, body, and epilogue.

Implements the full System V AMD64 `va_list` ABI: variadic function prologues spill all 6 integer argument registers to a contiguous 48-byte save area, `va_start` initializes the 24-byte `va_list` struct with correct `gp_offset`, `fp_offset`, `overflow_arg_area`, and `reg_save_area` fields, and `va_arg` dispatches through register save area or overflow area based on `gp_offset`.

Strength reduction is built in: multiply by powers of 2 uses `shl`, multiply by 3/5/9 uses `lea`, divide by powers of 2 uses `sar`/`shr`, modulo by powers of 2 uses `and`. All signed/unsigned operations dispatch to the correct instruction variant.

### The IR Bridge (ir_bridge.h)

Translation layer that maps AST `Node` structures to `ZCCNode` structures consumable by the IR. Handles type mapping (18 C types → IR type enum), variable name extraction, binary operator mapping, and function boundary management.

### The Optimizer (compiler_passes.c)

7,317 lines implementing 12 optimization passes over SSA-form IR. Dominance is computed via the standard iterative algorithm. Dominance frontiers are used for PHI placement in the multi-block Mem2Reg pass. Liveness intervals drive the linear scan register allocator.

### The Self-Host Invariant

The compiler is correct if and only if `zcc₂.s == zcc₃.s` (byte-identical). This is stronger than "compiles without errors" — it means the compiler's own output, when used to compile itself, produces exactly the same assembly. Any codegen bug affecting any function in the compiler itself breaks this invariant.

## License

Apache 2.0

## Author

**ZKAEDI** — [@LongNumberGeek](https://twitter.com/LongNumberGeek) — [zkaedi.ai](https://zkaedi.ai) — [HuggingFace](https://huggingface.co/zkaedi)

Built solo. Every line, every bug, every fix.
