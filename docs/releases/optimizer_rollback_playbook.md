# Optimizer Rollback Playbook

## Trigger Conditions
- correctness regression in required suite
- significant perf regression above policy
- invalid IR generated in production path

## Rollback Steps
1. Revert to previous stable tag/commit.
2. Disable newly enabled optimization flags.
3. Re-run required correctness + perf workflows.
4. Publish incident with root cause owner and ETA.
5. Re-open blocked PRs after fix-forward plan approved.

## Validation
- [ ] Verifier suites green
- [ ] InstCombine/SCCP suites green
- [ ] Perf gate back within policy

## Record
- Rolled back from:
- Rolled back to:
- Incident ID:
- Follow-up issue:
