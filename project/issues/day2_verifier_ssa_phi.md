# Day 2 — Verifier SSA/PHI

## Objective
Complete verifier core for SSA and PHI invariants.

## Tasks
- [ ] Implement `verify_ssa`
  - undefined value use detection
  - multiple definitions detection
- [ ] Implement `verify_phi_wellformed`
  - arity matches predecessor count
  - predecessor labels valid and unique
  - incoming value type matches phi type
  - phi nodes precede non-phi nodes in block
- [ ] Add/verify stable errors:
  - `E_SSA_001`, `E_SSA_002`
  - `E_PHI_001..E_PHI_005`

## Acceptance Criteria
- [ ] full negative verifier suite passes (expected fail semantics)
- [ ] full positive verifier suite passes
- [ ] deterministic diagnostics maintained

## Evidence
```bash
make -C tests/verify test-negative
make -C tests/verify-positive test-positive
```
