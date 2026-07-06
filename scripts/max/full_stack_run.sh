#!/usr/bin/env bash
set -euo pipefail

OWNER_REPO="${1:?usage: full_stack_run.sh owner/repo}"

bash scripts/max/full_quality_gate_local.sh
bash scripts/max/full_perf_gate_local.sh
bash scripts/max/hardening_audit.sh "$OWNER_REPO"
bash scripts/max/weekly_ops_report.sh out/ops/weekly_report.md

echo "FULL STACK RUN COMPLETE ✅"
