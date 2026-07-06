#!/usr/bin/env bash
set -euo pipefail
OWNER_REPO="${1:?usage: create_kickoff_issues.sh owner/repo}"

create_issue () {
  local title="$1" file="$2" labels="$3" milestone="$4"
  gh issue create --repo "$OWNER_REPO" \
    --title "$title" \
    --body-file "$file" \
    --label "$labels" \
    --milestone "$milestone"
}

create_issue "M1 Execution Start — Correctness Foundation" "project/issues/m1_execution_start.md" "meta,tracking,compiler,optimizer,ci" "M1 Correctness Foundation"
create_issue "Day 1 — Verifier CFG/Terminators" "project/issues/day1_verifier_cfg_terminators.md" "compiler,optimizer" "M1 Correctness Foundation"
create_issue "Day 2 — Verifier SSA/PHI" "project/issues/day2_verifier_ssa_phi.md" "compiler,optimizer" "M1 Correctness Foundation"
create_issue "Day 3 — InstCombine MVP" "project/issues/day3_instcombine_mvp.md" "compiler,optimizer" "M1 Correctness Foundation"
create_issue "Day 4 — SCCP MVP" "project/issues/day4_sccp_mvp.md" "compiler,optimizer" "M1 Correctness Foundation"
create_issue "Day 5 — CFG Simplify + CI Closeout" "project/issues/day5_cfg_simplify_ci_close.md" "compiler,optimizer,ci,infra" "M1 Correctness Foundation"

create_issue "M2 Kickoff — Measurable Performance Gate" "project/issues/m2_kickoff_perf_gate.md" "meta,tracking,perf,ci" "M2 Measurable Perf Gate"
create_issue "M3 Kickoff — Optimization Depth" "project/issues/m3_kickoff_optimization_depth.md" "meta,tracking,optimizer,perf" "M3 Optimization Depth"
create_issue "M4 Kickoff — Phase 3/4 Readiness" "project/issues/m4_kickoff_phase3_4_readiness.md" "meta,tracking,compiler,optimizer,release" "M4 Phase-3/4 Readiness"
