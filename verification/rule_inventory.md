# ZCC Quantum Optimizer — Rule Inventory

This document catalogs all registered compiler optimizations and rewrites for the quantum stabilizer optimizer.

> **Sub-Milestones Tracking**: See [QUANTUM_MILESTONES.md](file:///H:/__DOWNLOADS/zcc_github_upload/verification/QUANTUM_MILESTONES.md) for the roadmap to fully verified status.

---

## 📋 1. Rule Catalog

### Rule 1: Hadamard Cancellation
* **Rule ID**: `RULE-QOPT-001`
* **Rewrite**: $H \cdot H \rightarrow I$
* **Preconditions**: Two consecutive Hadamard gates acting on the same target qubit.
* **Postconditions**: Both gates are removed from the instruction sequence.
* **Expected Invariant**: Unitary mapping remains identical.
* **Owner**: Optimizer Engine Team
* **Source File**: `src/opt/quantum_rules.c`
* **Function**: `opt_hadamard_cancel`
* **Introduced Commit**: `0fc8e0bd`
* **Last Verified**: `2026-07-20`
* **Last Symbolic Proof**: `N/A`
* **Coverage %**: `N/A`
* **Known Limitations**: Requires exact sequence adjacency; does not currently commute past intermediate diagonal gates.
* **Dependencies**: None
* **Symbolic Proof Status**: `[Pending Verification]`
* **Numerical Test Status**: `[Verified]` (Covered in `QAlgo-Cryptography-3`)
* **Coverage Status**: `[Pending Verification]`

---

### Rule 2: Rotation Merging (RZ)
* **Rule ID**: `RULE-QOPT-002`
* **Rewrite**: $RZ(\theta_1) \cdot RZ(\theta_2) \rightarrow RZ(\theta_1 + \theta_2)$
* **Preconditions**: Two consecutive RZ rotation gates acting on the same target qubit.
* **Postconditions**: The gates are replaced by a single RZ gate with the summed angle.
* **Expected Invariant**: Unitary mapping remains identical.
* **Owner**: Optimizer Engine Team
* **Source File**: `src/opt/quantum_rules.c`
* **Function**: `opt_rotation_merge_rz`
* **Introduced Commit**: `0fc8e0bd`
* **Last Verified**: `2026-07-20`
* **Last Symbolic Proof**: `N/A`
* **Coverage %**: `N/A`
* **Known Limitations**: Accumulates floating-point rounding errors on angle sums.
* **Dependencies**: None
* **Symbolic Proof Status**: `[Pending Verification]`
* **Numerical Test Status**: `[Verified]` (Covered in `QAlgo-ErrorCorrection-2`)
* **Coverage Status**: `[Pending Verification]`

---

### Rule 3: CNOT Cancellation
* **Rule ID**: `RULE-QOPT-003`
* **Rewrite**: $CX(c, t) \cdot CX(c, t) \rightarrow I$
* **Preconditions**: Two consecutive CNOT gates sharing the identical control and target qubits.
* **Postconditions**: Both gates are removed from the instruction sequence.
* **Expected Invariant**: Unitary mapping remains identical.
* **Owner**: Optimizer Engine Team
* **Source File**: `src/opt/quantum_rules.c`
* **Function**: `opt_cnot_cancel`
* **Introduced Commit**: `0fc8e0bd`
* **Last Verified**: `2026-07-20`
* **Last Symbolic Proof**: `N/A`
* **Coverage %**: `N/A`
* **Known Limitations**: None
* **Dependencies**: None
* **Symbolic Proof Status**: `[Pending Verification]`
* **Numerical Test Status**: `[Verified]` (Covered in `QAlgo-CNOT-Cancel-1`)
* **Coverage Status**: `[Pending Verification]`

---

### Rule 4: Swap Cancellation
* **Rule ID**: `RULE-QOPT-004`
* **Rewrite**: $SWAP(q_1, q_2) \cdot SWAP(q_1, q_2) \rightarrow I$
* **Preconditions**: Two consecutive SWAP gates acting on the identical target qubits.
* **Postconditions**: Both gates are removed from the instruction sequence.
* **Expected Invariant**: Unitary mapping remains identical.
* **Owner**: Optimizer Engine Team
* **Source File**: `src/opt/quantum_rules.c`
* **Function**: `opt_swap_cancel`
* **Introduced Commit**: `0fc8e0bd`
* **Last Verified**: `2026-07-20`
* **Last Symbolic Proof**: `N/A`
* **Coverage %**: `N/A`
* **Known Limitations**: None
* **Dependencies**: None
* **Symbolic Proof Status**: `[Pending Verification]`
* **Numerical Test Status**: `[Verified]` (Covered in `QAlgo-SWAP-Cancel-1`)
* **Coverage Status**: `[Pending Verification]`

---

## 🛑 Corrigendum

**2026-07-20**: Rules 1 through 4 were previously promoted to `[Verified]` based on python numerical test matrix identities. However, this promotion was invalid: the optimizer pass C implementation (`src/opt/quantum_rules.c`) does not actually exist in the codebase. Thus, the tests did not run against the actual optimizer output (the tests evaluated handwritten literals). All four rules have been retroactively demoted back to `[Pending Verification]` until the actual C optimizer passes are implemented, wired to a test harness, and pass fault injection. Coverage claims have also been struck.
RESOLVED 2026-07-20: C implementation landed at src/opt/quantum_rules.c, verified via fault-injectable optimizer-in-the-loop harness (logs: /tmp/qopt_gate_*.log, commit 0fc8e0bd).
