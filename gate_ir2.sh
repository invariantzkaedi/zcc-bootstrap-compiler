#!/usr/bin/env bash
# Gate IR-2 — ZCC IR Bridge determinism check.
# Per zkaedi-zcc-ir-bridge SKILL.md v1.0.4.
#
# PASS condition: same input -> byte-identical IR across:
#   (1) two consecutive runs in the same env
#   (2) MALLOC_PERTURB_=42 (catches malloc-order non-determinism)
#   (3) running from a different cwd (catches path/getcwd leak)
#
# Usage:
#   bash gate_ir2.sh                              # defaults: zcc_pp.c
#   SRC=tests/foo.c bash gate_ir2.sh              # smaller probe
#   ZCC_DIR=/path bash gate_ir2.sh                # different worktree
#
# Exit codes: 0 = PASS  |  1 = FAIL  |  2 = setup error

set -u

ZCC_DIR="${ZCC_DIR:-/mnt/h/__DOWNLOADS/selforglinux}"
SRC="${SRC:-${ZCC_DIR}/zcc_pp.c}"
OUT="${OUT:-/tmp/ir_gate2}"
ZCC="${ZCC_DIR}/zcc"

mkdir -p "$OUT"
rm -f "$OUT"/*.json "$OUT"/*.err

# --- preflight ---------------------------------------------------------------
[ -x "$ZCC" ]  || { echo "SETUP FAIL: $ZCC not executable"; exit 2; }
[ -f "$SRC" ]  || { echo "SETUP FAIL: $SRC not found"; exit 2; }

emit_ir () {
    # $1 = output label  ;  rest = env-var prefix (or empty)
    local label="$1"; shift
    local out="$OUT/${label}.json"
    local err="$OUT/${label}.err"
    # NOTE: absolute paths only. The cwd-change test runs from /tmp and we must
    # not let cwd resolve any of: zcc binary, source, or output.
    "$@" env ZCC_IR_BACKEND=1 ZCC_IR_FLUSH=1 "$ZCC" "$SRC" -o /dev/null \
        > "$out" 2> "$err"
    local rc=$?
    local lines sha
    lines=$(wc -l < "$out")
    sha=$(sha256sum < "$out" | cut -d' ' -f1)
    printf "  %-12s  rc=%d  lines=%-6d  sha=%s\n" "$label" "$rc" "$lines" "${sha:0:16}"
    if [ "$lines" -eq 0 ] || [ "$rc" -ne 0 ]; then
        echo "    !! empty or non-zero exit. stderr tail:"
        tail -5 "$err" | sed 's/^/       /'
    fi
}

verdict () {
    # $1 label  $2 file_a  $3 file_b
    local label="$1" a="$2" b="$3"
    local sha_a sha_b
    sha_a=$(sha256sum < "$a" | cut -d' ' -f1)
    sha_b=$(sha256sum < "$b" | cut -d' ' -f1)
    if [ "$sha_a" = "$sha_b" ]; then
        printf "  %-32s  PASS\n" "$label"
        return 0
    fi
    printf "  %-32s  FAIL\n" "$label"
    local total
    total=$(diff "$a" "$b" | wc -l)
    echo "    diff lines total: $total"
    echo "    first 20:"
    diff "$a" "$b" | head -20 | sed 's/^/      /'
    return 1
}

# --- runs --------------------------------------------------------------------
echo "Gate IR-2 — IR emission determinism"
echo "  ZCC:    $ZCC"
echo "  SRC:    $SRC"
echo "  OUT:    $OUT"
echo ""
echo "Runs:"

cd "$ZCC_DIR"
emit_ir "A_baseline"
emit_ir "B_baseline"
emit_ir "P_perturb"     env MALLOC_PERTURB_=42

# Cwd-change run: jump out of ZCC_DIR entirely.
( cd /tmp && emit_ir "C_cwd" )

echo ""
echo "Verdicts:"

fail=0
verdict "baseline    (A == B)" "$OUT/A_baseline.json" "$OUT/B_baseline.json" || fail=1
verdict "malloc-order (A == P)" "$OUT/A_baseline.json" "$OUT/P_perturb.json"  || fail=1
verdict "cwd-change  (A == C)" "$OUT/A_baseline.json" "$OUT/C_cwd.json"       || fail=1

echo ""
if [ $fail -eq 0 ]; then
    echo "GATE IR-2: PASS — IR emission deterministic across all 3 perturbations."
    exit 0
fi

echo "GATE IR-2: FAIL"
echo ""
echo "Investigation hints:"
echo "  - grep the diff for memory addresses:  diff A.json B.json | grep -oE '0x[0-9a-f]{6,}'"
echo "  - grep for pids/timestamps:            diff A.json B.json | grep -oE '\\b[0-9]{6,}\\b'"
echo "  - grep for absolute paths:             diff A.json B.json | grep -E '/(mnt|tmp|home)/'"
echo "  - check stderr for warnings:           cat $OUT/*.err"
exit 1
