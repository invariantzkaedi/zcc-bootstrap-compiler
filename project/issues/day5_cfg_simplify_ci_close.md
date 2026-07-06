# Day 5 — CFG Simplify + CI Closeout

## Objective
Finish M1 by wiring cleanup and proving full gate stability.

## Tasks
- [ ] Implement unreachable block elimination
- [ ] Repair pred/succ and PHI incomings
- [ ] Run all correctness suites
- [ ] Validate artifact completeness in CI
- [ ] Ensure required checks enforced in branch protection

## Acceptance Criteria
- [ ] full correctness workflow green
- [ ] all artifacts available for triage
- [ ] M1 checklist updated to done

## Evidence
```bash
make -C tests/verify test-negative
make -C tests/verify-positive test-positive
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp test-normalized
```
