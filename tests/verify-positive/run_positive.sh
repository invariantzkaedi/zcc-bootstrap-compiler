#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FAIL=0

for d in "$ROOT"/*/; do
  [[ -f "${d}/valid.ir" ]] || continue

  echo "[RUN] $(basename "$d")"
  stderr_file="${d}/stderr.txt"

  set +e
  zcc-verify "${d}/valid.ir" > /dev/null 2> "$stderr_file"
  rc=$?
  set -e

  if [[ $rc -ne 0 ]]; then
    echo "[FAIL] expected success, got failure"
    echo "---- stderr ----"
    cat "$stderr_file"
    echo "----------------"
    FAIL=1
  else
    echo "[OK]   $(basename "$d")"
  fi
done

exit $FAIL