# QEC Semantics Specification (Canonical)

Version: 1.0.0  
Status: Normative  
Owners: Compiler/QEC Team

## 1. Scope

This document defines the canonical stabilizer propagation semantics used by:
- Matrix oracle (`U P U†`) — primary truth model
- Symplectic/tableau oracle — secondary truth model
- C implementation (`src/ir_to_zqec.c`) — production code path

All verification and CI checks MUST conform to this spec.

## 2. Conventions

### 2.1 Qubit indexing
- Qubits are indexed as integers `[0, n-1]`.
- **Endianness**:
  - [x] big-endian tensor ordering  
Document exact Kronecker placement in code comments.

### 2.2 Pauli representation
Single-qubit Paulis:
- `I, X, Y, Z`
- Global phase ignored unless explicitly noted.
- Equality in most tests is “up to sign” (`P == ±Q`).

### 2.3 Stabilizer frame
For each qubit `q`:
- `x[q] ∈ {0,1}`
- `z[q] ∈ {0,1}`
Optional phase bit (if implemented): `r ∈ {0,1}`.

## 3. Gate conjugation rules

### 3.1 Hadamard H(q)
- `X_q ↔ Z_q`
- `Y_q -> -Y_q` (phase/sign only)

### 3.2 Phase S(q)
- `X_q -> Y_q = X_q Z_q` (phase/sign omitted in frame-only checks)
- `Z_q -> Z_q`

### 3.3 CNOT(c, t)
- `X_c -> X_c X_t`
- `Z_t -> Z_c Z_t`
- `X_t -> X_t`
- `Z_c -> Z_c`

Bit update form:
- `x[t] ^= x[c]`
- `z[c] ^= z[t]`

### 3.4 CZ(a, b)
- `Z_a, Z_b` commute unchanged
- `X_a -> X_a Z_b`
- `X_b -> Z_a X_b`

Bit update form:
- `z[b] ^= x[a]`
- `z[a] ^= x[b]`

## 4. Syndrome semantics

### 4.1 Deterministic verification mode
- Random/noisy injection MUST be disabled in deterministic test mode.
- Same input + same seed MUST produce identical syndrome/correction outputs.

### 4.2 Error class precedence
Decoder precedence:
1. Composite Y-like condition (both X/Z thresholds exceeded)
2. X-only
3. Z-only
4. No error

## 5. Tie-break policy (normative)

When multiple corrections are degenerate/equivalent:
1. Minimum weight correction
2. Lexicographic qubit order
3. Fixed Pauli order `X < Y < Z` (or chosen order, but fixed)
4. Stable deterministic output across runs/threads

## 6. Equivalence and tolerances

- Matrix comparisons use `atol = 1e-9` unless otherwise documented.
- Stabilizer comparisons are up to sign unless phase-aware tests enabled.
- Any tolerance override MUST be documented per test.

## 7. Truth hierarchy

1. Matrix oracle (`U P U†`) — source of truth
2. Independent tableau implementation
3. C implementation and codegen traces

Disagreement handling:
- If C disagrees with matrix and tableau agree → C bug
- If tableau disagrees with matrix → tableau bug/spec bug
- If matrix disagrees with both → likely convention mismatch (indexing/ordering)

## 8. Versioning

Any semantics changes MUST:
- bump document version
- include migration note
- regenerate affected golden traces
- link proof/evidence in docs/evidence/YYYY-MM-DD/
