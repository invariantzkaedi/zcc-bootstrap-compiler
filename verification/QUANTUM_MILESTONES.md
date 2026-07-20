# Quantum Optimizer Sub-Milestones

This document tracks the sub-milestones required to graduate the Quantum Optimizer passes from their current state (Numerical Tests `[Verified]`) to fully production-ready (`[Verified]` across all axes). 

## Phase 1: Coverage & Integration (The Coverage Status)
- [ ] **Milestone 1.1**: Integrate coverage tooling (`gcov`/`lcov`) into the WSL test harness to trace execution across `src/opt/quantum_rules.c`.
- [ ] **Milestone 1.2**: Expand the fuzzing corpus (currently at 50 iterations) to guarantee 100% branch and statement coverage for all four implemented rules.
- [ ] **Milestone 1.3**: Promote **Coverage Status** to `[Verified]` across all rules in `rule_inventory.md`.

## Phase 2: Formal Verification (The Symbolic Proof Status)
- [ ] **Milestone 2.1**: Define the SMT-Lib mappings for `H`, `CNOT`, `RZ`, and `SWAP` matrix transformations.
- [ ] **Milestone 2.2**: Wire `z3` (or equivalent SMT solver) into a proof-generation script (`tools/verify_smt.py`).
- [ ] **Milestone 2.3**: Generate and validate symbolic proofs for exact adjacency cancellation (Rules 1, 3, 4) and angle addition (Rule 2).
- [ ] **Milestone 2.4**: Promote **Symbolic Proof Status** to `[Verified]` across all rules in `rule_inventory.md`.

## Phase 3: Production Hardening
- [ ] **Milestone 3.1**: Enable arbitrary pass scheduling (commuting rules past intermediate diagonal/identity gates).
- [ ] **Milestone 3.2**: Execute a full end-to-end compilation of a mock quantum circuit, emitting target classical simulation assembly.
