# Day 4 — SCCP MVP

## Objective
Implement SCCP core propagation and branch executability.

## Tasks
- [ ] Lattice UNDEF/CONST/OVERDEFINED
- [ ] SSA + CFG worklists
- [ ] Executable-edge tracking
- [ ] Constant branch pruning
- [ ] Constant materialization in IR

## Acceptance Criteria
- [ ] SCCP tests 01–05 green
- [ ] PHI meets executable preds only
- [ ] unknown conditions not pruned incorrectly

## Evidence
```bash
make -C tests/opt/sccp test-normalized
```
