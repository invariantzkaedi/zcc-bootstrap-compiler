#!/usr/bin/env bash
set -euo pipefail

OUT="${1:-out/ops/weekly_report.md}"
mkdir -p "$(dirname "$OUT")"

cat > "$OUT" << 'MD'
# Weekly Optimization Status

## Completed
- (fill)

## In Progress
- (fill)

## Blockers
- (fill)

## Correctness Status
- verifier negative: PASS/FAIL
- verifier positive: PASS/FAIL
- instcombine normalized: PASS/FAIL
- sccp normalized: PASS/FAIL

## Performance Status
- compile geomean overhead: x%
- runtime geomean delta: y%
- hard regressions: n

## Next Week
- (fill)
MD

echo "Wrote $OUT"
