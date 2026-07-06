#!/usr/bin/env bash
set -euo pipefail

chmod +x \
  scripts/max/day1_operator.sh \
  scripts/max/full_quality_gate_local.sh \
  scripts/max/full_perf_gate_local.sh \
  scripts/max/open_pr_with_template.sh \
  scripts/max/hardening_audit.sh \
  scripts/max/weekly_ops_report.sh \
  scripts/max/full_stack_run.sh \
  scripts/max/bootstrap_full_safe.sh \
  scripts/max/ci_smoke_matrix.sh \
  scripts/max/pr_ready_check.sh \
  scripts/ci/pre_push_guard.sh \
  scripts/legendary/run_all_the_things.sh || true

echo "Executable bits set ✅"
