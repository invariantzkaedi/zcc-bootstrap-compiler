#!/usr/bin/env bash
set -euo pipefail
OWNER_REPO="${1:?usage: create_labels.sh owner/repo}"

create_label () {
  local name="$1" color="$2" desc="$3"
  gh label create "$name" --repo "$OWNER_REPO" --color "$color" --description "$desc" 2>/dev/null || \
  gh label edit "$name" --repo "$OWNER_REPO" --color "$color" --description "$desc"
}

create_label compiler 1D76DB "Compiler core changes"
create_label optimizer 5319E7 "IR optimization passes"
create_label infra 0052CC "Build/tooling infrastructure"
create_label ci 0E8A16 "CI workflows and gates"
create_label perf FBCA04 "Performance benchmarking/gates"
create_label qa BFD4F2 "Test coverage and validation"
create_label release D93F0B "Release management"
create_label ops C5DEF5 "Operational process/reporting"
create_label blocked B60205 "Blocked by dependency"
create_label meta 6F42C1 "Meta coordination issue"
create_label tracking A2EEEF "Roadmap tracking"
