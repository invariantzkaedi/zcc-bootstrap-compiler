#!/usr/bin/env bash
set -Eeuo pipefail

# Deterministic ABI probe for struct slot collision class.
# Customize this to your repo test harness.
#
# Behavior:
# - Builds a focused probe program
# - Compiles with zcc + gcc reference
# - Compares observable behavior
#
# Exit non-zero on mismatch/failure.

PROBE_C="${PROBE_C:-tests/probes/struct_slot_collision.c}"
ZCC_BIN="${ZCC_BIN:-./zcc}"
OUTDIR="${OUTDIR:-/tmp/zcc-fi-probe}"
mkdir -p "$OUTDIR"

log() { printf '[fault_probe] %s\n' "$*"; }
die() { printf '[fault_probe][error] %s\n' "$*" >&2; exit 1; }

[[ -f "$PROBE_C" ]] || die "probe source missing: $PROBE_C"
[[ -x "$ZCC_BIN" ]] || die "zcc binary missing/executable: $ZCC_BIN"
command -v gcc >/dev/null || die "gcc required"

log "Compiling reference (gcc)..."
gcc -O0 -g "$PROBE_C" -o "$OUTDIR/ref.bin"

log "Compiling candidate (zcc)..."
"$ZCC_BIN" "$PROBE_C" -S -o "$OUTDIR/zcc.s"
gcc -O0 "$OUTDIR/zcc.s" -o "$OUTDIR/zcc.bin"

log "Running both..."
set +e
"$OUTDIR/ref.bin" > "$OUTDIR/ref.out" 2>&1
R1=$?
"$OUTDIR/zcc.bin" > "$OUTDIR/zcc.out" 2>&1
R2=$?
set -e

echo "ref_exit=$R1"
echo "zcc_exit=$R2"

if [[ $R1 -ne $R2 ]]; then
  log "Exit mismatch"
  diff -u "$OUTDIR/ref.out" "$OUTDIR/zcc.out" || true
  exit 21
fi

if ! diff -u "$OUTDIR/ref.out" "$OUTDIR/zcc.out"; then
  log "Output mismatch"
  exit 22
fi

log "Probe PASS"
exit 0
