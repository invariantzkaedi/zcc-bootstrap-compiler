# Triage Playbook (Fast Path)

## If verifier negative unexpectedly passes
- likely verifier regression
- run: `zcc-verify tests/verify/*/invalid.ir`
- inspect stderr mismatch

## If normalized IR diff fails
- check `actual.norm.ir` vs `expected.norm.ir`
- ensure transform legality (not naming drift)

## If perf gate fails
1. open `out/bench/summary.json`
2. inspect worst regressed benchmarks
3. check p-value significance
4. if non-significant + noisy, rerun once
5. if significant, bisect pass toggles
