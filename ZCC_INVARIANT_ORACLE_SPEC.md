# 🔱 ZCC INVARIANT ORACLE SPECIFICATION

> **Doctrine**: We do not construct isolated test suites; we define mathematically precise **Invariant Domains** verified by autonomous, compiler-integrated **Invariant Oracles**.

---

## 🔱 1. THE INVARIANT ORACLE PROTOCOL

The ZCC compiler is transitioning from an opaque transformation engine into a sovereign, self-proving systems toolchain. By introducing direct command-line oracle options, ZCC can verify its own compilation correctness, memory alignments, and target determinism invariants natively.

```text
               ┌──────────────────────────────────────┐
               │         ZCC compiler driver          │
               └──────────────────┬───────────────────┘
                                  │
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
 ┌───────────────┐        ┌───────────────┐        ┌───────────────┐
 │  --oracle-abi │        │--oracle-layout│        │--oracle-det   │
 └───────┬───────┘        └───────┬───────┘        └───────┬───────┘
         │                        │                        │
         ▼                        ▼                        ▼
 [Calling Convention]     [Memory Topology]        [Entropy Audits]
```

---

## 🔱 2. ORACLE ARCHITECTURE & CLI SPECIFICATION

### 🔱 2.1 ABI Invariant Oracle (`--oracle-abi`)
* **Objective**: Automatically traces and validates calling convention conformity against the x86-64 SystemV AMD64 ABI specification.
* **CLI Contract**:
  ```bash
  zcc --oracle-abi <file.c>
  ```
* **Property Domains Verified**:
  - **Register Classification**: Ensures arguments (GPRs `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9` and XMM registers `xmm0` to `xmm7`) are classified identically to host compilers.
  - **Eightbyte Split & Memory Passing**: Validates stack spill boundary transitions when aggregate parameters exceed 16 bytes.
  - **Hidden `sret` Corridor**: Validates that functions returning aggregates by-value correctly displace pointer destinations into `%rdi` and preserve return registers.
  - **Variadic Register-Save Areas**: Traces `%al` bounds, spill space allocations, and va_list pointer alignments.
* **Oracle Output Schema**:
  ```json
  {
    "oracle": "abi",
    "status": "ABI_VERIFIED",
    "signature_fingerprint": "0x8fa372eb001a",
    "inspected_functions": [
      {
        "name": "mix_varargs",
        "calling_convention": "SystemV_AMD64",
        "argument_spill_size": 32,
        "gpr_occupancy": 4,
        "sse_occupancy": 2,
        "sret_active": false
      }
    ]
  }
  ```

---

### 🔱 2.2 Memory Layout Invariant Oracle (`--oracle-layout`)
* **Objective**: Continuous memory topology certification, proving physical layout and size alignment equivalence against host compiler frontends.
* **CLI Contract**:
  ```bash
  zcc --oracle-layout <file.c>
  ```
* **Property Domains Verified**:
  - **`sizeof` Determinism**: Asserts matching sizes for base types, pointer arrays, and nested structures.
  - **`offsetof` Integrity**: Traces relative byte addresses of structure members, accounting for precise alignment padding.
  - **Bitfield Packing**: Validates exact bit occupancy boundaries, bit-shift masks, and adjacent field byte overlaps.
  - **Union Coalescence**: Confirms overlap granularity of mixed-scalar payloads.
* **Oracle Output Schema**:
  ```json
  {
    "oracle": "layout",
    "status": "LAYOUT_VERIFIED",
    "structs_audited": [
      {
        "identifier": "WeirdStruct",
        "total_bytes": 48,
        "bitfield_packing": "deterministic",
        "members": [
          {"name": "a", "offset": 0, "bit_offset": 0, "bits": 3},
          {"name": "b", "offset": 0, "bit_offset": 3, "bits": 11},
          {"name": "tail", "offset": 40, "bit_offset": 0, "bits": 64}
        ]
      }
    ]
  }
  ```

---

### 🔱 2.3 Determinism Invariant Oracle (`--oracle-determinism`)
* **Objective**: Compiler-native, environment-agnostic reproducibility auditing.
* **CLI Contract**:
  ```bash
  zcc --oracle-determinism <file.c>
  ```
* **Property Domains Verified**:
  - **Environmental Shuffling**: Natively compiles the target under simulated environment shuffling (stripping and randomizing environment storage vectors).
  - **Telemetry Neutrality**: Verifies that active telemetry outputs and IR logging (`ZCC_EMIT_IR=1`) have zero side-effects on emitted machine instructions.
  - **Hash Convergence**: Asserts that sequentially emitted binary artifacts produce bitwise-identical SHA256 signatures.
* **Oracle Output Schema**:
  ```json
  {
    "oracle": "determinism",
    "status": "DETERMINISM_VERIFIED",
    "converged_hash": "518ee90fc7f19de0cf53afc45fac4e4814ff4711d77d0a780f1736f31bc7daa0",
    "runs": 15,
    "env_entropy_survived": "100%",
    "pass_ordering": "stable",
    "allocation_order": "ASLR-independent"
  }
  ```

---

### 🔱 2.4 Self-Host Invariant Oracle (`--oracle-selfhost`)
* **Objective**: Sovereign multi-stage fixed-point verification.
* **CLI Contract**:
  ```bash
  zcc --oracle-selfhost
  ```
* **Property Domains Verified**:
  - **Stage Convergence**: Automates and drives the entire ZCC Stage-1 -> Stage-2 -> Stage-3 -> Stage-4 compilation pipeline natively.
  - **Byte-Identity Verification**: Asserts that `zcc2.s` and `zcc3.s` are bit-identical, and that compiler binaries `zcc2` and `zcc3` exhibit absolute behavioral parity.
  - **Lattice Certification**: Automatically compiles the complete 12-test SystemV sentinel gauntlet and validates execution outputs.
* **Oracle Output Schema**:
  ```json
  {
    "oracle": "selfhost",
    "status": "SELFHOST_CONVERGED",
    "stage_matrix": {
      "zcc1_to_zcc2": "SUCCESS",
      "zcc2_to_zcc3": "SUCCESS",
      "zcc3_to_zcc4": "SUCCESS"
    },
    "fixed_point": {
      "stage2_asm_hash": "a4d320be...",
      "stage3_asm_hash": "a4d320be...",
      "assembly_bitwise_identical": true,
      "binary_identical": true
    },
    "battle_lattice_score": "12/12 PASS"
  }
  ```

---

### 🔱 2.5 Stack Topology Oracle (`--oracle-stack`)
* **Objective**: Continuous verification of stack frame security boundaries.
* **CLI Contract**:
  ```bash
  zcc --oracle-stack <file.c>
  ```
* **Property Domains Verified**:
  - **Pushes/Pops Balance**: Ensures every caller-save register push has a mathematically matching pop sequence across nested branch exits.
  - **16-Byte Realignment**: Verifies that call instructions are reached with stack addresses aligned to a 16-byte boundary.
  - **Local Stack Frames**: Validates frame size alignments and offset displacements for local variables.

---

## 🔱 3. SOVEREIGN ARCHITECTURAL BLUEPRINT

Integrating these Oracles directly into ZCC’s source drivers elevates ZCC into a sovereign toolchain, completely decoupling compiler assurance from external OS shells or shell-script test suites.

```text
               ╔═════════════════════════════════════╗
               ║       ZCC SOVEREIGN ENGINE          ║
               ╚══════════════════╦══════════════════╝
                                  ║
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
 ┌───────────────┐        ┌───────────────┐        ┌───────────────┐
 │  Direct ELF   │        │     zld       │        │   SMT Proof   │
 │   Emission    │        │  Sovereign    │        │   Engine      │
 │  (Relocatable)│        │    Linker     │        │  (Z3 Formulas)│
 └───────────────┘        └───────────────┘        └───────────────┘
```

---

*🔱 ZKAEDI COMPILER FORGE — ZCC Invariant Oracle Specifications*
*Establishing mathematically provable domains inside sovereign toolchains.*
