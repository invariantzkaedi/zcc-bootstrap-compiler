# Day 1 — Verifier CFG/Terminators

## Objective
Implement and validate CFG + terminator verification baseline.

## Tasks
- [ ] Implement `verify_terminators`
  - exactly one terminator per basic block
  - report missing/multiple terminators
- [ ] Implement `verify_cfg`
  - successor targets exist
  - predecessor/successor consistency
  - entry block has no predecessors
- [ ] Add/verify stable errors:
  - `E_TERM_001`, `E_TERM_002`, `E_CFG_001`, `E_CFG_002`, `E_CFG_003`
- [ ] Ensure deterministic stderr format

## Acceptance Criteria
- [ ] `tests/verify/01..03` pass as expected failures
- [ ] no false failures on positive fixtures touching CFG/terminators
- [ ] diagnostics include function + block id

## Evidence
Commands:
```bash
make -C tests/verify test-negative
make -C tests/verify-positive test-positive
```
Paste output + artifact links.
