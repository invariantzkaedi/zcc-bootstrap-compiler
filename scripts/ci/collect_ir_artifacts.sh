#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-out/ir-artifacts}"
mkdir -p "$OUT_DIR"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -f "$src" ]]; then
    mkdir -p "$(dirname "$dst")"
    cp -f "$src" "$dst"
  fi
}

for d in tests/verify/*/; do
  [[ -d "$d" ]] || continue
  base="$(basename "$d")"
  copy_if_exists "${d}/invalid.ir" "${OUT_DIR}/verify-negative/${base}/invalid.ir"
  copy_if_exists "${d}/stderr.txt" "${OUT_DIR}/verify-negative/${base}/stderr.txt"
  copy_if_exists "${d}/expected_error.txt" "${OUT_DIR}/verify-negative/${base}/expected_error.txt"
done

for d in tests/verify-positive/*/; do
  [[ -d "$d" ]] || continue
  base="$(basename "$d")"
  copy_if_exists "${d}/valid.ir" "${OUT_DIR}/verify-positive/${base}/valid.ir"
  copy_if_exists "${d}/stderr.txt" "${OUT_DIR}/verify-positive/${base}/stderr.txt"
done

for d in tests/opt/instcombine/*/; do
  [[ -d "$d" ]] || continue
  base="$(basename "$d")"
  copy_if_exists "${d}/input.ir" "${OUT_DIR}/instcombine/${base}/input.ir"
  copy_if_exists "${d}/expected.ir" "${OUT_DIR}/instcombine/${base}/expected.ir"
  copy_if_exists "${d}/actual.ir" "${OUT_DIR}/instcombine/${base}/actual.ir"
  copy_if_exists "${d}/expected.norm.ir" "${OUT_DIR}/instcombine/${base}/expected.norm.ir"
  copy_if_exists "${d}/actual.norm.ir" "${OUT_DIR}/instcombine/${base}/actual.norm.ir"
done

for d in tests/opt/sccp/*/; do
  [[ -d "$d" ]] || continue
  base="$(basename "$d")"
  copy_if_exists "${d}/input.ir" "${OUT_DIR}/sccp/${base}/input.ir"
  copy_if_exists "${d}/expected.ir" "${OUT_DIR}/sccp/${base}/expected.ir"
  copy_if_exists "${d}/actual.ir" "${OUT_DIR}/sccp/${base}/actual.ir"
  copy_if_exists "${d}/expected.norm.ir" "${OUT_DIR}/sccp/${base}/expected.norm.ir"
  copy_if_exists "${d}/actual.norm.ir" "${OUT_DIR}/sccp/${base}/actual.norm.ir"
done

copy_if_exists "opt_metrics.csv" "${OUT_DIR}/opt_metrics.csv"

echo "Artifacts collected at: $OUT_DIR"
