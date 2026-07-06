#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FAIL=0

for d in "$ROOT"/*/; do
  [[ -f "${d}/input.ir" ]] || continue
  [[ -f "${d}/expected.ir" ]] || continue

  actual="${d}/actual.ir"
  echo "[RUN] $(basename "$d")"
  zcc-opt --pass=sccp --pass=cfg_simplify "${d}/input.ir" -o "$actual"

  if ! diff -u "${d}/expected.ir" "$actual"; then
    echo "[FAIL] $(basename "$d")"
    FAIL=1
  else
    echo "[OK]   $(basename "$d")"
  fi
done

exit $FAIL
