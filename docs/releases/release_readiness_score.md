# Release Readiness Score Model (v1)

Start at 100 points.

## Deductions
- Correctness gate failed: -50
- Perf gate failed: -30
- Flake rate >5%: -10
- Runtime geomean <0%: -10
- Any open P0 optimizer/verifier issue: -20
- Missing rollback playbook: -10

## Bands
- 90–100: Ready
- 75–89: Caution (release manager approval required)
- <75: Not ready
