# QEC Preflight Evidence Report

Date: 2026-07-06  
Commit: `<fill>`  
Runner: `<local|github-actions>`

## Scope executed
- [ ] Schema validation
- [ ] Invariants battery
- [ ] Determinism contract
- [ ] Counterexample minimization smoke test

## Commands run
```bash
QEC_FUZZ_SEEDS=50 pytest -q tests/test_trace_schema_validation.py tests/test_invariants_battery.py tests/test_determinism_contract.py
```

## Results
- Total tests: `<fill>`
- Passed: `<fill>`
- Failed: `<fill>`
- Flaky observed: `<yes/no>`

## Artifacts
- `artifacts/` contents:
  - `<file1>`
  - `<file2>`

## Notes
- Any convention ambiguities:
- Any tolerance overrides:
- Any deferred items:

## Sign-off
- Reviewer 1:
- Reviewer 2:
