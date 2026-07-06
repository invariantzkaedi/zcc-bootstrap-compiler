# Day 3 — InstCombine MVP

## Objective
Implement 15 baseline InstCombine rules with safe rewrite semantics.

## Tasks
- [ ] Implement dispatcher + rule application
- [ ] Implement 15 rules from roadmap
- [ ] Ensure no invalid reassociation on overflow-sensitive cases
- [ ] Rebuild or repair def-use after rewrites
- [ ] Keep transform deterministic

## Acceptance Criteria
- [ ] `tests/opt/instcombine` normalized suite green
- [ ] verifier passes after pass execution
- [ ] no regressions in unrelated tests

## Evidence
```bash
make -C tests/opt/instcombine test-normalized
```
