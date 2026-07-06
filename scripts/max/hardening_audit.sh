#!/usr/bin/env bash
set -euo pipefail

OWNER_REPO="${1:?usage: hardening_audit.sh owner/repo}"

echo "== Branch protection guidance ==="
echo "Use docs/policies/branch_protection_checklist.md"

echo "== Required workflows present ==="
gh workflow list --repo "$OWNER_REPO" | grep -E "IR Opt Quality Gate|M1 Daily Check|Nightly Regression Watchdog|Update Command Center|ProjectV2"

echo "== Policy files ==="
test -f docs/policies/merge_gate_policy.md
test -f docs/policies/branch_protection_checklist.md
test -f docs/policies/exception_issue_template.txt
echo "policy files OK"

echo "== Scripts executable check ==="
chmod +x scripts/ci/pre_push_guard.sh scripts/legendary/run_all_the_things.sh || true
echo "script perms normalized"

echo "Hardening audit complete."
