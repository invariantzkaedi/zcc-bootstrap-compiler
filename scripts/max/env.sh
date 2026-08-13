#!/usr/bin/env bash
# scripts/max/env.sh — Single source of truth for all shared IDs, paths, and thresholds.
# Source this file at the top of every scripts/max/*.sh:
#   source "$(dirname "$0")/env.sh"
#
# NEVER hardcode these values in any other script. Reference the variable instead.

# ── Build artifact paths ────────────────────────────────────────────────────
ZCC_BASE="build/base/zcc"
ZCC_CAND="build/cand/zcc"
ZCC_BASE_OPT="build/base/zcc-opt"
ZCC_CAND_OPT="build/cand/zcc-opt"
ZCC_BASE_VERIFY="build/base/zcc-verify"
ZCC_CAND_VERIFY="build/cand/zcc-verify"

# ── Suite & benchmark paths ─────────────────────────────────────────────────
BENCH_SUITE="benchmarks/list.txt"
BENCH_SUMMARY="out/bench/summary.json"
BENCH_MD="out/bench/summary.md"

# ── Status / command-center output ──────────────────────────────────────────
STATUS_OUT_DIR="out/status"
CMD_CENTER_RENDERED="${STATUS_OUT_DIR}/optimizer_command_center.rendered.md"

# ── Tool paths ───────────────────────────────────────────────────────────────
CI_BIN_DIR=".ci/bin"
GEN_CMD_CENTER="scripts/status/generate_command_center.py"
EVAL_THRESHOLDS="scripts/bench/evaluate_robust_thresholds.py"
WATCHDOG="scripts/bench/regression_watchdog.py"
RENDER_MD="scripts/bench/render_summary_md.py"
RUN_ROBUST_BENCH="scripts/bench/run_robust_benchmarks.sh"
PRE_PUSH_GUARD="scripts/ci/pre_push_guard.sh"
CHECK_COVERAGE="scripts/ci/check_coverage.sh"
COLLECT_IR="scripts/ci/collect_ir_artifacts.sh"

# ── Benchmark run parameters ─────────────────────────────────────────────────
BENCH_WARMUP=3
BENCH_RUNS=25
BENCH_TRIM=0.10

# ── Threshold parameters (IDs so grep catches misuse of raw numbers) ─────────
THRESH_MAX_COMPILE_OVERHEAD_PCT=15.0    # compile-time overhead ceiling (%)
THRESH_MIN_RUNTIME_GEOMEAN_PCT=-12.0   # runtime geomean floor (%)
THRESH_MAX_REGRESSED_BENCHES=5         # max individual bench regressions allowed
THRESH_PER_BENCH_REGRESS_PCT=-25.0     # per-benchmark regression floor (%)
THRESH_ALPHA=0.05                       # statistical significance level
THRESH_MAX_HARD_REGRESSIONS=5          # watchdog hard-regression ceiling
THRESH_HARD_PCT=-25.0                   # watchdog hard-regression floor (%)

# ── Command-center fixed flags ────────────────────────────────────────────────
CMD_CENTER_FLAGS="--correctness-ok true --perf-ok true --flake-rate 0.0"

# ── IR artifact output dir ────────────────────────────────────────────────────
IR_ARTIFACTS_DIR="out/ir-artifacts"

# ── Build CFLAGS/LDFLAGS for coverage builds ──────────────────────────────────
CFLAGS_COV="-O0 -w -fno-asynchronous-unwind-tables -g0 -DZCC_REAL_TELEMETRY -Iinclude -I. -fprofile-arcs -ftest-coverage"
LDFLAGS_COV="-lm -lgcov"

# ── Required policy files (IDs guard against path drift) ─────────────────────
POLICY_MERGE_GATE="docs/policies/merge_gate_policy.md"
POLICY_BRANCH_PROT="docs/policies/branch_protection_checklist.md"
POLICY_EXCEPTION_TPL="docs/policies/exception_issue_template.txt"
PR_TEMPLATE=".github/pull_request_template.md"
