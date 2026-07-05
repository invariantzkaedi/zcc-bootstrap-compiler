#!/bin/bash
set -euo pipefail

emit_evidence() {
  local rule_id="${1:?rule id required}"
  local stage="${2:?stage required}"
  local result="${3:?result required}"
  local target="${4:?target required}"
  local test_name="${5:-$target}"

  local spec_dir="${SPEC_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
  local run_id="${RUN_ID:-${GITHUB_RUN_ID:-default-run}}"
  if [[ -n "${GITHUB_RUN_ATTEMPT:-}" && -z "${RUN_ID:-}" ]]; then
    run_id="${run_id}-${GITHUB_RUN_ATTEMPT}"
  fi

  local -a args=(
    pnpm --dir "$spec_dir" exec tsx scripts/emit-evidence.ts
    --run-id "$run_id"
    --stage "$stage"
    --rules "$rule_id"
    --target "$target"
    --result "$result"
    --test "$test_name"
  )

  if [[ -n "${WAIVER_ID:-}" ]]; then
    args+=(--waiver-id "$WAIVER_ID")
  fi
  if [[ -n "${WAIVER_OWNER:-}" ]]; then
    args+=(--waiver-owner "$WAIVER_OWNER")
  fi
  if [[ -n "${WAIVER_EXPIRY:-}" ]]; then
    args+=(--waiver-expiry "$WAIVER_EXPIRY")
  fi

  "${args[@]}"
}
