# ZKAEDI SYSTEMS — MASTER EXTENDED README
### Complete Engineering Provenance · All Sessions · All Systems · All Discoveries
> Compiled from 142 conversation sessions · 5,449 unique file traces · 3,017 command executions
> Repository: `H:\__DOWNLOADS\zcc_github_upload` · Author: zkaedi · Compiled: 2026-07-19

---

## TABLE OF CONTENTS

1. System Architecture Overview
2. ZCC Compiler — Full Engineering History
   - 2.1 Bootstrapping Chain & Self-Hosting
   - 2.2 Preprocessor (part0_pp.c)
   - 2.3 Parser & AST (part1-part2)
   - 2.4 Semantic Analysis & Type System (part3)
   - 2.5 Code Generation (part4)
   - 2.6 Runtime & Symbol Table (part5)
   - 2.7 IR Bridge Pipeline
   - 2.8 Rust Frontend Integration (part7_rust)
   - 2.9 SQLite 3.53.1 Self-Hosting
   - 2.10 Doom Engine Milestone
   - 2.11 Float Gauntlet & Precision Engine
   - 2.12 ABI & Struct Layout Verification
   - 2.13 Compiler Passes Manager
   - 2.14 Juliet CWE Test Suite
   - 2.15 Known Bugs & Error Learner Corpus
   - 2.16 Verification Gates Protocol
   - 2.17 Forensic Chain & Tickets
3. ZKAEDI PRIME — Hamiltonian Dynamics Engine
   - 3.1 Canonical Two-Field Equation
   - 3.2 Two-Regime Constraint
   - 3.3 FHN Spatiotemporal Chaos (v1-v26)
   - 3.4 ZKAEDI PRIME Forge v5 / OMEGA SUPREME
   - 3.5 PRIME Gauntlet & CI Verification
   - 3.6 Meta-Learning Integration
   - 3.7 Ensemble Fusion Engine
4. Systems Visual Observatory
   - 4.1 SPH Fluid Dynamics Solver
   - 4.2 CPU Pipeline & Cache Profiler
   - 4.3 Compiler Register Allocator Sandbox
   - 4.4 Hamiltonian Orbit Solver
   - 4.5 SDF Shader Compiler & Risk Auditor
5. Dark Forest Suite — Security & MEV Engine
   - 5.1 The Dark Forest
   - 5.2 EVM Exploit Theater
   - 5.3 Mempool Warfare Simulator
   - 5.4 Contract Scanner
   - 5.5 Trace Analyzer
   - 5.6 Scenario Forge
   - 5.7 Omni Parser
   - 5.8 Vulnerability Topology Report
   - 5.9 Cloudflare Worker Integration
6. 3D Mesh, Raytracing & VFX Pipeline
   - 6.1 ZKAEDI 3D Tools Suite
   - 6.2 ZK3D SAH BVH Unpacker
   - 6.3 VFX Asset Processing Microservice
   - 6.4 Mesh Quality Gate & Dataset Parser
   - 6.5 Meshy/Tripo3D Plugin Integration
   - 6.6 Blender Animation Pipeline
   - 6.7 Fleet Pipeline Automation
7. Interactive WebGL 3D Visualizer Suite (13 scenes)
8. Blockchain, NFT & Ledger Systems
9. Safe Research Toolkit & Telemetry
10. Browser Extensions & Edge Tooling
11. Biomedical & Genomic Science Engine (25+ APIs)
12. Orchestration Mesh & Multi-Agent Systems
13. Knowledge Base & Forensic Chain
14. Operational Protocols & Rules
15. Session Index — 142 Conversations
16. Complete File Trace Index — 5,449 Files
17. Command Execution History — 3,017 Commands
18. Post-Mortems & Forensic Documents
19. Brain Artifacts by Session
20. System Map & Architecture Diagram

---

# 1. SYSTEM ARCHITECTURE OVERVIEW

This repository and its surrounding ecosystem represents over **3 months of continuous
engineering** across 10+ distinct technical domains. The work spans from a self-hosting
C compiler targeting x86-64, through Hamiltonian dynamical systems solvers, interactive
WebGL 3D physics engines, Ethereum MEV combat simulators, steganographic vault engines,
3D mesh processing pipelines, biomedical genomic query APIs, and an autonomous
multi-agent orchestration mesh.

All systems are governed by a strict forensic discipline documented in `AGENTS.md`,
enforcing evidence-first development with byte-level verification gates, immutable
commit provenance, and zero-claim-without-artifact policies.

```
+-------------------------------------------------------------------------+
|                       ZKAEDI SYSTEMS ECOSYSTEM                          |
+--------------+--------------+--------------+--------------+-------------+
|  ZCC         |  ZKAEDI      |  Dark Forest |  3D / VFX    |  Bio        |
|  Compiler    |  PRIME       |  Suite       |  Pipeline    |  Science    |
|  Engine      |  Hamiltonian |  Security    |  & WebGL     |  APIs       |
+--------------+--------------+--------------+--------------+-------------+
|  part0-5     |  H_t solver  |  Dark Forest |  BVH SAH     |  AlphaFold  |
|  IR Bridge   |  FHN Chaos   |  EVM Theater |  SDF Shader  |  AlphaGenom |
|  Rust Front  |  Forge v5    |  Mempool War |  3D Tools    |  ChEMBL     |
|  SQLite      |  Meta-MAML   |  NFT Mint    |  VFX Service |  GTEx       |
|  Doom        |  Ensemble    |  Ledger      |  Meshy Plugn |  PubMed     |
+--------------+--------------+--------------+--------------+-------------+
|              ORCHESTRATION MESH * CANARY GATES * LINEAGE LEDGER         |
|              142 SESSIONS * 5,449 FILES * 3,017 COMMANDS                |
+-------------------------------------------------------------------------+
```

---

# 2. ZCC COMPILER — FULL ENGINEERING HISTORY

## 2.1 Bootstrapping Chain & Self-Hosting

ZCC (Zkaedi C Compiler) is a self-hosting C compiler targeting x86-64 Linux SystemV ABI.
It compiles itself through three stages:

- **Stage 1**: GCC compiles ZCC source -> `zcc` binary
- **Stage 2**: `zcc` (stage 1) compiles ZCC source -> `zcc2` binary
- **Stage 3**: `zcc2` (stage 2) compiles ZCC source -> `zcc3` binary
- **Verification**: `cmp zcc2.s zcc3.s` — byte-identical assembly output required

```bash
# Primary bootstrap command
make selfhost

# Expected terminal output on success
SELF-HOST VERIFIED (assembly identical)

# Gate 1 verification
cmp zcc2.s zcc3.s && echo "Gate 1: PASS"
```

The self-hosting chain is the **primary correctness invariant**. Any codegen change that
breaks byte-identity between stage-2 and stage-3 assembly output is a regression,
regardless of whether the compiler still produces functional programs.

### Source File Concatenation Order

```
part0_pp.c      # Preprocessor (C89/C99/C11/C17 macros, includes, conditionals)
part1.c         # Lexer, token definitions
part2.c         # Parser infrastructure, expression parsing
part3.c         # Semantic analysis, type checking, symbol resolution
part4.c         # Code generation, x86-64 emission
part5.c         # Runtime builtins, stdio registration, stdlib symbol table
```

### Build Commands

```bash
# WSL Ubuntu (canonical build environment)
cd /mnt/h/__DOWNLOADS/zcc_github_upload
make zcc           # Build stage-1 ZCC
make selfhost      # Full 3-stage bootstrap + verification
make test          # Run test corpus
make clean         # Clean all generated binaries
```

---

## 2.2 Preprocessor (part0_pp.c)

The preprocessor is the most forensically sensitive component, representing approximately
**20 hours of landed forensic work** across multiple sessions. Hard prohibition: no
wholesale rewrites — diff must be under 50 lines unless explicitly authorized.

### Key Features Implemented

- Full C89/C99/C11 macro expansion with recursive substitution
- `#include` file resolution with search path management
- `#ifdef` / `#ifndef` / `#elif` / `#else` / `#endif` conditional compilation
- **Hideset barrier** (PP-MACRO-HIDESET-BARRIER): Prevents recursive macro self-expansion
- **Blocked list bounds** (PP-BLOCKED-LIST-BOUNDS): Prevents OOB in macro expansion tables
- Variadic macros (`__VA_ARGS__`)
- Stringification (`#`) and token pasting (`##`) operators
- Predefined macros (`__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__func__`)
- `_Pragma` operator support
- Line continuation (`\` at end of line)
- Trigraph and digraph sequences

### Known Issues Fixed

- **PP-BLOCKED-LIST-BOUNDS** (`pp-blocked-list-bounds-gate-evidence.md`): Bounds overflow
  in macro argument list parsing when more than the maximum supported argument count is passed.
- **PP-MACRO-HIDESET-BARRIER**: Recursive function-like macros caused infinite expansion
  loops without proper hideset tracking.
- **PP-REWRITE-REGRESSION-ROLLBACK** (commit `8098a94`): A wholesale rewrite attempt
  failed and was rolled back. Canonical case study for why surgical edits are mandatory.

### Forensic Files

```
tickets/PP-REWRITE-REGRESSION-ROLLBACK.md
tickets/pp-blocked-list-bounds-gate-evidence.md
tickets/pp-macro-hideset-barrier-gate-evidence.md
FORENSIC_CORRECTION_2026-04-19.md
```

---

## 2.3 Parser & AST (part1-part2)

### Lexer (part1.c)

- Full C tokenizer including all keyword recognition
- Integer, float, char, and string literal parsing
- Multi-character operator tokens (`->`, `++`, `--`, `<<=`, `>>=`, etc.)
- UTF-8 aware character handling

### Parser (part2.c)

- Recursive descent parser for full C grammar
- Expression parsing with full operator precedence (15 levels)
- Declaration parsing including complex declarators (pointers to functions, arrays of pointers)
- Statement parsing: `if`, `while`, `for`, `do-while`, `switch`, `return`, `goto`, `break`, `continue`
- Struct/union/enum declaration and definition
- Typedef handling and typedef-name disambiguation
- `_Generic` selection (C11)
- `_Alignas` and `_Alignof` (C11)
- `_Static_assert` (C11)
- `__attribute__` extensions (GCC compatibility)

### Token Name Function Rule

`token_name(int t)` must be declared globally in `part1.c` and defined without `static`
keyword in `part2.c`. Individual part compilation (Gate IR-1 coverage script) requires
this visibility. (E-LEARN cross-reference: AGENTS.md linkage rule)

---

## 2.4 Semantic Analysis & Type System (part3)

### Type System

- Complete C type hierarchy: `void`, `char`, `short`, `int`, `long`, `long long`, all unsigned
- `float`, `double`, `long double`
- Pointer types with const/volatile/restrict qualifiers
- Array types (complete and incomplete)
- Function types with parameter type lists
- Struct and union types with full member layout
- Enum types

### Symbol Resolution

- Block-scoped symbol tables (nested scopes)
- File-scope vs function-scope storage classes (`extern`, `static`, `auto`, `register`)
- **Tag/scope shadowing**: Struct tags in separate namespace from ordinary identifiers
- Forward declaration tracking

### FORENSIC_023A_PARSER001

A scoping bug where struct tag declarations in nested scopes would shadow outer-scope tag
declarations incorrectly, causing type mismatch errors during codegen. Documented in
`FORENSIC_023A_PARSER001.md`.

---

## 2.5 Code Generation (part4)

The code generator emits x86-64 AT&T-syntax assembly targeting Linux SystemV ABI.

### Register Allocation

- Primary registers: `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`-`r15`
- Caller-saved: `rax`, `rcx`, `rdx`, `rsi`, `rdi`, `r8`-`r11`
- Callee-saved: `rbx`, `r12`-`r15`, `rbp`
- Stack frame management with `rbp` as frame pointer
- `rsp` alignment to 16 bytes before `call` instructions

### ABI Implementation

- Up to 6 integer/pointer arguments in registers: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`
- Up to 8 float/double arguments in SSE registers: `xmm0`-`xmm7`
- Return values: `rax` for integers, `xmm0` for floats/doubles
- Variadic function support with `al` register argument count

### Key Codegen Bugs Fixed

#### CG-CMP-001 — setle/setge Opcode Swap

```
Root cause: setle and setge swapped in comparison opcode table in src/codegen.c
Fix:        Swapped the two entries in the comparison dispatch table
Detection:  797-function regression corpus caught this
KI:         knowledge/cg_cmp_001_setle_setge_opcode_swap/
```

#### CG-IR-003 — stdout Sign Extension

```
Root cause: stdout emitted as 32-bit immediate in IR mode, sign-extension corruption
            when written to 64-bit void* pointer slot
Fix:        Emit stdout reference as 64-bit void* symbol reference
KI:         knowledge/zcc_ir_bridge_pipeline_state/
```

#### CG-GINIT-FLOAT-002 — Float Limits Global Initializer

```
Root cause: eval_const_expr_p4 did not evaluate float arithmetic during static global
            data emission, causing INFINITY = 1.0f/0.0f and DBL_MAX to emit as 0.0
Fix:        Float-aware constant folding in eval_const_expr_p4
Helpers:    isnan(), isinf(), isfinite() bit-test helper functions added
KI:         knowledge/zcc_sqlite_float_limits/
```

#### DOOM MILESTONE — Seven Systems Discoveries

During compilation of `linuxdoom-1.10`, seven permanent compiler discoveries were made:

```
1. ABI:       Structs passed by value require complete copy to stack
2. Void check: void-returning function in expr context must not emit rax pop
3. Promotion:  Unary minus on unsigned char/short must promote to int first
4. CMP:       setle/setge opcode swap (CG-CMP-001)
5. 7-arg:     7th+ arguments on stack in correct order with proper alignment
6. AST:       goto across variable declarations requires VLA-style stack management
7. Squasher:  Combined verification test for all 7 discoveries
```

KI: `knowledge/zcc_doom_milestone/artifacts/zcc_doom_bugs.md`

---

## 2.6 Runtime & Symbol Table (part5)

### stdio Registration

`stdout`, `stderr`, `stdin` must be explicitly registered as `void*` (64-bit pointer):

```
Before fix: zcc2 executed as stale binary with stdout as 32-bit integer
            -> SIGSEGV inside ZCC_IR_FLUSH / ir_module_emit_text calling vfprintf
Fix:        Explicit void* registration of all three standard streams
Gate:       Always run: make clean && make selfhost && make ir-verify
KI:         knowledge/zcc_ir_bridge/
```

### Standard Library Coverage

```
printf, fprintf, sprintf, snprintf
malloc, calloc, realloc, free
memcpy, memmove, memset, memcmp
strlen, strcmp, strncmp, strcpy, strcat
fopen, fclose, fread, fwrite, fseek, ftell, rewind
exit, abort, atexit
qsort, bsearch
strtol, strtod, atoi, atof
sin, cos, tan, sqrt, pow, fabs, floor, ceil, exp, log
isnan, isinf, isfinite (bit-test implementations)
```

---

## 2.7 IR Bridge Pipeline

### Architecture

```
Source C
    |
    v
ZCC Frontend (part0-part3)
    |
    v
ZCC Codegen (part4) ---- x86-64 Mode ------> .s assembly
    |
    +--- IR Emission Mode -----------------> .ir SSA text
                                                 |
                                                 v
                                       ir_pass_manager.c
                                                 |
                                       +---------+----------+
                                       |                    |
                                  DCE Pass            Const Fold Pass
                                       |                    |
                                       +---------+----------+
                                                 |
                                                 v
                                       ir_to_x86.c -> .s assembly
```

### Key Files

```
ir.h                    -- IR node definitions, SSA value numbering, basic block structure
ir_pass_manager.c       -- Pass registration, ordering, dependency tracking
ir_to_x86.c             -- IR -> x86-64 lowering
zcc_ir_opt_passes.h     -- Pass interface declarations
gate_ir1.sh             -- Gate IR-1 coverage script (compiles parts individually)
```

### IR SSA Format

```
; ZCC IR 3-Address SSA Format Example
define i64 @safe_div(i64 %a, i64 %b) {
entry:
  %cmp = icmp eq i64 %b, 0
  br i1 %cmp, label %div_zero, label %do_div
div_zero:
  ret i64 0
do_div:
  %result = sdiv i64 %a, %b
  ret i64 %result
}
```

### Evidence Directory

```
docs/evidence/2026-07-16/safe_div_ir/
    commands.txt     -- Exact gate commands
    stdout.log       -- Full command output
    artifacts.md     -- IR artifact paths and checksums
    verdict.md       -- Gate status: PASS/FAIL
```

---

## 2.8 Rust Frontend Integration (part7_rust)

`part7_rust.c` implements the interoperability bridge allowing ZCC to parse Rust-style
safety primitive syntax and compile it to the existing x86-64 codegen backend.

```
tests/rust/test_safe_div_rust.rs        -- Safe division in Rust syntax
tests/rust/test_float_abi_strict.rs     -- Strict float ABI verification
rust_frontend_plan.md                   -- Full design specification
```

---

## 2.9 SQLite 3.53.1 Self-Hosting

SQLite's amalgamation (sqlite3.c, ~250,000 lines) uses nearly every C feature.

### Bug Fixes for ZCC-SQLite Compatibility

**NestedParse VLA Null-Dereference**
```
Root cause: ZCC's VLA codegen emitted incorrect stack pointer adjustment for NestedParse
Fix:        Corrected alloca-equivalent emission in codegen for nested VLA declarations
```

**Union String Initialization**
```
Root cause: sqlite3DigitPairs union with string member + designated initializer; ZCC failed
Fix:        Extended union initializer parsing in part3.c
```

### Parse Struct Layout Parity (CRITICAL)

The Parse struct has different sizes between GCC and ZCC:

```
GCC layout:  280 bytes (standard) / 312 bytes (extended)
ZCC layout:  424 bytes (SystemV ABI alignment differences)

CORRECT ZCC OFFSETS (never hardcode GCC values):
  offsetof(Parse, sLastToken)                    = 288
  sizeof(Parse) - offsetof(Parse, sLastToken)    = 136

GCC offsets (WRONG under ZCC, causes FinishCoding segfault):
  offsetof(Parse, sLastToken)                    = 176  <-- DO NOT USE
```

---

## 2.10 Doom Engine Milestone

**Status**: VERIFIED — id Software Doom (linuxdoom-1.10) compiles and executes under ZCC.

```bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload
make doom CC=./zcc
./doom -iwad doom.wad -nosound -nomusic
```

Seven systems discoveries documented in `knowledge/zcc_doom_milestone/`.

---

## 2.11 Float Gauntlet & Precision Engine

```
zcc_float_gauntlet.c    -- IEEE 754 edge cases: NaN, +Inf, -Inf, +0.0, -0.0,
                           denormals, float<->double conversion, transcendentals,
                           SSE2 vs x87 precision modes
float_guard.py          -- GCC vs ZCC binary diff guard script
tests/run_float_fuzz.py -- Randomized float arithmetic fuzzer
```

---

## 2.12 ABI & Struct Layout Verification

```
tools/abi_bitfield_lane_gen.py  -- Generates C programs with struct layouts,
                                   compiles with GCC + ZCC, runs __builtin_offsetof
                                   verification, reports ABI violations
```

---

## 2.13 Compiler Passes Manager

`compiler_passes.c` manages ordered execution:

```
1. Preprocessing pass  (part0_pp.c)
2. Parsing pass        (part1.c + part2.c)
3. Semantic pass       (part3.c)
4. Codegen pass        (part4.c)
5. IR optimization     (ir_pass_manager.c)
   - Dead Code Elimination (DCE)            [IMPLEMENTED]
   - Constant Folding                       [IMPLEMENTED]
   - Copy Propagation                       [PLANNED]
   - Common Subexpression Elimination       [PLANNED]
```

---

## 2.14 Juliet CWE Test Suite

`juliet_train_subset/` — NIST/NSA Juliet Test Suite subset:

```
CWE-121: Stack-based buffer overflow (s02-s08)
CWE-190: Integer overflow (s06)
```

Used to validate ZCC codegen does not introduce overflows, preserves integer overflow
semantics, and matches GCC output for cross-validation.

---

## 2.15 Known Bugs & Error Learner Corpus

### Bug Corpus (BUGS.md)

| ID                    | File        | Status | Description                          |
|-----------------------|-------------|--------|--------------------------------------|
| CG-CMP-001            | part4.c     | FIXED  | setle/setge opcode swap              |
| CG-IR-003             | part5.c     | FIXED  | stdout sign-extension in IR mode     |
| CG-GINIT-FLOAT-002    | part4.c     | FIXED  | Float limits global initializer      |
| PP-BLOCKED-LIST-BOUNDS| part0_pp.c  | FIXED  | Macro arg list bounds overflow       |
| PP-MACRO-HIDESET-BARRIER| part0_pp.c | FIXED | Recursive macro infinite expansion  |
| PARSER-001            | part2.c     | FIXED  | Struct tag scope shadowing           |

### Error-Learner Corpus (E-LEARN)

```
E-LEARN-001  Blender glTF API: Assert deprecations from training data only after RNA poll
E-LEARN-002  Blender Ops hasattr: hasattr(bpy.ops.wm, x) always True; use direct call
E-LEARN-003  Blender CLI Exit Code: Always pass --python-exit-code 1 to headless Blender
E-LEARN-004  Self-Swallowing Scopes: Don't catch same exception type raised for assertion
E-LEARN-005  All-Runs Disclosure: Fully disclose all intermediate execution runs
E-LEARN-006  D1-SYSPATH: script-file vs stdin execution resolves sys.path[0] differently
```

---

## 2.16 Verification Gates Protocol

| Gate | Trigger Condition | Command | Pass Criterion |
|------|------------------|---------|----------------|
| Gate 1 | Always (mandatory) | `cmp zcc2.s zcc3.s` | exit 0 (byte-identical) |
| Gate 2 | Codegen changes | Dual-direction interop test | Both directions run correctly |
| Gate 3 | part0_pp.c or part3.c touched | 797-function corpus diff | No unapproved deltas |
| Gate 4 | Lua/SQLite/curl/Doom target symptom | Target harness re-run | Repro + regression pass |
| Gate 5 | All closures (mandatory) | Re-run prior gate evidence | All still pass |

### Gate 2 — Cross-Toolchain Interoperability

```bash
# Direction 1: ZCC-compiled lib + GCC-compiled main
gcc -c zcc_lib.c -o zcc_lib_gcc.o
./zcc main.c -o main_zcc.s && gcc main_zcc.s zcc_lib_gcc.o -o test_d1 && ./test_d1

# Direction 2: GCC-compiled lib + ZCC-compiled main
gcc -c lib.c -o lib.o
./zcc main.c -o main.s && gcc main.s lib.o -o test_d2 && ./test_d2
```

---

## 2.17 Forensic Chain & Tickets

### Key Forensic Documents

| File | Description |
|------|-------------|
| FORENSIC_CORRECTION_2026-04-19.md | Gate discipline; ae6b5ff precedent; master reference |
| FORENSIC_023A_PARSER001.md | Parser-001 scoping bug forensic |
| ZCC_STATUS.md | Current milestone states |
| ZCC_BATTLEPLAN_v1.0.3.md | Active development plan |
| BUGS.md | Known-bug corpus |
| BOOTSTRAP_BASELINES.tsv | Bootstrap baseline lineage (REWRITE marker rows) |
| docs/DEBUG_PROTOCOL.md | 7-phase debug protocol |

### Key Tickets

| Ticket | Description |
|--------|-------------|
| PP-REWRITE-REGRESSION-ROLLBACK.md | Why wholesale pp rewrites fail (commit 8098a94) |
| b299f43-gate-evidence.md | Current evidence template |
| offset-aware-pointer-*.md | Offset-aware pointer arithmetic suite |
| e4-escape-analysis-*.md | Escape analysis planning |

---

# 3. ZKAEDI PRIME — HAMILTONIAN DYNAMICS ENGINE

## 3.1 Canonical Two-Field Equation

```
H_t(x,y) = H_base(x,y)
          + eta * H_{t-1}(x,y) * sigmoid(gamma * H_{t-1}(x,y))
          + eps * N(0, 1 + beta * |H_{t-1}(x,y)|)
```

Where:
- `H_base(x,y)` — Static base potential (walls, obstacles, cost landscape)
- `eta * H_{t-1} * sigmoid(...)` — Recursive self-modifying field evolution
- `eps * N(0, 1 + beta * |H_{t-1}|)` — Adaptive noise with state-dependent variance

### Canonical Parameters

```
eta   = 0.4          # Subcritical recursive field evolution
gamma = 0.3          # Sigmoid saturation control
beta  = 0.1          # Noise variance scaling coefficient
eps   = 0.05         # Base noise amplitude (NOT sigma -- sigma is DEPRECATED)
kick  = 2.0          # Departure event field perturbation
seed  = <explicit>   # Always set explicitly for reproducibility
max_steps = 50000    # Maximum iteration budget
```

### Critical Values

```
eta = 0.4    -- Canonical subcritical regime (field shaping)
eta = 1.05   -- Bifurcation boundary (regime transition)
eta = 0.0    -- Fastest pure pathfinder (no recursion)
```

---

## 3.2 Two-Regime Constraint

CRITICAL: The equation operates in two fundamentally distinct regimes.

```
ONE EQUATION, TWO REGIMES:

FIELD SHAPING REGIME (eta > 0)
  - eta drives recursive field evolution
  - Worldgen, H0 initialization, optimizer behavior
  - eta=0.4 canonical subcritical
  - eta=1.05 bifurcation boundary

NAVIGATION REGIME (scars + eps)
  - Performance from scars + eps tie-break
  - Departure event: H_base[x,y] += kick
  - eta provides NO navigation lift
  - Fastest pure pathfinder: eta=0
  - kick=0 and eps=0 = required non-solving control
```

Forbidden framing: "adaptive self-guided", "recursion-driven", "living reasoning" for navigation.
Required phrasing: "One equation, two regimes: eta shapes fields; scars + eps navigate."

---

## 3.3 FHN Spatiotemporal Chaos (v1-v26)

FitzHugh-Nagumo coupled field research — 26 experimental iterations:

```
FHN Equations:
  du/dt = u - u^3/3 - v + I_ext + D*nabla^2*u
  dv/dt = (u + a - b*v) / tau
```

### Key Discoveries

**Critical Spatial Dimensionality (L_c)**
- System cannot sustain spatiotemporal chaos below spatial scale L_c
- Above L_c, arbitrary complexity emerges from local coupling

**Two-Population Attractor Structure**
- Field points cluster around two competing fixed points
- Transition boundaries exhibit fractal geometry

**Spectral Arrest Diagnostic**
- Power spectral density of activation field u(x,y,t)
- Detects transition from chaotic broadband to periodic narrow-band oscillations

**Relaxation Oscillation Dynamics**
- Long quiet phases interrupted by rapid depolarization events
- Analogous to biological action potentials

---

## 3.4 ZKAEDI PRIME Forge v5 / OMEGA SUPREME

### Six-Stage Sovereign Pipeline

```
Stage 1: FORGE          -- Input ingestion, raw material processing
Stage 2: SILENT REGISTRY-- Cryptographic registration (RECORD*VERIFY*DISPATCH*PRESERVE)
Stage 3: DISPATCH       -- Agent routing and task assignment
Stage 4: PRESERVATION   -- Immutable artifact storage
Stage 5: TABLET         -- Tablet of Unclaimed (pending resolution queue)
Stage 6: AUDIT          -- Post-execution verification and scoring
```

### Anunnaki Agent Roster

| Agent     | Domain        | Responsibilities                              |
|-----------|---------------|-----------------------------------------------|
| ENLIL     | Orchestration | Master dispatcher, task routing               |
| ENKI      | Engineering   | Code generation, compiler work                |
| NINHURSAG | Research      | Knowledge synthesis, documentation            |
| MARDUK    | Security      | Threat assessment, vulnerability analysis     |
| INANNA    | Interface     | User-facing responses, visual output          |
| SHAMASH   | Verification  | Gate execution, evidence collection           |
| NERGAL    | Chaos         | FHN dynamics, randomization                   |
| NINGAL    | Archives      | Forensic chain, historical trace              |

### Glazed Brick Standard (Design Token Palette)

```css
--prime-cyan:    #00f5ff;                /* Primary accent -- electric cyan */
--prime-magenta: #ff00ff;                /* Secondary accent -- deep magenta */
--prime-gold:    #ffd700;                /* Highlight -- sovereign gold */
--prime-dark:    #0a0a1a;                /* Background -- deep space black */
--prime-panel:   rgba(6, 18, 31, 0.82); /* Panel glass */
--prime-muted:   #81a5b4;                /* Muted text */
--prime-error:   #e9fbff;                /* Error state */
--prime-border:  #1e3a5f;                /* Border color */
```

### Scoring Rubric: 5 Primary Factors x 6 Resonance Dimensions

Primary Factors:
1. Correctness   -- Does the artifact do what it claims?
2. Completeness  -- Are all required components present?
3. Coherence     -- Does it form a unified whole?
4. Creativity    -- Does it exhibit novel synthesis?
5. Consequence   -- What is the downstream impact?

Resonance Dimensions:
1. TEMPORAL       -- Timing and urgency alignment
2. SPATIAL        -- Geographic/structural fit
3. ENERGETIC      -- Computational efficiency
4. INFORMATIONAL  -- Signal-to-noise ratio
5. RELATIONAL     -- Inter-system compatibility
6. TRANSFORMATIONAL -- Catalytic potential

### Rarity Tier System

```
TIER 0: COMMON       (score < 40)
TIER 1: UNCOMMON     (40 <= score < 60)
TIER 2: RARE         (60 <= score < 75)
TIER 3: EPIC         (75 <= score < 88)
TIER 4: LEGENDARY    (88 <= score < 96)
TIER 5: MYTHIC       (96 <= score < 100)
TIER 6: SOVEREIGN    (score = 100, perfect resonance)
```

### Five-Law Manifesto

```
1. The Law of Evidence:     No claim exists without an artifact
2. The Law of Lineage:      Every artifact traces to its origin
3. The Law of Immutability: Verified artifacts cannot be modified
4. The Law of Coherence:    The system maintains internal consistency
5. The Law of Evolution:    Each iteration supersedes the last with proof
```

### FTID Acrostic: Forge -- Tablet -- Immutable -- Dispatch

### Silent Registry Entry Schema

```json
{
  "record_id": "FTID-XXXXXXXX",
  "timestamp": "ISO-8601",
  "agent": "AGENT_NAME",
  "action": "RECORD|VERIFY|DISPATCH|PRESERVE",
  "payload_hash": "SHA-256",
  "chain_prev": "PREV_RECORD_ID",
  "status": "PENDING|VERIFIED|DISPATCHED|PRESERVED"
}
```

---

## 3.5 PRIME Gauntlet & CI Verification

```bash
python tools/prime/zkaedi_prime.py --gauntlet

# Expected:
GAUNTLET START
[1/7] Field Evolution Test................... PASS (eta=0.4, steps=1000)
[2/7] Bifurcation Boundary Test.............. PASS (eta=1.05 detected)
[3/7] Navigation Benchmark (eta=0)........... PASS (shortest path found)
[4/7] Two-Regime Constraint Verification..... PASS (nav != field shaping)
[5/7] Scar Mechanism Test.................... PASS (kick=2.0 propagated)
[6/7] Noise Scaling Test (eps)............... PASS (eps=0.05 variance)
[7/7] Full Maze Suite (10 mazes)............. PASS (all solved)
GAUNTLET END: 7/7 PASS
```

CI Workflow: `.github/workflows/prime-gauntlet.yml` on every push to main.

---

## 3.6 Meta-Learning Integration

### MAML (Model-Agnostic Meta-Learning)

```python
def maml_inner_update(model, task_data, alpha=0.01):
    loss = compute_loss(model, task_data.support)
    grads = torch.autograd.grad(loss, model.parameters())
    adapted_params = [p - alpha * g for p, g in zip(model.parameters(), grads)]
    return adapted_params
```

### Meta-SGD (Learnable per-parameter learning rates)

```python
def meta_sgd_update(model, meta_lr, task_data):
    loss = compute_loss(model, task_data.support)
    grads = torch.autograd.grad(loss, model.parameters())
    adapted = [p - lr * g for p, lr, g in zip(model.params, meta_lr, grads)]
    return adapted
```

### Reptile (First-order, no second-order derivatives)

```python
def reptile_update(model, task_models, epsilon=0.1):
    avg_params = mean([m.parameters() for m in task_models])
    for p, avg_p in zip(model.parameters(), avg_params):
        p.data += epsilon * (avg_p - p.data)
```

### PRIME-MAML Coupling

The PRIME energy field H_t provides the exploration landscape for task sampling:
- High-energy regions -> novel task domains (exploration)
- Low-energy regions -> well-understood domains (exploitation)
- Gradient of H_t guides task curriculum scheduling

---

## 3.7 Ensemble Fusion Engine

Models fused: `gemma-7b-solidity-energy-signatures` + `solidity-vuln-auditor-7b`

```python
def prime_ensemble_fuse(model_a_output, model_b_output, H_field):
    agreement = jaccard_similarity(model_a_output.findings, model_b_output.findings)
    w_a = sigmoid(H_field.confidence_a)
    w_b = sigmoid(H_field.confidence_b)
    fused = [(f, (w_a if f in model_a else 0) + (w_b if f in model_b else 0))
             for f in union(model_a_output.findings, model_b_output.findings)]
    return sorted(fused, key=lambda x: x[1], reverse=True)
```

---

# 4. SYSTEMS VISUAL OBSERVATORY

## 4.1 SPH Fluid Dynamics Solver

Smoothed Particle Hydrodynamics on HTML5 Canvas:

```
rho_i = sum_j m_j W(r_ij, h)                    # Density
p_i   = k(rho_i - rho_0)                        # Pressure (equation of state)
a_i   = -sum_j m_j(p_i/rho_i^2 + p_j/rho_j^2)  # Pressure force
      + mu * sum_j m_j (v_j - v_i)/rho_j        # Viscosity force
      + g                                         # Gravity
```

Features:
- O(N) Spatial Grid Bucketing -- cells of size h, only 27 adjacent cells queried
- AVX-512 Register Lane Visual Overlays -- particles colored by SIMD lane
- Real-time control: h, k, mu, rho_0, gravity vector
- Performance: 1000 particles @ 60 FPS; 5000 particles @ 15 FPS

---

## 4.2 CPU Pipeline & Cache Profiler

Superscalar execution pipeline simulator:

```
Pipeline: IF -> ID -> EX -> MA -> WB
Features:
- L1 Cache Split-Load Detection (64-byte boundary, 2-cycle penalty)
- Data Hazard Bubbles (RAW, WAR, WAW visualization)
- Out-of-Order Execution Window (ROB occupancy)
- Branch Predictor State (bimodal 2-bit saturating counters)
Controls: Sample programs, step instruction-by-instruction, pipeline depth 3-12, OOO toggle
```

---

## 4.3 Compiler Register Allocator Sandbox

Interactive Chaitin-Briggs graph-coloring allocator:

```
Algorithm:
1. Live Range Analysis
2. Interference Graph Construction
3. Graph Coloring (K physical registers)
4. Spilling (insert load/store when not K-colorable)
5. Coalescing (merge non-interfering copy-related nodes)

Visualization:
- Force-Directed Graph Physics
- Color Assignment Animation
- Spill Code Injection Tracking
- K-Register Configuration slider (2-16)
```

---

## 4.4 Hamiltonian Orbit Solver

```
Potentials: Newtonian (-GM/r), Kepler, Harmonic (kr^2/2), Lennard-Jones, Custom
Integrators: RK4 (high accuracy), Symplectic Verlet (energy-conserving)
Visualization: p-q phase space, Poincare section, HSL quantum wavefunction grid
```

---

## 4.5 SDF Shader Compiler & Risk Auditor

`tools/zcc_sdf_compiler.py` (165 KB standalone):

```
Capabilities:
- Raymarching Loop Bound Verification (MAX_STEPS <= 256, step_size >= MIN_DIST)
- Precision Risk Detection (float operations losing precision in depth buffers)
- Memory Safety (no unbounded array accesses in shader loops)
- Safety Ceiling Enforcement (auto-clamp user-set parameters)
- Performance Risk Scoring (GPU frame time estimate from shader complexity)

Risk Categories:
CRITICAL: Infinite loop potential
HIGH:     Precision loss > 0.1% at max depth
MEDIUM:   Step count > 128 (frame time risk)
LOW:      Non-optimal but safe patterns
INFO:     Style suggestions
```

---

# 5. DARK FOREST SUITE — SECURITY & MEV ENGINE

## 5.1 The Dark Forest

Interactive Ethereum predator/prey mempool ecosystem:

```
World: Procedural starfield (2000 stars), nebula clouds, parallax layers
Pools: Gravity wells with orbital mechanics around AMM liquidity pools
Ships: MEV Predator Ships (AI-controlled sandwich attackers)
       Searcher Ship (player-controlled, 4 weapon systems)

Weapons:
  [Q] Flashbots Shield   -- Private mempool routing (3-second cooldown)
  [W] Gas Spike          -- Rapid gas price escalation weapon
  [E] Sandwich Detector  -- Reveal incoming sandwich attack vectors
  [R] Bundle Cannon      -- Submit competing Flashbots bundle

Audio:  Procedural ambient (sine oscillators tuned to block time, reverb from congestion)
Radar:  200x200 minimap with threat vectors and pool depth heatmap
```

---

## 5.2 EVM Exploit Theater

Interactive proxy vulnerability visualizer:

```
Exploit Waves:
1. Reentrancy Waves       -- Recursive call injection, balance drain tracking
2. SSTORE Attacks         -- Storage slot manipulation
3. Delegatecall Misuse    -- Context hijacking through delegatecall chains
4. Selfdestruct Waves     -- Contract destruction cascades
5. Signature Replay       -- Cross-chain signature replay attacks

Particle Physics: Defense barriers scatter exploit particles
Bytecode Rain: Matrix-style falling EVM bytecode (color-coded by opcode category)
  Blue:   PUSH/POP/DUP    Red:    JUMP/JUMPI
  Green:  CALL variants   Yellow: SSTORE/SLOAD    Purple: LOG0-LOG4
```

---

## 5.3 Mempool Warfare Simulator

```
Transaction Flow Physics -- unconfirmed txs as particles
Sandwich Bot Logic       -- intercept high-slippage swaps
Frontrunning Engine      -- race to copy pending transactions
Block Building           -- drag-and-drop block assembly from mempool
P/L Tracking:
  Successful sandwiches: +0.842 ETH
  Failed sandwiches:     -0.023 ETH
  Gas costs:             -0.031 ETH
  Net:                   +0.788 ETH
```

---

## 5.4 Contract Scanner

```
Detection Patterns:
- Reentrancy (CEI violation, external calls before state updates)
- Integer Overflow (pre-SafeMath arithmetic without bounds checking)
- Access Control (missing onlyOwner or role guards on sensitive functions)
- Unguarded Storage (public state vars that should be private)
- tx.origin Auth (authentication using tx.origin instead of msg.sender)
- Selfdestruct Exposure (unprotected selfdestruct() calls)
- Delegatecall Misuse (delegatecall with untrusted calldata/target)

CVSS v3.1 Scoring: CRITICAL (9.0-10.0) | HIGH (7.0-8.9) | MEDIUM (4.0-6.9) | LOW (0.1-3.9)
```

---

## 5.5 Trace Analyzer

```
Input:  Tenderly trace JSON, Etherscan debug trace, Hardhat trace, Foundry forge trace
Output: Interactive call graph, gas per opcode, storage map, log timeline, revert reason
```

---

## 5.6 Scenario Forge

```json
{
  "name": "Euler Finance Recreation",
  "pools": [{"token": "USDC", "tvl": 197000000}],
  "exploiter": {"type": "flash_loan_reentrancy", "profit_target": 197000000},
  "defenders": [{"type": "pause_guardian", "response_blocks": 5}],
  "narrative": ["Setup", "Flash Loan", "Reentrancy", "Profit", "Aftermath"]
}
```

---

## 5.7 Omni Parser

```
Input:  Solidity source, EVM bytecode, ABI JSON, transaction traces, mempool dumps, JSON logs
Output: Unified AST + call graph schema (nodes, edges with types and values)
```

---

## 5.8 Vulnerability Topology Report

`brain/014e74b0/vulnerability_topology_report.md`:
- Surface area classification: 6 domains (DeFi, Bridges, Browser, Orchestration, CI/CD, Extension)
- Risk heat map per domain
- Worker analysis: extracted metadata from 40+ worker ZIP archives

---

## 5.9 Cloudflare Worker GLB Drop Zone

Interactive HTML Cloudflare Worker for safe 3D file processing:

```
Features:
- Drag-and-drop GLB/GLTF upload
- Client-side BVH computation (no server upload)
- VRAM budget estimation
- Polygon/vertex count display
- Material and texture inventory
- Export as optimized GLTF

Security:
- CSP: default-src 'self'; script-src 'self' 'wasm-unsafe-eval'
- Subresource Integrity on all external scripts
- No API keys in client code
- All processing local (no file upload)
```

---

# 6. 3D MESH, RAYTRACING & VFX PIPELINE

## 6.1 ZKAEDI 3D Tools Suite

`tools/zkaedi_3d_tools.py` (64 KB standalone Python toolkit):

### Core Algorithms

**Voronoi Partitioning**
```python
def voronoi_partition(vertices, k):
    '''Partition 3D mesh vertices into k Voronoi regions using Lloyd relaxation.'''
    centroids = vertices[np.random.choice(len(vertices), k, replace=False)]
    for _ in range(max_iterations):
        distances = cdist(vertices, centroids)
        assignments = np.argmin(distances, axis=1)
        new_centroids = [vertices[assignments==i].mean(0) for i in range(k)]
        if np.allclose(centroids, new_centroids, atol=1e-6):
            break
        centroids = np.array(new_centroids)
    return assignments, centroids
```

**KMeans Vertex Clustering**
```python
def kmeans_vertex_cluster(vertices, normals, k, weights=(0.7, 0.3)):
    '''Cluster vertices using weighted position + normal features for LOD generation.'''
    features = np.hstack([vertices * weights[0], normals * weights[1]])
    return sklearn_kmeans(features, n_clusters=k).labels_
```

**Skeletal Weight Assignment**
```python
def compute_skinning_weights(vertices, bones, falloff='linear'):
    '''Compute per-vertex bone influence weights using distance-based falloff.'''
    weights = np.zeros((len(vertices), len(bones)))
    for i, bone in enumerate(bones):
        dist = np.linalg.norm(vertices - bone.center, axis=1)
        weights[:, i] = falloff_fn(dist, bone.radius, falloff)
    return normalize_rows(weights)
```

### VRAM Compliance Auditor
```python
def audit_vram_budget(mesh, textures, target_vram_mb=256):
    '''Estimate VRAM usage and flag budget violations.'''
    # Returns: VRAMAuditReport(vertex_buffer_size, index_buffer_size,
    #           texture_memory_total, recommended_lod_levels, exceeded_budget)
```

---

## 6.2 ZK3D SAH BVH Unpacker

Surface Area Heuristic Bounding Volume Hierarchy for raytracing:

```
Files:
  zkaedi_bvh_sah.cpp      -- C++ SAH BVH construction algorithm
  zk3d_bvh_unpacker.cpp   -- Binary BVH file parser and mesh extractor

SAH Cost Function:
  cost(split) = TRAVERSAL_COST
              + (left_SA / parent_SA) * left_count * INTERSECT_COST
              + (right_SA / parent_SA) * right_count * INTERSECT_COST

Rust Bindings (maturin):
  build_bvh_from_mesh(vertices, indices, max_leaf_prims) -> BVHNode
```

---

## 6.3 VFX Asset Processing Microservice

`scratch/vfx_pipeline/app/main.py` (26 KB FastAPI):

```
API Endpoints:
  POST /api/v1/process          -- Batch process uploaded 3D assets
  GET  /api/v1/status/{job_id}  -- Check processing job status
  GET  /api/v1/download/{id}    -- Download processed asset
  POST /api/v1/validate         -- Validate asset without processing
  GET  /api/v1/health           -- Service health check

Processing Pipeline:
  Upload GLB/GLTF
    -> Validation Gate (mesh integrity, texture formats, material count)
    -> BVH Construction (SAH algorithm)
    -> LOD Generation (3 levels: 100%, 50%, 25%)
    -> Texture Compression (BC7 desktop, ASTC mobile)
    -> GLTF Export (draco-compressed, embedded textures)
```

---

## 6.4 Mesh Quality Gate & Dataset Parser

```
scratch/5321fdc4/mesh_quality_gate.py:
  - Watertight mesh check (no open edges)
  - Non-manifold geometry detection
  - UV mapping validity (no overlapping UVs)
  - Normal vector consistency
  - LOD degradation quality score

scratch/vfx_pipeline/scratch/parse_meshy_dataset.py:
  - Batch download of Meshy.ai generated meshes
  - Metadata extraction (prompt, model ID, generation time)
  - Automatic quality gate filtering
  - CSV manifest generation
```

---

## 6.5 Meshy/Tripo3D Plugin Integration

```
H:\_studio_tripo3d\meshy-unreal-plugin-v0.1.3.zip
  - Generate 3D assets from text prompts inside UE5 editor
  - Automatic material assignment
  - LOD auto-generation
```

---

## 6.6 Blender Animation Pipeline

Session 067eabf5 -- Processed `C:\Users\zkaed\OneDrive\Documents\Untitled.blend`:

```
Generated Stills:
  closeup_frame_0001.png      -- Opening frame
  closeup_frame_0120.png      -- Mid-animation
  closeup_frame_0240.png      -- Late animation
  h1_reveal.png               -- H1 reveal shot
  h1_reveal_crane_swing.png   -- Crane camera swing
  h2_wheel_quarter_spin.png   -- Wheel mechanics
  h2_wheel_spin.png           -- Full wheel spin
  h3_Z_swing.png              -- Z letter swing
  h4_Z_letter_swing.png       -- Final Z reveal

Generated Video: grok-video-9fff86bf-b604-4cad-9102-5d2b5c9b80c9.mp4

E-LEARN Blender Rules:
  E-LEARN-001: RNA poll for glTF API deprecation, don't trust training data alone
  E-LEARN-002: hasattr(bpy.ops.wm, x) always True; use direct call verification
  E-LEARN-003: Always pass --python-exit-code 1 to headless Blender
```

---

## 6.7 Fleet Pipeline Automation

Session 5321fdc4:

```
scratch/run_clean_fleet_pipeline.py   -- End-to-end asset processing pipeline
scratch/process_new_assets.py         -- Batch processor for new asset ingestion
scratch/inspect_new_assets.py         -- Asset inspection and quality audit
scratch/populate_lowpoly.py           -- Automated LOD low-poly population
scratch/test_form.py                  -- Form validation testing
```

---

# 7. INTERACTIVE WEBGL 3D VISUALIZER SUITE

All 13 visualizers are self-contained HTML files stored in brain artifact directory
`a4147fa4-50cb-49e2-91cc-097947505b61/artifacts/`.
Each contains embedded JavaScript WebGL 2.0 code with no external dependencies.

| Scene | File | Description |
|-------|------|-------------|
| 01 | 3d_01_mobius.html | Non-orientable Mobius strip; demonstrates topology |
| 02 | 3d_02_gravity_well.html | Central potential gravity well; elliptic/parabolic/hyperbolic orbits |
| 03 | 3d_wormhole.html | Einstein-Rosen bridge; Morris-Thorne metric, ray-marched SDF |
| 04 | 3d_04_hyperboloid.html | One/two-sheeted hyperboloid deformation; eccentricity morphing |
| 05 | 3d_05_ripple.html | 2D/3D wave equation ripple interference from multiple point sources |
| 06 | 3d_06_dna_helix.html | 3D molecular DNA double helix; animated base pairing |
| 07 | 3d_07_trefoil_knot.html | Topological trefoil knot; parallel transport vector field |
| 08 | 3d_08_sine_ocean.html | Gerstner wave superposition; 8 wave components, realistic ocean |
| 09 | 3d_09_supernova.html | 50,000-particle explosion; Maxwell-Boltzmann velocity distribution |
| 10 | 3d_gravity_vortex.html | Accretion disk vortex dynamics; gravitational + rotational forces |
| 11 | 3d_audio_planet.html | Sound-reactive sphere; microphone FFT deforms surface |
| 12 | 3d_waterfall_spectrogram.html | Real-time 3D frequency waterfall; rolling spectral landscape |
| 13 | 3d_hamiltonian_torus.html | PRIME orbits mapped onto torus; phase-space (p,q) -> (theta,phi) |

### Clifford & De Jong Attractors (Session c6935108)

```
Clifford:  x_{n+1} = sin(a*y_n) + c*cos(a*x_n)
           y_{n+1} = sin(b*x_n) + d*cos(b*y_n)
           5 million points; density-based SVG coloring

De Jong:   x_{n+1} = sin(a*y_n) - cos(b*x_n)
           y_{n+1} = sin(c*x_n) - cos(d*y_n)

Files: clifford.svg, dejong.svg, dynamics_viz.html, vr_stereo_viz.html
```

---

# 8. BLOCKCHAIN, NFT & LEDGER SYSTEMS

## 8.1 Immutable Cryptographic Ledger

`zkaedi-lab/lineage/immutable_ledger.py` (28 KB append-only ledger):

```python
class ImmutableLedger:
    def append(self, record):
        prev_hash = self.entries[-1].hash if self.entries else GENESIS_HASH
        record.hash = sha256(f"{record.content}{prev_hash}").hexdigest()
        self.entries.append(record)
        return record.hash

    def verify(self):
        for i, entry in enumerate(self.entries[1:], 1):
            expected = sha256(f"{entry.content}{self.entries[i-1].hash}").hexdigest()
            if entry.hash != expected:
                return False
        return True
```

Retention: 30-day sliding window for operational entries; permanent for milestones.

---

## 8.2 NFT Minting Pipeline (Sepolia Testnet)

```solidity
// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;
import "@openzeppelin/contracts/token/ERC721/extensions/ERC721URIStorage.sol";

contract ZKAEDIVault is ERC721, ERC721URIStorage {
    uint256 private _nextTokenId;
    constructor() ERC721("ZKAEDI Vault", "ZKV") {}
    function mint(address to, string memory tokenURI) public returns (uint256) {
        uint256 tokenId = _nextTokenId++;
        _safeMint(to, tokenId);
        _setTokenURI(tokenId, tokenURI);
        return tokenId;
    }
}
```

Network: Sepolia testnet | Provider: Alchemy RPC | NFT images: 3D visualizer renders

---

## 8.3 ZKAEDI Trading Dashboard

`scratch/ZKAEDI_PRIME_SUITE/zkaedi-trading-dashboard/`
- Real-time price feeds (Alchemy WebSocket)
- PRIME field overlay on price charts
- MEV opportunity detection
- Flashbots bundle history
- Production bundle: `dist/assets/index-BFqYzCe1.js` (229 KB minified)

---

## 8.4 ZKGhost Vault & LSB Steganography Engine

`scratch/zkghost_source/zkghost-vault-modularized/`

```
Architecture:
  VaultRegistry          -- Root directory and agent registry management
  GhostEmulator          -- Event-driven steganographic payload emulator
  ExtractionPipeline     -- End-to-end LSB extraction pipeline
  LSBGhostExtractor      -- Least-Significant Bit covert channel extractor
  AgentBase              -- Base class for all vault agents
  HealthService          -- Service health monitoring
  RetryPolicy            -- Exponential backoff retry mechanism
  EventBus               -- Pub/sub event routing

LSB Extractor:
  - 1-bit, 2-bit, 4-bit LSB channels
  - RGB and RGBA images (PNG, BMP, TIFF)
  - Sequential and pseudo-random pixel ordering
  - AES-256-CBC payload decryption

Tests (95%+ coverage):
  test_vault.py, test_emulator.py, test_pipeline.py, test_lsb_extractor.py,
  test_recovery.py, test_health.py, test_agent_base.py, test_registry.py,
  test_analyzers.py, test_events.py
```

---

# 9. SAFE RESEARCH TOOLKIT & TELEMETRY

## 9.1 SRDF Anthropic Safe Research Toolkit

`F:\__NSFW_VIDEOS\SRDF_Anthropic_Safe_Research_Toolkit_v0.2_hardened\`
- Hardened research investigation runner (v0.2)
- `investigations/anthropic-investigation-001.json` -- Structured investigation record

## 9.2 Online Canary Gate

`zkaedi-lab/runner/online_canary_gate.py` -- Binary hash verification against ledger:
```python
def verify(self, binary_path):
    current_hash = sha256_file(binary_path)
    registered_hash = self.ledger.get_hash(binary_path)
    if current_hash != registered_hash:
        return CanaryResult(status=TRIPPED, registered=registered_hash, current=current_hash)
    return CanaryResult(status=CLEAN)
```

## 9.3 Agent Mutator & Lineage Tracking

`zkaedi-lab/runner/zkaedi_agent_mutator.py` -- Mutation operators:
Point mutation | Operator swap | Branch inversion | Dead code injection | Variable rename
All mutations registered in immutable_ledger.py with pre/post hash + behavioral delta.

## 9.4 Ledger Security Test Suite

`zkaedi-lab/tests/test_ledger_security.py` (68 KB):
Append integrity | Chain verification | Hash collision resistance |
Concurrent write safety | Replay attack prevention | Timing attack mitigation

## 9.5 ZKAEDI Suno WebSocket Bridge

`zkaedi_suno_ws_bridge.py` -- PRIME field values -> Suno.ai music parameters:
- H_t amplitude -> tempo BPM
- Field gradient -> key/mode selection
- eps noise level -> reverb/decay
- eta recursion depth -> harmonic complexity

## 9.6 Gemini Prime Probe

`scratch/gemini_prime_probe.py` (22 KB):
Response consistency | Temperature vs field energy | Token sampling | Context utilization

---

# 10. BROWSER EXTENSIONS & EDGE TOOLING

## 10.1 ZKAEDI Recovery Extension

`H:\agents\zkaedi-recovery-extension\` -- Microsoft Edge Manifest v3 extension:
- Session Recovery (restore conversation state after browser restart)
- Context Persistence (maintain PRIME field state across navigation)
- Sidebar Mode (persistent panel, doesn't close on tab switch)
- Zero External Calls (all state in chrome.storage.local)

User requirement: "I need it to load in my sidebar I can't click out of the extension or it disappears"
Solution: sidebarAction API with persistent panel.

## 10.2 Edge Sidebar Plugin

- Packed: `zkaedi-recovery-extension.crx`
- Unpacked: Drop directory into edge://extensions developer mode
- Compact 320px width UI optimized for sidebar context

## 10.3 Anti-Gravity Engine Worker

`scratch/ZKAEDI_PRIME_SUITE/anti-gravity-engine/worker/anti-gravity-engine.min.js` (110 KB):
- PRIME field computation in Cloudflare Worker context
- GLB 3D mesh processing via WebAssembly
- BVH construction offloaded to Worker thread

## 10.4 Cloudflare Worker GLB Drop Zone

```javascript
async function processGLB(file) {
    const buffer = await file.arrayBuffer();
    const mesh = await new GLTFParser(buffer).parse();
    const bvh = buildBVH(mesh.geometry, { maxLeafPrims: 4 });
    return { vertices: mesh.geometry.vertices.length / 3,
             triangles: mesh.geometry.index.length / 3,
             vram_mb: estimateVRAM(mesh),
             bvh_depth: bvh.maxDepth };
}
```

---

# 11. BIOMEDICAL & GENOMIC SCIENCE ENGINE

Integrated plugin suite connecting to 25+ biomedical APIs:

## Variant Analysis APIs
| API | Purpose |
|-----|---------|
| AlphaGenome | Non-coding variant effects (RNA-seq, DNASE, ChIP, TF) |
| AlphaFold DB | Structural confidence (pLDDT), domain boundaries |
| gnomAD | Allele frequencies, gene constraint (pLI, LOEUF) |
| ClinVar | Pathogenicity classifications (Pathogenic, VUS, Benign) |
| dbSNP | rsID <-> genomic coordinates <-> HGVS |
| Ensembl VEP | Variant consequence and effect prediction |

## Protein Analysis APIs
| API | Purpose |
|-----|---------|
| UniProt | Protein metadata, function, sequences |
| InterPro | Domain/family annotation (14 databases unified) |
| STRING | Protein-protein interaction networks |
| Human Protein Atlas | Spatial expression across tissues |
| PDB | 3D structure download and search |
| Foldseek | 3D structural similarity search |

## Gene Expression & Regulatory APIs
| API | Purpose |
|-----|---------|
| GTEx | RNA expression across 54 tissue types |
| ENCODE cCREs | cis-regulatory element registry |
| UCSC | PhyloP/phastCons conservation scores |
| JASPAR | Transcription factor binding profiles |
| UniBind | Validated TF binding site coordinates |

## Drug & Literature APIs
| API | Purpose |
|-----|---------|
| ChEMBL | Bioactive molecules, IC50/Ki, drug mechanisms |
| PubChem | Chemical structures, bioactivity data |
| OpenFDA | Adverse events, recalls, approvals, NDC lookups |
| OpenTargets | Target-disease associations, tractability |
| PubMed | Literature search, abstract retrieval |
| EuropePMC | Open-access full text download |
| bioRxiv/medRxiv | Preprint browsing and filtering |
| arXiv | Physics/CS/math preprint search |
| OpenAlex | Scholarly database, h-index, citation counts |
| ClinicalTrials.gov | Trial search, eligibility, status |

---

# 12. ORCHESTRATION MESH & MULTI-AGENT SYSTEMS

## 12.1 ZKAEDI Orchestration Mesh

`tools/zkaedi_orchestration_mesh.py` (117 KB):

```python
class TaskGraph:
    '''DAG representing inter-task dependencies.'''
    def execute(self, executor):
        ready = self.get_ready_tasks()
        while ready:
            futures = {t: executor.submit(self.nodes[t].fn) for t in ready}
            for task, future in futures.items():
                self.mark_done(task, future.result())
            ready = self.get_ready_tasks()

class MeshBus:
    '''Inter-agent message bus with topic routing.'''
    def publish(self, topic, message):
        for subscriber in self.subscriptions.get(topic, []):
            subscriber.queue.put(message)
```

Features: Pub/Sub Messaging | Background Job Scheduler | Priority Queue | Dead Letter Queue

## 12.2 Agent Generator App

`scratch/agent_generator_app/app.py` (46 KB):
- Drag-and-drop skill composition UI
- Agent persona configuration
- Automated SKILL.md generation
- Agent deployment to config/skills/
- Test harness generation
- Auto-prompt randomizer for agent testing

## 12.3 Antigravity Edge Suite Intelligence Report

`brain/014e74b0/antigravity_edge_suite_intelligence_report.md`:
- Cloudflare Worker vulnerability surface mapping
- Browser extension attack vectors
- Worker ZIP archive metadata analysis (40+ workers)
- HNF (Hypernetwork Fragment) inspection results
- Dynamics testing across 10 experimental configurations

---

# 13. KNOWLEDGE BASE & FORENSIC CHAIN

## Active Knowledge Items

| KI Name | Key Artifact | Description |
|---------|-------------|-------------|
| zkaedi_prime_forge_v5_visual_canon | forge_v5_visual_canon.md | PRIME Forge v5 architecture |
| zcc_sqlite_float_limits | zcc_sqlite_float_limits.md | SQLite stabilization & float limits |
| zcc_ir_bridge_pipeline_state | ir_bridge_pipeline_state.json | IR Bridge architecture v1.0.0 |
| zcc_ir_bridge | zcc_ir_bridge_segfault_resolution.md | IR Bridge segfault resolution |
| zcc_doom_milestone | zcc_doom_bugs.md | Doom engine 7 discoveries |
| cg_cmp_001_setle_setge_opcode_swap | cg_cmp_001.md | setle/setge swap bug |

All KIs: `C:\Users\zkaed\.gemini\antigravity-ide\knowledge\`

---

# 14. OPERATIONAL PROTOCOLS & RULES

## 14.1 AGENTS.md — Execution Contract

Source: `C:\Users\zkaed\.gemini\antigravity\brain\AGENTS.md`

Core mandate: Before ANY change to ANY file — regardless of framing as bug fix, feature,
hardening, cleanup, or quick question — Phase 0 MUST complete. No exceptions. No shortcuts.

## 14.2 Phase 0 — Read Before Touch Protocol

```bash
# 0.1 Repository forensic snapshot
git log --all --oneline --format='%h %ad %s' --date=short | head -30
ls FORENSIC*.md tickets/*.md 2>/dev/null

# 0.2 Read IN FULL: most recent FORENSIC_*.md + 5 newest tickets/*.md

# 0.3 Candidate file historical trace
git log -p --follow -- <file> | head -300
# If fix exists historically: STOP AND REPORT. Do not proceed.

# 0.4 Baseline bootstrap gate
make selfhost | tail -5
# Must see: SELF-HOST VERIFIED (assembly identical)

# 0.5 Phase 0 verdict
BASELINE:              GREEN | RED
SYMPTOM-IN-HISTORY:    YES (commit <sha>) | NO
FORENSIC-LATEST-SHA:   <sha>
PROCEED:               YES | NO
```

## 14.3 Verification Gate System

| Gate | Trigger | Command | Pass Criterion |
|------|---------|---------|----------------|
| Gate 1 | Always | `cmp zcc2.s zcc3.s` | Byte-identical (exit 0) |
| Gate 2 | Codegen changes | Dual-direction interop test | Both directions run correctly |
| Gate 3 | part0_pp.c or part3.c | 797-function corpus diff | No unapproved deltas |
| Gate 4 | Lua/SQLite/curl/Doom | Target harness re-run | Repro + regression pass |
| Gate 5 | All closures | Re-run prior gate evidence | All still pass |

## 14.4 Commit Body Template

```
<scope>(<area>): <action + result>, no spin

Goal:    <one sentence intended result>
Outcome: <one sentence verified on-disk truth>

Root cause:
- <specific invariant violated>
- <where/why violation happened>

Patch summary:
- <file>: <minimal change>

Gate 1: <PASS|FAIL> via cmp zcc2.s zcc3.s
<raw output>

Gate 2: <PASS|FAIL|N/A>
<raw output or strict N/A reason>

Gate 3: <PASS|FAIL|N/A>
<raw output or N/A>

Gate 4: <PASS|FAIL|N/A>
<raw output or N/A>

Gate 5: <PASS|FAIL>
<raw output>

Bugs caught mid-gate: <description> | None
Hygiene/deferred: HYGIENE-<id>: <description> | None
Forensic notes: <history findings>
Residual risk: <explicit bounded risk> | None
```

## 14.5 Hard Prohibitions

```
1. No wholesale rewrites of part0_pp.c, part3.c, part4.c -- diff < 50 lines
2. No gate-pass claims without raw output pasted verbatim
3. No testing source-failure against pre-preprocessed amalgams
4. No "error messages changed" = progress inference
5. No commit subject more confident than evidence justifies
6. No git reset --hard or git push --force without explicit authorization + reflog snapshot
```

## 14.6 Stop Conditions

Stop implementation and report if any is true:
- Phase 0.3 reveals fix already exists in history
- Baseline RED and failure is not authorized target
- Any gate fails after edits
- Required binary/artifact missing and cannot be regenerated
- Conclusion language is probabilistic ("should", "likely", "in theory")
- User request conflicts with checked-out tree -- tree wins
- Diff scope exceeds approved bounds

When stopped:
```
STATUS: BLOCKED (INSUFFICIENT EVIDENCE)
```

---

# 15. SESSION INDEX — 142 CONVERSATIONS

## Most Active Sessions (by file size / step count)

| Session ID | DB Size | Steps | Primary Domain |
|-----------|---------|-------|----------------|
| 8148c94e | 42.34 MB | 3,328 | ZCC ABI mismatch (mismatch_seed662.c) |
| 39e0d7a8 | 64.99 MB | 6,592 | ZK3D BVH, Rust bindings, UI animations |
| 7b96883d | 73.56 MB | 7,856 | ZXR CFG topology, warzone analysis |
| 1dd7c79a | 61.06 MB | 2,726 | ZCC self-host verification, macro folding |
| 72f2da15 | 138.49 MB | 5,620 | Encyclopedia tab UI, prompt generator |
| 5321fdc4 | 65.21 MB | 6,595 | Fleet pipeline, mesh quality gate |
| 416b7d8a | 77.40 MB | 8,129 | Primary ZCC development session |
| bba4d798 | 62.31 MB | 5,354 | SQLite under ZCC, part4.c, macros |
| 4acc535c | 44.73 MB | 1,277 | ZCC + SQLite integration |
| cba8cd53 | 25.14 MB | 1,398 | Bug hunt post-mortem |
| cc5b687d | 29.66 MB | 1,624 | SAH BVH, zcc_network.c, daemons |
| 70153340 | 29.57 MB | 1,853 | Island audit, video inspection |
| eb78ef6c | 30.92 MB | 2,077 | ZCC codegen work |
| d7a31de1 | 47.65 MB | 3,972 | ZCC development |

## All Known Session IDs

```
26d6f314  8148c94e  39e0d7a8  e8ff49b1  571da055  5aa6e29c
cc5b687d  bba4d798  7b96883d  1dd7c79a  72f2da15  7db3dc8b
784fd761  05958d7b  64532837  3fb2e409  70153340  5321fdc4
416b7d8a  cba8cd53  4acc535c  eb78ef6c  d7a31de1  d6665b2a
0da2e327  c6935108  dd489b5f  2a304443  58b471bc  55d36d50
067eabf5  a4147fa4  7c9c98c1  7d20bba7  014e74b0  42435e75
5198abf3  b669ba41  7c9c98c1  412f81be  2a2d9fc0  2f7095cb
03f1ba75  [103 additional sessions]
```


### 15.2 Protobuf Session Snapshots (.pb) — 101 Files (1.33 GB)

While SQLite `.db` files store indexed relational step data, Antigravity core preserves complete binary state snapshots as `.pb` (Protobuf) files in `C:\Users\zkaed\.gemini\antigravity\conversations\`.

#### Largest Session Snapshots by Disk Footprint

| Snapshot File | Size (MB) | Domain / Extracted Artifact Context |
|---------------|-----------|-----------------------------------|
| `7a99fc25-3b21-42ce-8afb-624997c0dbd9.pb` | 151.75 MB | 3D Mesh & VFX Fleet Pipeline |
| `8e84b478-3539-4a84-827f-7d9b306889e9.pb` | 56.67 MB  | ZKAEDI PRIME Hamiltonian Solvers |
| `dc138f3d-1554-4e07-afbf-0d5ba61d2b42.pb` | 38.32 MB  | ZCC Compiler Frontend & IR Bridge |
| `72463d47-9881-41ee-9053-afb87a0ce065.pb` | 36.78 MB  | 3D WebGL Shader Observatory |
| `d78f678f-9061-4684-90b2-2fbe8ae24a10.pb` | 35.50 MB  | ZCC Self-Host Verification |
| `c2dd5297-04ec-4e3b-a4c6-d69615c8d9bf.pb` | 34.03 MB  | Dark Forest EVM Exploit Theater |
| `48a4070a-1d97-42e2-806f-fc2fd0f072e1.pb` | 32.46 MB  | 3D Mesh / Voronoi & KMeans |
| `ad18a6c6-3127-4cc0-948d-2ef70fc5eeb6.pb` | 32.07 MB  | SQLite 3.53.1 Amalgamation Integration |
| `5152096a-f124-467c-ab29-a5f8be988607.pb` | 31.78 MB  | ZKGhost LSB Steganography Vault |
| `3b4ae260-5167-4361-b1a6-906c148cbb56.pb` | 31.65 MB  | SPH Fluid Dynamics & AVX-512 Solver |
| `0336b160-6877-41ca-89af-0433c173624e.pb` | 30.14 MB  | Compiler Register Allocator Sandbox |
| `07663a55-9f31-4559-aac2-0e36efef9fd9.pb` | 29.66 MB  | 3D Mesh SAH BVH Raytracer |
| `94dbc2d2-d120-4fa9-8507-f586f718ad36.pb` | 28.38 MB  | ZCC Doom Engine Milestone |
| `f639e800-0f39-4add-bac7-64bb18b83e9d.pb` | 28.30 MB  | Multi-Agent Orchestration Mesh |
| `3dad2464-2593-412c-a250-74b37d6f74b9.pb` | 28.29 MB  | Biomedical Genomic API Pipeline |
| `bf59e300-dbf6-43f3-a29e-84e77a4bee45.pb` | 27.85 MB  | Sepolia NFT Minting & Trading Dashboard |
| `ff5c57bb-744b-4978-a417-8289ac042666.pb` | 27.36 MB  | SRDF Safe Research Toolkit |
| `7ac2ee0b-f4ce-4d33-852a-f25a6c2797ff.pb` | 27.20 MB  | Edge Sidebar & Recovery Extension |
| `82efc58a-ce10-4db5-975d-ffa298d7cc0b.tmp`| 27.20 MB  | Active Transient State Snapshot |
| `422705b7-5672-4ec2-a703-5a9b54211b38.pb` | 27.07 MB  | ZCC Preprocessor Hideset & Bounds |
| `4cc0e9e2-d33a-4930-8e91-a064cada1ac1.pb` | 26.40 MB  | Float Gauntlet & Precision Engine |

*Total Disk Footprint of Protobuf Snapshots: 1,334.08 MB (101 files)*



### 15.3 Cherry-Picked Database Sessions & Standout Deliverables

A direct forensic extraction across all 143 SQLite `.db` conversation files yielded 136 active sessions containing direct prompt history, tool invocations, and code deliverables:

- **ZCC Compiler Engine**: 42 sessions covering preprocessor hideset, parser-001, float gauntlet, IR bridge SSA, and Doom/SQLite self-host milestones.
- **WebGL 3D Visualizer Observatory**: 19 sessions containing the 13 standalone HTML WebGL 2.0 physics visualizers, Clifford/DeJong SVG attractors, and VR stereo dashboards.
- **ZKAEDI PRIME Hamiltonian Dynamics**: 17 sessions documenting the canonical two-field solver ($H_t$), FHN spatiotemporal chaos (v1–v26), Forge v5 OMEGA SUPREME, and MAML meta-learning.
- **3D Mesh, Raytracing & VFX Pipeline**: 12 sessions producing `zkaedi_3d_tools.py`, ZK3D SAH BVH unpackers, FastAPI VFX microservices, and Blender animation keyframe rendering.
- **Blockchain, NFT & Ledger Systems**: 9 sessions delivering `immutable_ledger.py`, Sepolia ERC-721 minting, ZKAEDI trading dashboards, and ZKGhost LSB steganography vault engines.
- **Browser Extensions & Edge Tooling**: 9 sessions providing the ZKAEDI Edge Recovery Extension (Manifest v3), Edge sidebar persistent panels, and Cloudflare Worker GLB drop zones.
- **Dark Forest Suite & EVM Security**: 7 sessions generating the Dark Forest Web Audio combat game, EVM Exploit Theater particle physics, Mempool Warfare simulator, and Solidity contract scanner.
- **Orchestration & Multi-Agent Mesh**: 6 sessions yielding `zkaedi_orchestration_mesh.py` (117 KB), drag-and-drop agent generator apps, and edge intelligence reports.

*Full Cherry-Picked Catalog File*: [`CHERRY_PICKED_CATALOG.md`](file:///H:/__DOWNLOADS/zcc_github_upload/CHERRY_PICKED_CATALOG.md)


---

# 16. COMPLETE FILE TRACE INDEX — 5,449 FILES

## ZCC Compiler Sources (H:\__DOWNLOADS\zcc_github_upload)

```
part0_pp.c          part1.c              part2.c
part3.c             part4.c              part5.c
part7_rust.c        compiler_passes.c    ir.h
ir_pass_manager.c   ir_to_x86.c          zcc_ir_opt_passes.h
zcc_float_gauntlet.c  zcc_network.c      float_guard.py
Makefile            gate_ir1.sh          rust_frontend_plan.md
```

## ZCC Tests

```
tests/run_float_fuzz.py
tests/test_safe_div_ir.c
tests/rust/test_safe_div_rust.rs
tests/rust/test_float_abi_strict.rs
juliet_train_subset/CWE-121/s02-s08/*.c
juliet_train_subset/CWE-190/s06/*.c
```

## ZCC Documentation & Forensics

```
FORENSIC_CORRECTION_2026-04-19.md
FORENSIC_023A_PARSER001.md
ZCC_STATUS.md
ZCC_BATTLEPLAN_v1.0.3.md
BUGS.md
BOOTSTRAP_BASELINES.tsv
docs/DEBUG_PROTOCOL.md
docs/evidence/2026-07-16/safe_div_ir/{commands.txt,stdout.log,artifacts.md,verdict.md}
evidence/zcc-run-1784467045-217774/provenance.jsonl
```

## ZCC Tickets

```
tickets/PP-REWRITE-REGRESSION-ROLLBACK.md
tickets/b299f43-gate-evidence.md
tickets/pp-blocked-list-bounds-gate-evidence.md
tickets/pp-macro-hideset-barrier-gate-evidence.md
tickets/offset-aware-pointer-*.md
tickets/e4-escape-analysis-*.md
tickets/switch_stmt-issue.md
tickets/pointer_deref-issue.md
```

## ZKAEDI PRIME Tools

```
tools/prime/zkaedi_prime.py
tools/zkaedi_orchestration_mesh.py         (117 KB)
tools/zkaedi_3d_tools.py                   (64 KB)
tools/zcc_sdf_compiler.py                  (165 KB)
tools/abi_bitfield_lane_gen.py
tools/dashboards/js-yaml.min.js
```

## ZKAEDI Lab

```
zkaedi-lab/lineage/immutable_ledger.py     (28 KB)
zkaedi-lab/runner/online_canary_gate.py
zkaedi-lab/runner/zkaedi_agent_mutator.py
zkaedi-lab/tests/test_ledger_security.py   (68 KB)
```

## Brain Session Artifacts (selected)

```
brain/0da2e327-*/prestige_stability_saturation_post_mortem.md
brain/0da2e327-*/zcc_quantum_segfault_analysis.md
brain/0da2e327-*/scratch/run_1000yr_extremely_fast.py
brain/014e74b0-*/antigravity_edge_suite_intelligence_report.md
brain/014e74b0-*/vulnerability_topology_report.md
brain/014e74b0-*/scratch/analyze_workers.py
brain/014e74b0-*/scratch/extract_zips.py
brain/c6935108-*/clifford.svg
brain/c6935108-*/dejong.svg
brain/c6935108-*/dynamics_viz.html
brain/c6935108-*/vr_stereo_viz.html
brain/c6935108-*/part4_diff_audit.md
brain/cc5b687d-*/cg_mismatch_1003697_autopsy.md
brain/cba8cd53-*/bug_hunt_post_mortem.md
brain/dd489b5f-*/float_codegen_comparison.md
```

## 3D Visualizer Artifacts (a4147fa4-*)

```
3d_hamiltonian_torus.html        3d_02_gravity_well.html
3d_wormhole.html                 3d_gravity_vortex.html
3d_audio_planet.html             3d_waterfall_spectrogram.html
3d_09_supernova.html             3d_06_dna_helix.html
3d_07_trefoil_knot.html          3d_01_mobius.html
3d_08_sine_ocean.html            3d_04_hyperboloid.html
3d_05_ripple.html
```

## Blender Animation Stills (067eabf5-*)

```
closeup_frame_0001.png  closeup_frame_0120.png  closeup_frame_0240.png
h1_reveal.png           h1_reveal_crane_swing.png
h2_wheel_quarter_spin.png  h2_wheel_spin.png
h3_Z_swing.png          h4_Z_letter_swing.png
```

## Scratch Ecosystem

```
scratch/vfx_pipeline/app/main.py                   (26 KB FastAPI)
scratch/zkghost_source/zkghost-vault-modularized/  (10-file test suite)
scratch/ZKAEDI_PRIME_SUITE/zkaedi-trading-dashboard/dist/  (229 KB bundle)
scratch/ZKAEDI_PRIME_SUITE/anti-gravity-engine/worker/anti-gravity-engine.min.js (110 KB)
scratch/agent_generator_app/app.py                 (46 KB)
scratch/gemini_prime_probe.py                      (22 KB)
scratch/patch_extractor.py                         (34 KB)
scratch/write_neon.py                              (24 KB)
scratch/build_notebook.py                          (23 KB)
```

---

# 17. COMMAND EXECUTION HISTORY — 3,017 COMMANDS

## Command Categories

### ZCC Build & Bootstrap (~340 executions)

```bash
wsl -e sh -c "cd /mnt/h/__DOWNLOADS/zcc_github_upload && make selfhost"
wsl -e sh -c "cd /mnt/h/__DOWNLOADS/zcc_github_upload && make zcc"
wsl -e sh -c "cd /mnt/h/__DOWNLOADS/zcc_github_upload && make clean"
wsl -e sh -c "cd /mnt/h/__DOWNLOADS/zcc_github_upload && make test"
wsl -e sh -c "cmp zcc2.s zcc3.s"
wsl -e sh -c "make ir-verify 2>&1 | tee /tmp/zcc-ir-verify.log"
```

### Git Operations (~520 executions)

```bash
wsl -e sh -c "git log --all --oneline --format='%h %ad %s' --date=short | head -30"
wsl -e sh -c "git diff --stat"
wsl -e sh -c "git add -p"
wsl -e sh -c "git commit -m '...'"
wsl -e sh -c "git push origin main"
wsl -e sh -c "git log -p --follow -- <file> | head -300"
wsl -e sh -c "git remote add origin https://github.com/invariantzkaedi/zcc-bootstrap-compiler.git"
```

### GitHub Actions (~85 executions)

```bash
wsl -e gh run list --status failure
wsl -e gh run view <id> --json headSha,headBranch,event,conclusion,status
wsl -e gh run watch <id>
```

### Python Analysis & Automation (~890 executions)

```bash
python C:\Users\zkaed\.gemini\antigravity-ide\brain\<session>\scratch\*.py
python tools\prime\zkaedi_prime.py --gauntlet
python float_guard.py
python tests\run_float_fuzz.py
python tools\abi_bitfield_lane_gen.py
```

### WSL System & Compilation (~410 executions)

```bash
wsl -e sh -c "gcc --version"
wsl -e sh -c "make --version"
wsl -e sh -c "cargo --version"
wsl -e sh -c "wsl g++ -Wno-deprecated-declarations -DUSE_SHA256 -std=c++17 -O2 ..."
wsl -e sh -c "python3 -m maturin develop --release"
wsl -e sh -c "apt install -y gcc make cargo"
```

### Blender Headless (~45 executions)

```bash
blender -b Untitled.blend --python-exit-code 1 --python render_frames.py
blender -b --python-exit-code 1 -P check_gltf_api.py
```

### Node.js & Frontend (~175 executions)

```bash
node test_prompt_generator.js
npm run dev
npm run build
npx vite build
npx -y create-vite-app@latest ./
```

### SQLite DB Inspection (~55 executions)

```bash
python -c "import sqlite3; conn = sqlite3.connect('...db'); ..."
python scratch\inspect_convs.py
python scratch\extract_all.py
```

---

# 18. POST-MORTEMS & FORENSIC DOCUMENTS

## PP-REWRITE-REGRESSION-ROLLBACK (commit 8098a94)

Lesson: A wholesale rewrite of part0_pp.c caused cascading failures across 40+ test cases.
Required reverting to last known-good state and applying surgical edits.
Maximum diff size for preprocessor changes: 50 lines.

## CG-MISMATCH-1003697 Autopsy

`brain/cc5b687d/cg_mismatch_1003697_autopsy.md`
Root cause: edge case in struct member access codegen when struct pointer was in a
callee-saved register that had been spilled.

## Bug Hunt Post-Mortem

`brain/cba8cd53/bug_hunt_post_mortem.md`
3-day hunt for non-deterministic self-host failure.
Root cause: race condition in temporary file naming during parallel compilation.
Fix: unique temp file names using process ID + timestamp.

## Float Codegen Comparison

`brain/dd489b5f/float_codegen_comparison.md`
50 test cases GCC vs ZCC comparison:
3 precision differences | 1 NaN handling discrepancy | 1 denormal rounding difference

## ZCC Quantum Segfault Analysis

`brain/0da2e327/zcc_quantum_segfault_analysis.md`
SIGSEGV in stage-2 ZCC compiling quantum simulation program.
Root cause: function pointer through struct member with wrong type cast in codegen.

## Prestige Stability Saturation Post-Mortem

`brain/0da2e327/prestige_stability_saturation_post_mortem.md`
PRIME field behavior during 1000-year simulated evolution.
Discovery: "prestige saturation" fixed point where field stops evolving despite noise.
Resolution: adaptive noise floor prevents full saturation.

---

# 19. BRAIN ARTIFACTS BY SESSION

| Session | Key Artifacts | Type |
|---------|-------------|------|
| 014e74b0 | antigravity_edge_suite_intelligence_report.md | Analysis |
| 014e74b0 | vulnerability_topology_report.md | Security |
| 05958d7b | brain_inventory_report.md, brain_manifest.md | Documentation |
| 0da2e327 | prestige_stability_saturation_post_mortem.md | Post-mortem |
| 0da2e327 | zcc_quantum_segfault_analysis.md | Debug |
| 2a304443 | walkthrough.md | Summary |
| 39e0d7a8 | implementation_plan.md, walkthrough.md | Plan/Summary |
| 416b7d8a | implementation_plan.md, walkthrough.md | Plan/Summary |
| 42435e75 | implementation_plan.md, task.md, walkthrough.md | Plan/Summary |
| 5198abf3 | implementation_plan.md, task.md, walkthrough.md | Plan/Summary |
| 5321fdc4 | implementation_plan.md, walkthrough.md | Plan/Summary |
| 58b471bc | implementation_plan.md, walkthrough.md | Plan/Summary |
| 70153340 | audit_report.md, viewport_recording.md, walkthrough.md | Audit/Media |
| 72f2da15 | implementation_plan.md, walkthrough.md | Plan/Summary |
| 7d20bba7 | implementation_plan.md, walkthrough.md | Plan/Summary |
| a4147fa4 | 13x 3D WebGL HTML files (each ~5.3 MB) | Interactive |
| bba4d798 | implementation_plan.md, walkthrough.md | Plan/Summary |
| c3adffc1 | implementation_plan.md, walkthrough.md | Plan/Summary |
| c6935108 | clifford.svg, dejong.svg, dynamics_viz.html, vr_stereo_viz.html | Viz |
| c6935108 | part4_diff_audit.md | Analysis |
| cba8cd53 | bug_hunt_post_mortem.md | Post-mortem |
| cc5b687d | cg_mismatch_1003697_autopsy.md | Debug |
| dd489b5f | float_codegen_comparison.md | Analysis |
| 067eabf5 | blend_animation_preview.md, 9x PNG stills | Media |

---

# 20. SYSTEM MAP & ARCHITECTURE DIAGRAM

```
+==========================================================================+
|                    ZKAEDI COMPLETE SYSTEM MAP v2026-07-19                |
+==========================================================================+
|                                                                          |
|  LAYER 0: RUNTIME & BOOTSTRAP                                            |
|  [WSL Ubuntu (canonical shell)] [GCC (bootstrap)] [x86-64 SystemV ABI]  |
|                                                                          |
|  LAYER 1: ZCC COMPILER ENGINE                                            |
|  [part0_pp.c] [part1.c] [part2.c] [part3.c] [part4.c] [part5.c]        |
|  [IR Bridge: ir.h | ir_pass_manager.c | ir_to_x86.c]                    |
|  [part7_rust.c] [compiler_passes.c]                                     |
|  [SQLite 3.53.1] [Doom Engine] [Float Gauntlet] [Juliet CWE Suite]      |
|                                                                          |
|  LAYER 2: ZKAEDI PRIME DYNAMICS                                          |
|  [H_t = H_base + eta*H_{t-1}*sigma(gamma*H_{t-1}) + eps*N(0,1+beta|H|)]|
|  [FHN Chaos v1-v26] [Forge v5 / OMEGA SUPREME] [Anunnaki Agents x8]    |
|  [Meta-MAML/Reptile/Meta-SGD] [Ensemble Fusion] [PRIME Gauntlet CI]     |
|                                                                          |
|  LAYER 3: VISUALIZATION & PHYSICS                                        |
|  [SPH Fluid Solver] [CPU Pipeline Profiler] [Register Allocator]        |
|  [Hamiltonian Orbit Solver] [SDF Shader Auditor]                        |
|  [WebGL 3D Suite: 13 scenes] [Clifford/DeJong] [VR Stereo]             |
|                                                                          |
|  LAYER 4: SECURITY & BLOCKCHAIN                                          |
|  [Dark Forest] [EVM Exploit Theater] [Mempool Warfare Simulator]         |
|  [Contract Scanner] [Trace Analyzer] [Scenario Forge] [Omni Parser]     |
|  [NFT Mint Pipeline] [Trading Dashboard] [GLB Cloudflare Worker]        |
|                                                                          |
|  LAYER 5: 3D / VFX PIPELINE                                              |
|  [ZKAEDI 3D Tools] [ZK3D SAH BVH] [VFX Asset Microservice FastAPI]      |
|  [Meshy Parser] [Fleet Pipeline] [Blender Animation Pipeline]            |
|  [Mesh Quality Gate] [Tripo3D Plugin] [VRAM Compliance Auditor]          |
|                                                                          |
|  LAYER 6: INTELLIGENCE & SCIENCE (25+ APIs)                              |
|  [AlphaGenome/AlphaFold] [gnomAD/ClinVar/dbSNP/VEP] [ChEMBL/OpenFDA]  |
|  [GTEx/HPA/ENCODE] [STRING/InterPro/UniProt] [PubMed/arXiv/OpenAlex]   |
|  [ClinicalTrials.gov] [Reactome] [EMBL-EBI OLS]                         |
|                                                                          |
|  LAYER 7: ORCHESTRATION & INTEGRITY                                      |
|  [Orchestration Mesh 117KB] [Immutable Ledger] [ZKGhost Vault/LSB]      |
|  [Canary Gate] [Agent Mutator] [Lineage Tracking]                       |
|  [AGENTS.md Rules] [Phase 0 Protocol] [5-Gate Verification System]      |
|  [Edge Extension] [Anti-Gravity Worker] [SRDF Research Toolkit]         |
|                                                                          |
|  CROSS-CUTTING:                                                          |
|    142 Sessions | 5,449 Files | 3,017 Commands | 206 PB Snapshots        |
|    142 SQLite DBs | 25+ KI Artifacts | 8 Post-Mortems                   |
+==========================================================================+
```

---

## APPENDIX A: GIT REMOTE MANAGEMENT

After any `git filter-repo` history rewrite, immediately restore remote:
```bash
git remote add origin https://github.com/invariantzkaedi/zcc-bootstrap-compiler.git
```
Append `REWRITE` marker row to `BOOTSTRAP_BASELINES.tsv` to preserve lineage auditing.

---

## APPENDIX B: OPERATOR SHORTCUTS

```bash
alias zcc_phase0='git log --all --oneline --format="%h %ad %s" --date=short | head -30; find . -maxdepth 2 \( -name "FORENSIC*.md" -o -path "./tickets/*.md" \) -print'
alias zcc_baseline='set -o pipefail; make selfhost 2>&1 | tee /tmp/zcc-selfhost-baseline.log'
alias zcc_gate1='cmp zcc2.s zcc3.s'
alias zcc_ir='make ir-verify 2>&1 | tee /tmp/zcc-ir-verify.log'
alias zcc_float='python float_guard.py && python tests/run_float_fuzz.py'
alias prime_gauntlet='python tools/prime/zkaedi_prime.py --gauntlet'
alias canary='python zkaedi-lab/runner/online_canary_gate.py'
```

---

## APPENDIX C: ENVIRONMENT SPEC

```
OS:      Windows 11 (Host) + WSL Ubuntu (Build)
Shell:   PowerShell (user) / bash in WSL (compilation)
Repo:    H:\__DOWNLOADS\zcc_github_upload  (Windows)
         /mnt/h/__DOWNLOADS/zcc_github_upload (WSL)
Brain:   C:\Users\zkaed\.gemini\antigravity-ide\brain\
Config:  C:\Users\zkaed\.gemini\config\
Alt:     C:\Users\zkaed\.gemini\antigravity\
Python:  3.12 (vfx_pipeline venv)
Rust:    via WSL cargo + maturin
Node:    via npm/npx for Vite builds
GitHub:  https://github.com/invariantzkaedi/zcc-bootstrap-compiler
```

---

## APPENDIX D: FORENSIC TIMESTAMP LINEAGE

```
2026-04  FORENSIC_CORRECTION_2026-04-19.md: Gate discipline established
         Commit ae6b5ff: First gate-evidence commit body template
         Commit 8098a94: PP-REWRITE-REGRESSION-ROLLBACK documented

2026-05  ZCC IR Bridge pipeline architecture v1.0.0
         FHN chaos research begins (v1-v26 iterations)

2026-06  ZCC Doom Milestone: 7 systems discoveries
         CG-CMP-001: setle/setge opcode swap fixed
         SQLite 3.53.1: Parse struct layout parity established
         ZKGhost Vault: LSB steganography engine built
         ZCC IR Bridge: Segfault resolved (stdout void* registration)

2026-07  Float Gauntlet: IEEE 754 precision engine verified
         Safe Div IR: Gate IR-1 evidence collected
         Rust Frontend: part7_rust.c integration designed
         ZKAEDI PRIME Forge v5 / OMEGA SUPREME canonized
         Brain enumeration: This document (2026-07-19)
```

---

## APPENDIX E: SEVERITY MAP — COMPILATION TARGETS

```
Target           Lines      Complexity  Status
linuxdoom-1.10   ~100,000   EXTREME     VERIFIED (self-host + run)
sqlite3.c        ~250,000   EXTREME     VERIFIED (self-host + queries)
lua              ~20,000    HIGH        VERIFIED
curl             ~150,000   HIGH        IN PROGRESS
zcc itself       ~35,000    HIGH        SELF-HOSTING (Gate 1 PASS)
```

---

*README_EXTENDED.md*
*Generated: 2026-07-19 from 142 conversation sessions, 5,449 file traces, 3,017 commands.*
*Source of truth: checked-out tree + forensic chain.*
*Memory and framing are subordinate to artifacts.*

*Repository: H:\__DOWNLOADS\zcc_github_upload*
*Author: zkaedi | Compiled by: Antigravity IDE*
