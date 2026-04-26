#!/usr/bin/env bash
set -euo pipefail

ZCC="${ZCC:-./zcc}"
CC_ORACLE="${CC_ORACLE:-cc}"

python3 tools/gen_layout_oracle.py

for file in tests/layout/oracle_case_*.c; do
  echo "[ORACLE] $file"

  oracle_bin="/tmp/oracle-layout"
  zcc_bin="/tmp/zcc-layout"

  "$CC_ORACLE" "$file" -o "$oracle_bin"
  "$ZCC" --target=x86_64-linux-gnu "$file" -o "$zcc_bin"

  oracle_out="$("$oracle_bin")"
  zcc_out="$("$zcc_bin")"

  if [ "$oracle_out" != "$zcc_out" ]; then
    echo "layout oracle mismatch: $file"
    echo "--- oracle"
    printf '%s\n' "$oracle_out"
    echo "--- zcc"
    printf '%s\n' "$zcc_out"
    exit 1
  fi
done

echo "[DIFFERENTIAL LAYOUT] oracle match complete"