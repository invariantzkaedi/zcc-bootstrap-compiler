#!/usr/bin/env bash
set -euo pipefail

echo "[PREFLIGHT] checking tools"

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required command: $1"
    exit 1
  fi
}

need python3
need grep
need sed
need awk

if command -v clang >/dev/null 2>&1; then
  echo "[PREFLIGHT] clang found: $(clang --version | head -n1)"
else
  echo "[PREFLIGHT] clang missing; cross-target layout oracle may fail"
fi

python3 tools/check_policy_filename.py

echo "[PREFLIGHT] ok"