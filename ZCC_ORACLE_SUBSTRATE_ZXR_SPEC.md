# 🔱 ZCC ORACLE RUNTIME SUBSTRATE & EXECUTION RECORD (.ZXR) SPECIFICATION

> **Doctrine**: All compiler invariants are queryable; all compilation decisions are deterministic, recordable, and replayable. We transition the compiler from a stateless transformer into a verifiable, time-travel computational substrate.

---

## 🔱 1. THE ARCHITECTURAL FLOW

The Oracle Runtime Substrate provides the shared execution fabric, telemetry dispatchers, invariant collectors, and stable fingerprinting engine. Every compilation run can be logged into a strictly deterministic, append-only **ZCC Execution Record (`.zxr`)** file.

```text
       Source File (.c) ──► [ ZCC Parser & Codegen ] ──► [ ASM Emission (.s) ]
                                   │
                                   ▼
                   ╔═══════════════════════════════╗
                   ║    ORACLE RUNTIME SUBSTRATE   ║
                   ╚═══════════════╦═══════════════╝
                                   │
         ┌─────────────────────────┼─────────────────────────┐
         ▼                         ▼                         ▼
 ┌───────────────┐         ┌───────────────┐         ┌───────────────┐
 │ Invariant     │         │ stable        │         │ execution     │
 │ Collectors    │         │ Fingerprints  │         │ Record (.zxr) │
 └───────┬───────┘         └───────┬───────┘         └───────┬───────┘
         │                         │                         │
         ▼                         ▼                         ▼
   --oracle-stack            [CFG/ABI/Stack]           zcc --replay
```

---

## 🔱 2. THE ORACLE RUNTIME SUBSTRATE C INTERFACE

To ensure absolute portability and zero external dependency, the Substrate is built natively in pure C:

```c
/* ================================================================= */
/* ZCC ORACLE RUNTIME SUBSTRATE ENGINE                               */
/* ================================================================= */

#ifndef ZCC_ORACLE_SUBSTRATE_H
#define ZCC_ORACLE_SUBSTRATE_H

#include <stdio.h>
#include <stdint.h>

#define MAX_FINDINGS 1024
#define FINGERPRINT_LEN 64

typedef struct {
    char type[64];
    char message[256];
    int line_number;
    int critical;
} InvariantFinding;

typedef struct {
    const char *oracle_name;
    const char *status; /* "VERIFIED" or "DIVERGENCE_DETECTED" */
    char fingerprint[FINGERPRINT_LEN + 1];
    InvariantFinding findings[MAX_FINDINGS];
    int num_findings;
} OracleReport;

typedef struct {
    const char *name;
    int (*run)(void *cc, OracleReport *report);
} InvariantOracle;

/* Shared Oracle Registry */
typedef struct {
    InvariantOracle oracles[32];
    int num_oracles;
} OracleRegistry;

/* ================================================================= */
/* STABLE COMPILER FINGERPRINTS                                      */
/* ================================================================= */

typedef struct {
    uint64_t cfg_topology_hash;      /* Structural control-flow complexity */
    uint64_t abi_lowering_hash;      /* Calling convention register maps */
    uint64_t stack_geometry_hash;    /* Local/spill alignments and offsets */
    uint64_t regalloc_sequence_hash; /* Graph coloring allocation order */
    uint64_t pass_pipeline_hash;     /* Active optimization pass traces */
    uint64_t entropy_seed_hash;      /* Environmental baseline signature */
} CompilerFingerprint;

#endif /* ZCC_ORACLE_SUBSTRATE_H */
```

---

## 🔱 3. THE FOUNDATIONAL ORACLE: `--oracle-stack`

Stack topology unifies calling convention registers, aggregate spills, stack adjustments, and alignment invariants. The Stack Oracle validates frame integrity at every callsite:

### 🔱 3.1 Stack Frame Invariants
1. **RSP 16-Byte Realignment**: Verifies that the stack pointer is aligned to 16 bytes immediately prior to executing a `call` instruction.
2. **Push/Pop Balance**: Ensures any caller-saved GPR pushes are exactly popped before exiting the active block or function.
3. **Spill Boundary Protections**: Asserts that local variable alignments do not overlap with spill slots or parameters passed on the stack.

### 🔱 3.2 Stack Oracle Telemetry Output
```json
{
  "oracle": "stack",
  "status": "VERIFIED",
  "fingerprint": "8fa3c0eb01ffac3e2849de748b0a1122efcc7a88998b417c8d9e6e8e2c0db56e7",
  "inspections": [
    {
      "function": "consume_aggregate",
      "rsp_alignment_before_call": 16,
      "rsp_alignment_after_call": 16,
      "callee_saved_registers_preserved": true,
      "spill_headroom_bytes": 32,
      "max_frame_bytes": 96,
      "findings": []
    }
  ]
  "divergences": []
}
```

---

## 🔱 4. ZCC EXECUTION RECORDS (`.ZXR`) SPECIFICATION

Instead of throwing away compiler decision state after code-generation, ZCC records every optimization, register coloring choice, and lowering pass into a **ZCC Execution Record (`.zxr`)** file.

### 🔱 4.1 CLI Interface
* **Emit Execution Record**:
  ```bash
  zcc --emit-exec-record <source.c> -o <artifact.zxr>
  ```
* **Replay Execution Record**:
  ```bash
  zcc --replay-record <artifact.zxr>
  ```
* **Diff Compilation History**:
  ```bash
  zcc --diff-record <run_a.zxr> <run_b.zxr>
  ```

### 🔱 4.2 Structural Serialization Constraints
To ensure absolute, cross-platform reproducibility, the `.zxr` binary format enforces the following strict constraints:
1. **No Raw Pointer Leakage**: All node relations must be serialized as relative integer indices or canonical symbol identifiers.
2. **ASLR Independence**: Heap allocator memory locations must not influence node iteration orders.
3. **Append-Only Sequence**: All compiler pass events, CFG transforms, and register allocations are recorded as a linear time-series.
4. **Stable Versioning**: Includes a strict compiler version and runtime ABI check header.

### 🔱 4.3 `.zxr` JSON Equivalent Schema
```json
{
  "zxr_version": "1.0.0",
  "deterministic_epoch": 1,
  "compiler_version": "zcc_prime_1.1.0",
  "source_identity": {
    "filename": "kernel.c",
    "sha256": "4f92a34eb..."
  },
  "fingerprint": {
    "cfg_topology_hash": "0x8fa372eb001a",
    "abi_lowering_hash": "0xd320be44ac81",
    "stack_geometry_hash": "0x2849de748b0a",
    "regalloc_sequence_hash": "0xefcc7a88998b",
    "pass_pipeline_hash": "0x0db56e7f8fa3"
  },
  "pass_records": [
    {
      "pass": "constant_folding",
      "nodes_folded": 12,
      "dce_eliminated": 4
    },
    {
      "pass": "regalloc",
      "allocation_map": [
        {"vreg": 1, "preg": "rdi", "spilled": false},
        {"vreg": 2, "preg": "rsi", "spilled": false},
        {"vreg": 3, "preg": "stack", "spill_offset": -8}
      ]
    }
  ],
  "final_binary": {
    "size_bytes": 10240,
    "sha256": "8f92b7405e3692a188f6c4a30a1122efcc7a88998b417c8d9e6e8e2c0db56e7f"
  }
}
```

---

## 🔱 5. STRICT CANONICAL SERIALIZATION INVARIANTS

To guarantee absolute cross-machine and multi-year compilation convergence, all `.zxr` files adhere to these strict canonicalization rules:

1. **Little-Endian Binary Representation**: All serialized integers, sizes, offsets, and hashes are written in strictly little-endian format.
2. **Lexicographical Section Sorting**: All generated sections (such as `.text`, `.data`, `.rodata`, `.bss`) are serialized in strictly lexicographical order.
3. **Deterministic CFG DFS Traversal**: Intermediate Representation (IR) graphs and Control Flow Graph (CFG) nodes are serialized following a strictly deterministic Depth-First Search (DFS) topological order.
4. **Stable-Indexed Register Allocation Sequences**: All register allocation sequence blocks are output using stable-indexed virtual register maps.
5. **No Heap or Temporal Pollution**: Heap-based addresses, ASLR-influenced pointers, compilation wall-clock times, and thread execution schedules are strictly banned from entering the serialization stream.
6. **Immutable Block Architecture**: Once written, `.zxr` files are completely immutable and represent a cryptographically sealed computation block.

---

## 🔱 6. FORENSIC DRIFT INTROSPECTION (`zcc --explain-drift`)

When comparing two compilations to debug optimization or target variations, ZCC natively introspects the structural `.zxr` deltas:
```bash
zcc --explain-drift run_a.zxr run_b.zxr
```

### 🔱 6.1 Explain-Drift Output Architecture
```text
DRIFT DETECTED: REGALLOC DIVERGENCE

FUNCTION:
  transform()

STRUCTURAL DELTA:
  vreg_41 allocated register %r12 in run_a.zxr vs spill slot stack[-40] in run_b.zxr

DOWNSTREAM CASCADE:
  Altered instructions: movq %r12, %rax (run_a) vs movq -40(%rbp), %rax (run_b)
  Altered peephole collapse chain: 4 instructions elided vs 2 instructions elided
  Altered binary signature: d320be44ac81... vs 8fa372eb001a...

ROOT CAUSE IDENTIFIED:
  Pass pipeline ordering divergence inside optimization block.
```

---

## 🔱 7. PROOF-CARRYING COMPILATION & MATHEMATICAL PROVENANCE

To bridge deterministic execution tracking with mathematically checkable transformation proofs, ZCC natively supports **Proof-Carrying Compilation**.

### 🔱 7.1 Proof Record Command Interface
* **Emit Proof Verification Chains**:
  ```bash
  zcc --emit-proof-record kernel.c
  ```
  This command produces:
  - `kernel.zxr`: The deterministic execution history block.
  - `kernel.zproof`: The SMT-verifiable transformation correctness chain.

### 🔱 7.2 Symbolic ABI Invariant Proofs (`--oracle-abi-proof`)
Instead of behavioral checking, ZCC symbolically executes argument lowering and stack allocation pathways to verify absolute SysV conformance:
```bash
zcc --oracle-abi-proof file.c
```
* **Theorems Verified**:
  - **Register Assignment Legality**: Prove that registers assigned to parameters match the exact SystemV AMD64 eightbyte classification sequence.
  - **Non-Overlapping Stack domains**: Prove that stack-based arguments, local variables, and spill slots possess strictly disjoint physical addresses.
  - **Alignment Restoration**: Prove that the stack pointer remains aligned to a 16-byte boundary at every external callsite transition.

### 🔱 7.3 Granular Intermediate Replay (`--replay-until-pass <pass>`)
To support localized, time-travel debugging of compilation pipelines, execution records can be replayed up to an arbitrary optimization or lowering phase:
```bash
zcc --replay-until-pass regalloc kernel.zxr
```
This halts execution immediately at the specified phase, yielding:
- The exact state of the Control Flow Graph (CFG) at that moment.
- Active virtual register pressure maps.
- Graph coloring or spilling decisions responsible for the allocation.

### 🔱 7.4 Semantic vs Syntactic Determinism Invariants
While syntactic determinism (generating byte-identical machine assembly) is an exceptional baseline, ZCC formalizes **Semantic Determinism**:
- **Syntactic Parity**: Evaluates if instruction layouts, register mappings, and block sequences are bitwise identical.
- **Semantic Equivalence**: Evaluates if optimization schedules (e.g. branch scheduling, loop unrolling, or register substitutions) preserve semantic control-flow dominance structures and value mappings, even when syntactic layouts vary for target architecture tuning.

### 🔱 7.5 Isolated Micro-Theorem Lattices
To prevent combinatorial state-space explosion, ZCC breaks down mathematical proofs into highly focused, decoupled **Micro-Theorem Domains**:
- **Domain A: Peephole Equivalence**: Confirms localized instruction merges are semantic identities.
- **Domain B: Stack Geometry**: Proves disjoint address alignments for stack-allocated symbols.
- **Domain C: ABI Lowering**: Certifies SystemV register mappings.
- **Domain D: CFG Dominance**: Asserts control-flow topology is strictly preserved during block scheduling.
- **Domain E: Regalloc Interference**: Formally verifies graph coloring validity.

### 🔱 7.6 Automated Drift Bisection (`zcc --bisect-drift`)
Drifts between sequential or distributed compiles are isolated dynamically:
```bash
zcc --bisect-drift run_a.zxr run_b.zxr
```
* **Bisection Mechanism**:
  - Automatically performs a binary search over the `.zxr` compilation pass histories.
  - pinpoints the first semantic or syntactic divergence.
  - Identifies the exact optimization function or register coloring pass that triggered the theorem failure.

### 🔱 7.7 Trustless Compilation & Distributed Proof Consensus
By decoupling validation from platform environmental limits, ZCC establishes a **Verifiable Computational Substrate**:
- **Cross-Machine Consensus**: Compiling identical C sources on heterogeneous targets (WSL, Windows, ARM64 bare-metal) produces mathematically convergent `.zproof` theorem chains, even when machine-specific instruction scheduling varies.
- **Trustless Replay**: Remote build swarms can replay `.zxr` records and verify `.zproof` certificates natively without needing to trust the host compiler executable or execution environment.

### 🔱 7.8 Cryptographic Computational Attestations (`zcc --attest`)
To formalize public/private attestation keys on build artifacts, ZCC natively verifies the proof structures:
```bash
zcc --attest artifact.zxr artifact.zproof
```
* **Oracle Verification Output**:
  ```text
  COMPUTATION VERIFIED

  semantic domains:
    ABI ........ VERIFIED
    CFG ........ VERIFIED
    REGALLOC ... VERIFIED
    STACK ....... VERIFIED
    PEEPHOLE .... VERIFIED

  consensus:
    7/7 remote nodes converged

  artifact trust:
    CRYPTOGRAPHICALLY ATTESTED
  ```

---

## 🔱 8. MINIMAL VIABLE IMPLEMENTATION BLUEPRINT

To guarantee structured execution without compounding complexity, implementation progresses through three highly focused developmental phases:

### 🔱 Phase A: Deterministic Pass Event Recorder
Embeds lightweight event tracking primitives directly inside the compiler driver:
* **Primitives**:
  - `record_pass_begin(const char *pass_name)`
  - `record_pass_end(const char *pass_name)`
  - `record_transform(const char *pass_name, uint64_t node_id, const char *transform_details)`
* **Active Passes Instrumented**: Only `constant_folding`, `peephole`, and `regalloc`.
* **Objective**: Verifies that compiler decision topology is mathematically stable under randomized environment vectors before compiling binary records.

### 🔱 Phase B: Canonical CFG Fingerprinter
Establishes ZCC’s first semantic intermediate representation fingerprinting layer:
* **Lowering Rules**:
  - Emits CFG block lists in strictly deterministic Depth-First Search (DFS) topological order.
  - Implements stable virtual node IDs and canonical control-flow edge sorting.
* **Objective**: Validates that compiled multi-stage outputs (`zcc`, `zcc2`, `zcc3`) yield exactly convergent semantic CFG fingerprints.

### 🔱 Phase C: Push/Pop Micro-Theorem Prototype
Implements ZCC’s first native SMT transformation correctness proof block, focused exclusively on stack instruction elimination:
* **Target Pattern**:
  ```asm
  push %rax
  pop  %rax
  ```
* **Theorem Verified**:
  - **Stack Depth Parity**: Prove that stack offset adjustments equal 0.
  - **Register Value Preservation**: Prove that `%rax` state contains identical semantic values post-elimination.
  - **Control Flags Invariance**: Prove that conditional flags register remains unperturbed.

---

## 🔱 9. SOVEREIGN SYSTEMS VERIFICATION DOCTRINE

To prevent observational telemetry and proof structures from perturbing active compiler logic, ZCC establishes strict domain boundaries:

### 🔱 9.1 The Separation of Observational Domains
By isolating computation paths, ZCC guarantees that debugging hooks, telemetry emissions, and oracle evaluations have zero semantic side-effects on codegen:
$$\text{AST Codegen} \neq \text{SSA IR Experimentation} \neq \text{Telemetry Substrate} \neq \text{Oracle Substrate} \neq \text{Proof Substrate}$$

### 🔱 9.2 AST Sovereignty vs Parallel IR Observation
* **AST Authority**: The Abstract Syntax Tree remains the canonical and sovereign authority of truth for native codegen and compiler bootstraps.
* **Parallel IR**: The SSA intermediate representation behaves as a strictly parallel observer of truth. This prevents PHI-node instability or CFG block ordering shifts from polluting recursive Stage-2 and Stage-3 compiler self-host loops.

### 🔱 9.3 Fixed-Point Convergence under Observation
Byte-identical assembly parity (`zcc2.s == zcc3.s`) ceases to be a simple bootstrap checklist and becomes a formal **Fixed-Point Convergence Theorem**. This proof-by-induction demonstrates that the ZCC compiler remains a stable and invariant system under recursive self-compilation, even when tracing environmental variables and logging telemetry.

### 🔱 9.4 The Recursive ML Cognitive Feedback Loop
The compiler acts as its own high-fidelity dataset generator, initiating a self-hardening cognitive feedback loop:
1. **Emit**: Target compilations generate canonicalized Intermediate Representation (IR) streams.
2. **Train**: The emitted IR trains external ML anomaly/pattern analyzers.
3. **Discover**: Analyzers uncover subtle optimization pathologies or semantic vulnerabilities.
4. **Generate**: Discoveries generate new, rigorous battle sentinels.
5. **Harden**: Sentinels automatically lock and secure compiler-native invariant domains.
6. **Recompile**: The hardened compiler outputs structurally superior, safer IR telemetry.

---

## 🔱 10. THE ROADMAP TO CIVILIZATION-SCALE VERIFIABILITY

```text
  [Self-Host Compiler]
           │
           ▼
  [Deterministic Compiler] (Phase 19 Sealed)
           │
           ▼
  [Invariant-Proving Substrate] (Oracle Registry & --oracle-stack)
           │
           ▼
  [Replayable Compilation Records] (.zxr Emission & Time-Travel Replay)
           │
           ▼
  [Sovereign ELF Object Toolchain] (zld Sovereign Linker integration)
           │
           ▼
  [Verifiable Infrastructure Substrate] (Sovereign OS + AI Attestation)
```

---

*🔱 ZKAEDI COMPILER FORGE — ZCC Oracle Substrate & Execution Record Specs*
*Establishing mathematically provable domains inside sovereign toolchains.*
