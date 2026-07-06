# QEC Truth Hierarchy & Escalation

## Hierarchy
1. Matrix conjugation oracle (`U P U†`)
2. Independent tableau oracle
3. C/runtime propagation and decoder output

## Escalation matrix

- C != Matrix and Tableau == Matrix -> Fix C
- Tableau != Matrix and C == Matrix -> Fix Tableau
- C != Matrix and Tableau != Matrix:
  - check indexing conventions
  - check test harness tensor ordering
  - check sign/phase equivalence mode
  - then classify as spec ambiguity or dual bug

## Required evidence per mismatch

- seed
- n_qubits
- input Pauli (x,z bits and optional phase)
- gate sequence
- matrix result
- tableau result
- C result
- minimized sequence
- repro command
