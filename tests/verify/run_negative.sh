#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FAIL=0

for d in "$ROOT"/*/; do
  [[ -f "${d}/invalid.ir" ]] || continue
  [[ -f "${d}/expected_error.txt" ]] || continue

  echo "[RUN] $(basename "$d")"
  stderr_file="${d}/stderr.txt"

  set +e
  zcc-verify "${d}/invalid.ir" > /dev/null 2> "$stderr_file"
  rc=$?
  set -e

  if [[ $rc -eq 0 ]]; then
    echo "[FAIL] expected verifier failure, got success"
    FAIL=1
    continue
  fi

  expected="$(cat "${d}/expected_error.txt")"
  if ! grep -Fqi "$expected" "$stderr_file"; then
    echo "[FAIL] expected stderr to contain: $expected"
    echo "---- stderr ----"
    cat "$stderr_file"
    echo "----------------"
    FAIL=1
  else
    echo "[OK]   $(basename "$d")"
  fi
done

exit $FAIL