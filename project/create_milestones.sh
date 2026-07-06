#!/usr/bin/env bash
set -euo pipefail
OWNER_REPO="${1:?usage: create_milestones.sh owner/repo}"

mk () {
  local title="$1" desc="$2"
  gh api -X POST "repos/${OWNER_REPO}/milestones" -f title="$title" -f description="$desc" >/dev/null 2>&1 || true
}

mk "M1 Correctness Foundation" "Verifier + InstCombine MVP + SCCP MVP + CFG simplify + correctness CI"
mk "M2 Measurable Perf Gate" "Metrics + robust benchmark harness + statistical perf gate + baseline pinning"
mk "M3 Optimization Depth" "Rule expansion + mixed pipeline tests + iterative budget controls"
mk "M4 Phase-3/4 Readiness" "Clone/remap + SSA helpers + canonical loop validator + flagged unroll/inline + release readiness"
