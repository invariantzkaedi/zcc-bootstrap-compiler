#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
NORM="$ROOT/../mixed/normalize_ir.py"
FAIL=0

for d in "$ROOT"/*/; do
  [[ -f "${d}/input.ir" ]] || continue
  [[ -f "${d}/expected.ir" ]] || continue

  actual="${d}/actual.ir"
  expn="${d}/expected.norm.ir"
  actn="${d}/actual.norm.ir"

  echo "[RUN] $(basename "$d")"
  "$ROOT/../../../zcc-opt" --enable-inline-mvp --opt-metrics-out "$ROOT/../../../opt_metrics.csv" "${d}/input.ir" -o "$actual"

  python3 "$NORM" "${d}/expected.ir" > "$expn"
  python3 "$NORM" "$actual" > "$actn"

  if ! diff -u "$expn" "$actn"; then
    echo "[FAIL] $(basename "$d")"
    FAIL=1
  else
    echo "[OK]   $(basename "$d")"
  fi
done

exit $FAIL
