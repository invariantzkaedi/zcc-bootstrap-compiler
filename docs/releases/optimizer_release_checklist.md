# Optimizer Release Checklist

## Pre-Release
- [ ] All required correctness checks green
- [ ] Perf gate green against pinned baseline
- [ ] No open P0/P1 correctness bugs
- [ ] Known limitations documented
- [ ] Rollback plan validated

## Release
- [ ] Tag created and pushed
- [ ] Release note drafted
- [ ] Artifact links attached (correctness + perf)
- [ ] Ownership/escalation listed

## Post-Release (48h)
- [ ] Monitor CI failure delta
- [ ] Monitor benchmark drift
- [ ] Confirm no spike in regression incidents
