#!/usr/bin/env bash
# evaluate_impact.sh — Policy-aware exit-code evaluation for impact attribution
set -u

# Run the command passed as arguments
"$@"
rc=$?

echo "--- Policy Evaluation ---"
case "$rc" in
  0)
    echo "VERDICT: Impact is acceptable (NONE/LOW). Proceeding."
    exit 0
    ;;
  1)
    echo "VERDICT: Impact is MEDIUM (review required)."
    # For Milestone 1.0 local execution, warnings are logged but non-blocking (exit 0)
    # unless strictly set as release-blocking. Let's make it blocking for safe release.
    exit 1
    ;;
  2)
    echo "VERDICT: Impact is HIGH (release blocked)." >&2
    exit 2
    ;;
  *)
    echo "VERDICT: Operational failure or crash in impact tool (exit: $rc)." >&2
    exit 3
    ;;
esac
