#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

OWNER_REPO="${1:?usage: hardening_audit.sh owner/repo}"

echo "== Branch protection guidance ==="
echo "Use $POLICY_BRANCH_PROT"

echo "== Required workflows present ==="
gh workflow list --repo "$OWNER_REPO" | \
  grep -E "IR Opt Quality Gate|M1 Daily Check|Nightly Regression Watchdog|Update Command Center|ProjectV2"

echo "== Policy files ==="
test -f "$POLICY_MERGE_GATE"
test -f "$POLICY_BRANCH_PROT"
test -f "$POLICY_EXCEPTION_TPL"
echo "policy files OK"

echo "== Scripts executable check ==="
chmod +x "$PRE_PUSH_GUARD" scripts/legendary/run_all_the_things.sh || true
echo "script perms normalized"

echo "Hardening audit complete."
