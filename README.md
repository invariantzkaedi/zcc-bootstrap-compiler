![Self-Host Verification](https://github.com/izkaedi-ui/ZCC/actions/workflows/selfhost.yml/badge.svg)

# ZCC: A High-Integrity Self-Hosting C Compiler and EVM Translation Suite

ZCC is a robust, production-capable Systems-C compiler and EVM (Ethereum Virtual Machine) translation framework. Built for high-assurance systems engineering and multi-stage self-hosting validation, ZCC bridges native systems-level C compilation with formal execution tracing and translation.

---

## 🔱 Key Capabilities

* **256-bit EVM Lifter**: Complete translation of EVM bytecode with full instruction compliance.
* **SwarmDecompile**: An automated decompilation engine that has fuzzed and verified 5,000+ smart contracts.
* **Native x86-64 JIT Backend**: High-performance execution engine for compiled target binaries.
* **Symbolic Verification**: Formal validation capabilities (`--prove no-revert`) to mathematically assert instruction safety.
* **Systems Bootstrap Capability**: Successfully compiles itself (self-hosting), Lua 5.4.6, SQLite 3.45.0, and DOOM 1.10.

```bash
make selfhost          # Execute triple-stage compiler bootstrap verification
make swarm-fuzz        # Run the contract fuzzing harness
make swarm-jit         # Validate JIT execution against the contract swarm
make swarm-prove       # Run formal symbolic verification gates
zcc --jit contract.bin -o contract.exe
zcc --prove contract.bin "no-revert"
```

---

## 🔱 Compilation & Verification Status

| Metric | Verification Result |
| :--- | :--- |
| **Self-Hosting** | Pass (Stage 2 $\leftrightarrow$ Stage 3 assembly byte-identical `zcc2.s == zcc3.s`) |
| **Regression Tests** | Pass (21/21 targeted assertions) |
| **Fuzz Suite** | Pass (53/53 fuzzing test cases) |
| **SQLite 3.45.0** | Pass (Full transactional SQL integrity verified) |
| **Lua 5.4.6** | Pass (100% compliance on the core `testes/all.lua` VM test suite) |
| **IR Backend Tests** | Pass (21/21 passes verified) |
| **Peephole Elisions** | 16,067 redundant instructions optimized during self-host compilation |

### SQLite Integration & Compliance
ZCC compiles SQLite 3.45.0 (approx. 85,000 lines of amalgamated source) out of the box. The resulting binary successfully performs complete SQL transactional workflows:
* Verifies B-Tree allocations, the LALR(1) parser, standard memory allocators, and page caches.
* Stabilized by fixing System V AMD64 `va_list` structure layouts, nested global structure initializers, negative array offset constants, and struct-by-value System V ABI rules.

```text
SQLite 3.45.0 compiled by ZCC
open rc=0
1 = 1
SELECT 1 rc=0 err=none
CREATE TABLE rc=0 err=none
INSERT rc=0 err=none
x = 42
SELECT rc=0 err=none
```

### Lua 5.4.6 Integration
ZCC compiles Lua 5.4.6 (approx. 30,000 lines) and passes the complete `testes/all.lua` suite (excluding host-dependent subprocess calls). All core components—including garbage collection (`gc.lua`), debug structures (`db.lua`), closure scopes (`closure.lua`), numeric ranges (`math.lua`), and coroutine mechanisms—run with production-grade stability under ZCC-compiled code.

### DOOM 1.10 Compilation
ZCC successfully compiles, links, and executes the entire `linuxdoom-1.10` source code (approx. 45,000 lines). The compiled binary parses WAD format assets, initializes sub-systems, and renders 3D frames into an X11 framebuffer without pointer corruption or alignment faults, validating ZCC's support for complex C structures and global state management under System V ABI constraints.

### Metacompiler Chain
ZCC compiles third-party C compilers (such as TinyCC), which in turn compile and link verified executables:
```text
ZCC ──> Compiles tinycc.c ──> Generates tcc binary
tcc ──> Compiles "int main() { return 42; }" ──> Generates target binary
target binary ──> Returns exit code 42
```

---

## 🔱 Quick Start

### 1. Build and Bootstrap
ZCC uses a three-stage bootstrap pipeline to verify compiler correctness:
```bash
# Clone the repository
git clone https://github.com/izkaedi-ui/ZCC.git
cd ZCC

# Build Stage 1, Stage 2, and Stage 3, and verify byte-identity
make selfhost
```

### 2. Run Verification Checks
```bash
# Run the core compiler regression test suite
bash zcc_battle_phase3.sh

# Run the contract fuzzing suite
python3 fuzz_host.py --seeds seeds --zcc ./zcc2
```

### 3. Compile Standard Programs
```bash
./zcc2 hello.c -o hello.s
gcc -o hello hello.s
./hello
```

---

## 🔱 Supported C Language Specifications

* **Primitive Types**: `char`, `short`, `int`, `long`, `long long` (both signed and unsigned), `_Bool`, `void`.
* **Derived Types**: Multi-dimensional arrays, pointers, structures (including packing attributes), unions, function pointers, and `typedef` declarations.
* **Statements**: Block scopes, selection statements (`if`/`else`, `switch`/`case`/`default`), iteration statements (`while`, `do`/`while`, `for`), jump statements (`break`, `continue`, `return`, `goto`), and labeled statements.
* **Expressions**: Complete arithmetic/logical/comparison operators, assignment operator variants, pre/post increment and decrement, ternary operator (`?:`), comma operator, `sizeof`, casting operators, member accesses (`.`, `->`), array subscripting (`[]`), function calls, and variadic function macros (`va_list`, `va_start`, `va_arg`).
* **Storage Classes**: Local/global variables, `static`, `extern`, string literals, and forward declarations.
* **Integrated Preprocessing**: ZCC includes an integrated preprocessor (`part0_pp.c`) that automatically processes macro expansions, conditional directives, and header inclusions during compilation.

---

## 🔱 Architectural Layout

ZCC compiles single-translation-unit C source files. The compiler is composed of discrete modules concatenated into a unified `zcc.c` compilation unit:

```
┌─────────────────────────────────────────────────────────┐
│                    zcc.c (concatenated)                  │
│                                                         │
│  part1.c ─── Type system, symbols, scopes, allocators   │
│  part0_pp.c ── C preprocessor and header inclusion      │
│  part2.c ─── Lexical scanner (tokenizer)                │
│  part3.c ─── Recursive descent parser                   │
│  ir.h    ─── IR Instruction definitions                 │
│  ir_emit_dispatch.h ─ IR lowering dispatch tables       │
│  ir_bridge.h ─────── AST-to-IR translation layer        │
│  part4.c ─── x86-64 code generation & registers         │
│  part5.c ─── Compiler driver & peephole optimizer       │
│  ir.c    ─── IR module structures & lowerer             │
│  ir_to_x86.c ─────── IR-to-x86 instruction lowerer      │
│                                                         │
├─────────────────────────────────────────────────────────┤
│              Linked separately:                         │
│                                                         │
│  compiler_passes.c ── Optimization passes & body        │
│                       emission (7,317 lines)            │
│  compiler_passes_ir.c ── Utility functions for IR passes │
└─────────────────────────────────────────────────────────┘
```

### Dual-Emission Pipeline
ZCC implements two separate code generation backends selectable at function-level granularity:

1. **AST-Direct Path** (`part4.c`):
   * Performs direct AST-to-assembly translation.
   * Generates target x86-64 assembly instructions without intermediate representation.
   * Features native strength reductions, signed/unsigned instruction selection, and an assembly-level peephole optimizer.
   * Fully verified to compile the self-hosting compiler stages and SQLite.

2. **IR Backend** (`compiler_passes.c`):
   * Translates AST representations to a 3-address SSA (Static Single Assignment) Intermediate Representation via `ir_bridge.h`.
   * Executes a multi-pass optimization pipeline.
   * Emits optimized assembly.
   * Uses a hybrid framing structure (`body_only` mode) where the AST manages stack frame allocation (prologue/epilogue) and the IR backend controls body emission.

*Hybrid Gating*: The IR backend path is controlled on a per-function basis by `ir_whitelisted()` inside `part4.c`. Functions not whitelisted fall back to the AST-direct path, allowing for incremental optimization testing.

### SSA IR Instruction Set
The IR utilizes a virtual-register architecture supporting unlimited registers:
* **Arithmetic & Bitwise**: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `NEG`, `AND`, `OR`, `XOR`, `SHL`, `SHR`, `NOT`.
* **Memory & Control**: `LOAD`, `STORE`, `ALLOCA`, `ADDR`, `BR`, `CONDBR`, `RET`, `CALL`, `PHI`.
* **Metadata & Types**: `CONST`, `COPY`, `CAST`, `NOP`, `PGO_COUNTER_ADDR`.

### Optimization Passes
The optimization pipeline (`run_all_passes()` in `compiler_passes.c`) executes:
1. **Control-Flow Reachability Analysis**: Prunes dead code blocks.
2. **Parameter Escape Marking**: Discovers if parameters escape to memory.
3. **PGO Instrumentation**: Injects branch profiling counter probes.
4. **Constant Folding**: Statically folds numeric and address constants.
5. **Strength Reduction**: Translates multiplication/division to bit shifts and additions.
6. **Copy Propagation**: Eliminates redundant virtual register allocations.
7. **IR Peephole Optimization**: Performs algebraic and layout simplifications.
8. **Redundant Load Elimination (RLE)**: Removes redundant loads from identical memory addresses.
9. **Dead Code Elimination (DCE)**: Recursively removes instructions with zero active uses.
10. **Escape Analysis**: Promotes heap/stack memory references to virtual registers where safe.
11. **Scalar Promotion (Mem2Reg)**: Promotes local stack allocations to SSA registers.
12. **PGO Block Reordering**: Optimizes basic block physical layout to improve instruction cache locality.

### Register Allocation & Spilling
The IR backend implements a linear scan register allocator. Registers are allocated from the standard System V AMD64 callee-saved register pool (`rbx`, `r12`-`r15`). Overflow variables are spilled into dedicated stack frames managed through relative stack offsets (`slot_base`).

### Assembly Peephole Optimization
The AST-direct backend features an assembly-level post-emission peephole pass (`part5.c`) that scans generated instruction sequences to:
* Remove redundant self-moves (`movq %rax, %rax`).
* Eliminate redundant `push`/`pop` sequences.
* Eliminate dead store operations and redundant load-after-store steps.
* Apply target machine strength reductions (e.g. replacing multiply with `lea`).

---

## 🔱 Verification and Testing

### 1. Regression Test Suite
Executable via `zcc_battle_phase3.sh`, this suite validates:

| Category | Coverage Focus |
| :--- | :--- |
| **Basic Operations** | Return values, arithmetic, conditional branching |
| **Loops / Mem2Reg** | For-loops, while-loops, multi-variable optimization |
| **Pointers / Memory** | Multi-level dereferencing, array stride calculations |
| **Function Calls** | Parameter passing, stack alignment, recursion (Fibonacci) |
| **Struct / Union** | Offset calculation, structure layout alignment |
| **Switch Block** | Switch/case jumping and default fallthrough |
| **Globals** | Multi-unit mutable state propagation |
| **Complex CFG** | Chained loops, nested ternary expressions, logical operators |
| **Register Pressure** | Register allocation spills and parameter exhaustion |
| **cc_alloc Pattern** | Dynamic allocation tracking and zero-fill sequences |

### 2. Differential Fuzzing
`fuzz_host.py` runs differential verification across random and targeted seeds comparing ZCC's execution output against GCC and Clang. Tests cover:
* Octal and hexadecimal escape sequences inside character arrays.
* Correct execution of `sizeof` operators on string literal arrays.
* Unsigned comparison boundary conditions and shift ranges.
* Complex ternary operation chaining.

### 3. IR-AST Equivalence Checking
`verify_ir_backend.sh` performs multi-stage checks asserting that compiling target sources using the IR backend produces execution results identical to the AST-direct backend.

---

## 🔱 Bug Corpus

ZCC's development is backed by a compiler bug corpus containing ground-truth fixes, CWE classifications, and severity ratings:

| Bug ID | Title | CWE | Severity |
| :--- | :--- | :--- | :--- |
| **CG-IR-003** | stdout pointer corruption (stale binary sign extension) | CWE-704 | Critical |
| **CG-IR-004** | Phantom callee-save push/pop in body_only mode | CWE-682 | Critical |
| **CG-IR-005** | PHI liveness inversion, CONDBR copies, serial lost-copy | CWE-682 | Critical |
| **CG-IR-006** | Stack frame too small for IR spill slots | CWE-121 | Critical |
| **CG-IR-007** | movslq width — OP_LOAD emitting movq for 32-bit loads | CWE-704 | Critical |
| **CG-IR-008** | AST/IR stack slot collision — parameter overwrite | CWE-787 | Critical |
| **CG-IR-009** | Pre-scan frame depth missing alloca bytes | CWE-131 | High |
| **CG-IR-010** | 4-byte movl for pointer load/store truncation | CWE-704 | Critical |
| **CG-IR-011** | Callee-saved register mismatch between AST and IR | CWE-682 | Critical |
| **CG-IR-012b** | 33 hollow accessor stubs returning zero | CWE-476 | Critical |
| **CG-IR-013** | ZND_CALL missing from stmt handler | CWE-839 | Critical |
| **CG-IR-014** | ZND_ASSIGN missing from expr handler | CWE-839 | Critical |
| **LEX-001** | Unsigned literal suffix U/L discarded in lexer | CWE-704 | Critical |
| **LEX-002** | Octal escape sequences not implemented | CWE-704 | High |
| **INIT-001** | ND_NEG — negative array initializers emitted as zero | CWE-682 | Critical |
| **INIT-002** | sizeof(char_array) returning 8 instead of string length | CWE-131 | Critical |
| **ABI-001** | System V AMD64 va_list support (3 phases) | CWE-704 | Critical |
| **ABI-002** | Struct-by-value parameter passing (Token ABI) | CWE-704 | Critical |
| **CODEGEN-001** | cltq sign-extending pointer arithmetic results (8 sites) | CWE-704 | Critical |
| **CODEGEN-002** | Global struct initializer emitting 1-byte fields for all types | CWE-787 | Critical |
| **ARRAY-001** | Multidimensional array parsing stride bounds mismatch | CWE-131 | Critical |
| **ABI-003** | va_arg register order inversion in downward layout | CWE-704 | Critical |
| **CG-IR-019** | SysV Aggregate ABI split spills and stack offset drift | CWE-682 | Critical |

---

## 🔱 Source Statistics

| Component | Lines | Description |
| :--- | :--- | :--- |
| **part1.c** | ~1,200 | Type systems, symbols, scopes, memory allocators |
| **part0_pp.c** | ~2,200 | C macro preprocessor and header inclusion resolver |
| **part2.c** | ~800 | Lexical scanner, token mappings, escapes |
| **part3.c** | ~1,500 | Recursive descent parser for statements & expressions |
| **part4.c** | ~2,700 | Code generation, System V calling conventions |
| **part5.c** | ~1,100 | Compiler driver, globals, peephole optimizer |
| **ir.h** | ~200 | SSA Intermediate Representation instruction set |
| **ir_bridge.h** | ~186 | AST-to-IR translation layer |
| **ir.c** | ~400 | IR module management and lowering |
| **ir_to_x86.c** | ~300 | IR-to-assembly translator |
| **compiler_passes.c** | ~7,317 | Optimization passes and block layout emitter |
| **compiler_passes_ir.c**| ~570 | IR helper utilities |
| **Total** | **~18,500** | Full compiled footprint |

---

## 🔱 Environment Variables

| Variable | Effect |
| :--- | :--- |
| `ZCC_IR_BACKEND=1` / `ZCC_IR_LOWER=1` | Forces all compilation paths through the IR backend |
| `ZCC_PGO_INSTRUMENT=1` | Injects branch profiling counter probes into compiled IR |
| `ZCC_DUMP_PGO_BLOCKS=1` | Outputs basic block execution probabilities to standard error |
| `ZCC_GEN_PROFILE=<path>` | Records branch probability data to the specified path |

---

## 🔱 Known Constraints & Limitations

ZCC enforces the following design limitations:
* **No Inline Assembly**: C-native inline assembly statements are unsupported.
* **Unsupported C Features**: Bitfields in structures, variable-length arrays (VLAs), and certain C11 keywords (`_Atomic`, `_Generic`, `_Complex`) are not supported.
* **Linkage Boundaries**: Expects single-file compilation (concatenated sources).
* **Target Architecture**: Generates assembly exclusively for x86-64 Linux architectures using the System V AMD64 ABI specification.

---

## 🔱 QEC-VOP Quick Start

**QEC-VOP (QEC Verification Operations Platform): a deterministic, policy-enforced verification operations platform.**

**QEC-VOP: deterministic verification control plane**

Initialize and verify the verification control plane:
```bash
# 1. Ingest generated test artifacts
python3 scripts/ingest_artifacts.py

# 2. Compile dashboard report
python3 scripts/generate_dashboard_data.py

# 3. Synchronize open incidents from tracker
python3 scripts/sync_incidents.py --dry-run

# 4. View visual report locally
# Open tools/qec_dashboard.html in a web browser
```

---

## 🔱 Related Projects

* [**zcc-compiler-bug-corpus**](https://huggingface.co/datasets/zkaedi/zcc-compiler-bug-corpus) — Curated database of codegen defects and fixes categorized by CWE classifications.
* [**zkaedi-cc**](https://huggingface.co/spaces/zkaedi/zkaedi-cc) — Static analysis tool leveraging multi-agent validation.
* [**ZKAEDI-MINI**](https://huggingface.co/zkaedi/ZKAEDI-MINI-GGUF) — Compact language model optimized for smart contract and compiler code generation analysis.

---

## License

ZCC is distributed under the **Apache License 2.0**.

---

## Author & Credits

Developed by **ZKAEDI** ([zkaedi.ai](https://zkaedi.ai) | [HuggingFace](https://huggingface.co/zkaedi)).
