# EVM Lifter Semantic Boundary

This document formalizes the intentional semantic boundaries for the EVM Bytecode Lifter subsystem in the `zkaedii/zcc` repository. It serves to clarify what is "in scope" vs "out of scope" for an SSA IR Frontend, explicitly separating documented constraints from software defects.

## 1. Scope Definition
**In Scope (Static Analysis Frontend):**
The purpose of the EVM lifter in this repository is to operate as a *static analysis frontend*, not an execution engine. It aims to emit a structurally coherent intermediate representation for downstream analysis tools (e.g., taint tracking, semantic anomaly detection, or visualization).
- Translating linear EVM bytecode streams into the ZCC 3-address SSA IR.
- Resolving the operational stack (simulating depth, constraints, and dependencies).
- Identifying standard basic blocks and static control-flow edges.
- Tagging instructions with explicit security and memory classifications (`evm_ir_tag_t`) to enable downstream static analysis passes.

**Out of Scope (Execution Emulation):**
- Perfect 256-bit numerical emulation during compilation.
- Complete dynamic symbolic execution.
- Tracking concrete state changes within the EVM linear memory mapping (`MSTORE`, `MLOAD`) or Storage trie across abstract function calls.

## 2. 64-bit Truncation Constraint
The EVM native word size is 256 bits, whereas ZCC's current IR represents 64-bit wide integers natively. 
To honor this mismatch honestly without halting analysis:
- Narrow values (`PUSH1` to `PUSH8`) map directly to an `IR_CONST`.
- Wide values (`PUSH9` to `PUSH32`) map to an `IR_CONST` containing the least-significant 64 bits of the immediate, but are explicitly flagged with `IR_TAG_TRUNCATED_WIDE_CONST`.
- **Why it is accepted:** For security audits and structural analysis, recognizing the *origin* and *topology* of constants is often more critical than performing full-width arithmetic during the compilation pass.

## 3. Control Flow & Indirect Jumps
- **Direct Jumps (`PUSH` -> `JUMP`):** Completely mapped. The lifter statically verifies the target against a pre-computed bitmap of valid `JUMPDEST` ops. Invalid static targets emit a synthetic `EVM_BARRIER`.
- **Indirect Jumps (Dynamic Stack Target -> `JUMP`):** Intentionally left approximated. The lifter lacks a full data-flow analysis engine necessary to trace calculated pointers through arithmetic or memory back to a jump target. It emits `IR_NOP` (with stack pops) and passes execution to the next boundary.

## 4. Semantic Support Classifications
As formalized in PR #23, EVM opcodes belong to one of four classes:
1. **`FULLY_SUPPORTED`**: The semantics of the opcode are mapped directly to corresponding IR primitives (e.g., `ADD`, `DUP`, `POP`).
2. **`APPROXIMATED_ANALYZABLE`**: The semantics are abstract or deferred, but the instruction yields a structurally sound, highly-tagged IR segment.
3. **`PLACEHOLDER_ONLY`**: A basic scaffold ensuring the stack pops/pushes the correct item counts, but providing no domain-specific semantic translation (e.g., `ADDMOD`).
4. **`INVALID_OR_UNASSIGNED`**: Unused 0x00-0xff byte values.

## 5. Synthetic Intrinsic Constructs
Opcodes exhibiting highly complex IO or memory mutations are mapped to pseudo-intrinsic function calls.
- **`__evm_sha3`**: Takes two arguments (offset, length). Emits `IR_CALL` with `IR_TAG_SHA3`. 
- **`__evm_calldatacopy` / `__evm_codecopy`**: Takes structured `IR_ARG` lists mimicking the EVM pop count and tracks memory operation topology (`IR_TAG_MEMORY_COPY`).
- **`__evm_logN`**: Dynamically packs offset, length, and `N` topics into an `IR_CALL` linked structure tagged `IR_TAG_LOG`. 

These intrinsics do *not* execute code. They are synthetic tokens allowing the IR to express complex EVM side-effects transparently to the backend optimizer and analysis tooling.
