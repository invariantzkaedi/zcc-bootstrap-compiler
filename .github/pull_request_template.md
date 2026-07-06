## Summary
<!-- What does this PR change and why? -->

## Milestone/Track
- Milestone: <!-- M1/M2/M3/M4 -->
- Track: <!-- Compiler Core / Optimizer / Tooling-CI / Performance / Release-Ops -->
- Related issue(s): <!-- #123 -->

## Change Type
- [ ] Verifier
- [ ] InstCombine
- [ ] SCCP
- [ ] CFG Simplify
- [ ] CI/Infra
- [ ] Perf Harness
- [ ] Docs/Release

## Implementation Notes
<!-- Key design decisions, constraints, tradeoffs -->

## Safety/Correctness
- [ ] Verifier run pre/post mutating pass in debug mode
- [ ] No undefined-value or multi-def regressions introduced
- [ ] PHI invariants preserved
- [ ] CFG pred/succ invariants preserved

## Test Evidence (Required)
### Verifier Negative
```bash
make -C tests/verify test-negative
```
Result:
<!-- paste -->

### Verifier Positive
```bash
make -C tests/verify-positive test-positive
```
Result:
<!-- paste -->

### InstCombine
```bash
make -C tests/opt/instcombine test-normalized
```
Result:
<!-- paste -->

### SCCP
```bash
make -C tests/opt/sccp test-normalized
```
Result:
<!-- paste -->

## Perf Evidence (If optimizer behavior changed)
```bash
scripts/bench/run_robust_benchmarks.sh ...
python3 scripts/bench/evaluate_robust_thresholds.py ...
```
- Compile geomean overhead:
- Runtime geomean delta:
- Regressed benchmark count:
- Significant regressions:

## Artifacts
- Correctness artifact URL:
- Perf artifact URL:

## Risk Assessment
- Risk level: <!-- Low/Med/High -->
- Potential failure mode:
- Mitigation:
- Rollback plan:

## Reviewer Checklist
- [ ] Acceptance criteria from linked issue are met
- [ ] Tests are sufficient and meaningful
- [ ] Diagnostics are stable/actionable
- [ ] CI/perf gates pass
- [ ] Docs updated if behavior changed
