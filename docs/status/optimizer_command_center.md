# ⚡ Optimizer Command Center

## Live Gates
- Correctness Gate: ![Correctness](https://github.com/OWNER/REPO/actions/workflows/ir-opt-quality-gate.yml/badge.svg)
- Perf Gate: ![Perf](https://github.com/OWNER/REPO/actions/workflows/ir-opt-bench.yml/badge.svg)
- Project Sync: ![Project Sync](https://github.com/OWNER/REPO/actions/workflows/projectv2-auto-add.yml/badge.svg)

## Current Readiness
- Release readiness score: `{{RELEASE_SCORE}}/100`
- Correctness: `{{CORRECTNESS_STATUS}}`
- Performance: `{{PERF_STATUS}}`
- Flake Rate: `{{FLAKE_RATE}}`
- Last updated: `{{UPDATED_AT}}`

## Top Regressions
{{TOP_REGRESSIONS_TABLE}}

## Required Before Tag
- [ ] No P0/P1 open in optimizer/verifier
- [ ] Correctness required checks green
- [ ] Perf thresholds passing
- [ ] Rollback playbook validated
