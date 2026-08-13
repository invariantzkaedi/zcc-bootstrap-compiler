#!/usr/bin/env bash
# ZKAEDI gate runner — evidence-grade exit capture.
# Usage: wsl -e bash scripts/gate.sh <logname> <command> [args...]
# Prints command, runs it, tees to /tmp/<logname>.log, prints REAL exit code.
set -u
LOG="/tmp/${1}.log"; shift
echo "GATE-CMD: $*" | tee "$LOG"
echo "GATE-DATE: $(date -Iseconds)" | tee -a "$LOG"
"$@" 2>&1 | tee -a "$LOG"
EC=${PIPESTATUS[0]}
echo "EXIT:${EC}" | tee -a "$LOG"
exit "${EC}"