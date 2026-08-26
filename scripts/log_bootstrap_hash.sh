#!/usr/bin/env bash
# Item 4: automated per-commit bootstrap-hash logger with host-toolchain identity.
# Appends one row to BOOTSTRAP_BASELINES.tsv on each invocation (CI-friendly).
# Verdict logic: compares against the last locked row for the same toolchain key
# and exits nonzero on unexplained drift.
set -euo pipefail
cd "$(dirname "$0")/.."
LEDGER="BOOTSTRAP_BASELINES.tsv"

BUILD_LOG=$(mktemp)
make selfhost >"$BUILD_LOG" 2>&1 || { echo "SELFHOST FAILED — cannot baseline"; cat "$BUILD_LOG"; exit 2; }

HASH=$(md5sum zcc2.s | cut -d' ' -f1)
COMMIT=$(git rev-parse --short=8 HEAD 2>/dev/null || echo "nogit")
CC_ID=$( ${CC:-gcc} --version | head -1 )
UNAME=$(uname -srm)
ELISIONS=$(grep -oE '[0-9]+ elided' "$BUILD_LOG" | head -1 | grep -oE '[0-9]+' || echo "?")
rm -f "$BUILD_LOG"
DATE=$(date -u +%Y-%m-%dT%H:%M:%SZ)
KEY="${CC_ID}"

COMMENT="${1:-}"

if [ ! -f "$LEDGER" ]; then
  printf "date\tcommit\tmd5_zcc2s\telisions\ttoolchain\tuname\tcomment\n" > "$LEDGER"
fi

# Drift check against last row with same compiler toolchain and uname environment
PRIOR=$(awk -F'\t' -v cc="$CC_ID" -v u="$UNAME" 'NR>1 && $5==cc && $6==u {h=$3} END{print h}' "$LEDGER" || true)
if [ -z "$PRIOR" ] && echo "$UNAME" | grep -q "azure"; then
  PRIOR=$(awk -F'\t' -v cc="$CC_ID" 'NR>1 && $5==cc && $6~/azure/ {h=$3} END{print h}' "$LEDGER" || true)
fi
if [ -z "$PRIOR" ]; then
  PRIOR=$(awk -F'\t' -v cc="$CC_ID" 'NR>1 && $5==cc {h=$3} END{print h}' "$LEDGER" || true)
fi

PRIOR=$(echo "$PRIOR" | tr -d '\r\n[:space:]')
HASH=$(echo "$HASH" | tr -d '\r\n[:space:]')

printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "$DATE" "$COMMIT" "$HASH" "$ELISIONS" "$CC_ID" "$UNAME" "$COMMENT" >> "$LEDGER"

echo "BOOTSTRAP HASH: $HASH  commit=$COMMIT  elisions=$ELISIONS"
echo "TOOLCHAIN: $CC_ID | $UNAME"
if [ -n "$PRIOR" ] && [ "$PRIOR" != "$HASH" ]; then
  echo "DRIFT DETECTED vs prior same-toolchain baseline $PRIOR"
  echo "BOOTSTRAP_HASH_EXIT=1 (codegen drifted or units added — review required)"
  exit 1
fi
echo "BOOTSTRAP_HASH_EXIT=0 (stable or first baseline for this toolchain)"
