#!/usr/bin/env bash
# Gate IR-1 — ZCC IR Bridge coverage check.
set -u
ZCC_DIR="${ZCC_DIR:-/mnt/g/zccMAIN/zcc}"
ZCC="${ZCC_DIR}/zcc"
OUT="${OUT:-/tmp/ir_gate1}"
XFAIL_FILE="${ZCC_DIR}/gate_ir1_xfail.txt"
mkdir -p "$OUT"
rm -f "$OUT"/*.ir "$OUT"/*.err
[ -x "$ZCC" ] || { echo "SETUP FAIL: $ZCC not executable"; exit 2; }

declare -A XFAIL_SET
if [ -f "$XFAIL_FILE" ]; then
    while IFS= read -r line; do
        [[ "$line" =~ ^# ]] && continue
        [[ -z "$line" ]] && continue
        XFAIL_SET["$line"]=1
    done < "$XFAIL_FILE"
fi

pass=0; fail=0; xfail=0; xpass=0

probe () {
    local name="$1" src="$2"
    local out="$OUT/${name}.ir" err="$OUT/${name}.err"
    ZCC_EMIT_IR=1 "$ZCC" "$src" -o /tmp/_g1.s >"$out" 2>"$err"
    local rc=$?
    local lines=$(cat "$out" | wc -l)
    local ok=0
    [ "$rc" -eq 0 ] && [ "$lines" -gt 0 ] && ok=1
    if [ "${XFAIL_SET["$name"]+_}" ]; then
        if [ $ok -eq 1 ]; then
            printf "  XPASS %-30s lines=%d\n" "$name" "$lines"; xpass=$((xpass+1))
        else
            printf "  xfail %-30s rc=%d\n" "$name" "$rc"; xfail=$((xfail+1))
        fi
    else
        if [ $ok -eq 1 ]; then
            printf "  PASS  %-30s lines=%d\n" "$name" "$lines"; pass=$((pass+1))
        else
            printf "  FAIL  %-30s rc=%d lines=%d\n" "$name" "$rc" "$lines"
            tail -3 "$err" | sed 's/^/         /'
            fail=$((fail+1))
        fi
    fi
}

echo "Gate IR-1 — IR emission coverage"
echo "  ZCC: $ZCC"
echo "  XFAIL: $XFAIL_FILE"
echo ""
cd "$ZCC_DIR"
probe smoke_hello           "${ZCC_DIR}/zcc-level1/tools/true.c"
probe smoke_echo            "${ZCC_DIR}/zcc-level1/tools/false.c"
probe smoke_wc              "${ZCC_DIR}/zcc-level2/tests/test_double_bug.c"
probe smoke_zcc_pp          "${ZCC_DIR}/part0_pp.c"
probe smoke_part2           "${ZCC_DIR}/part2.c"
probe smoke_part3           "${ZCC_DIR}/part3.c"
probe smoke_part4           "${ZCC_DIR}/part4.c"
probe smoke_part5           "${ZCC_DIR}/part5.c"
probe smoke_compiler_passes "${ZCC_DIR}/compiler_passes.c"

echo ""
echo "Results: PASS=$pass FAIL=$fail xfail=$xfail XPASS=$xpass"
if [ $fail -eq 0 ] && [ $xpass -eq 0 ]; then
    echo "GATE IR-1: PASS — $pass baseline probes green, $xfail xfails tolerated."
    exit 0
fi
[ $xpass -gt 0 ] && { echo "GATE IR-1: XPASS — promote to baseline, remove from xfail."; exit 1; }
echo "GATE IR-1: FAIL — $fail baseline probe(s) failed."
exit 1
