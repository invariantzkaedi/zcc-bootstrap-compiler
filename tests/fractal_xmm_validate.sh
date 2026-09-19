#!/usr/bin/env bash
# fractal_xmm_validate.sh — ZCC XMM codegen stress validator
#
# Usage:
#   bash tests/fractal_xmm_validate.sh                  # uses ./zcc and gcc reference
#   bash tests/fractal_xmm_validate.sh ./zcc_stage2     # test a specific stage
#
# Expected workdir: repository root (/mnt/h/__DOWNLOADS/zcc_github_upload) or tests/
# Exit codes:
#   0 = pass (XMM ops present, output matches GCC reference & golden MD5)
#   1 = ZCC compile failed
#   2 = required FP instruction missing from .s or assemble failed
#   3 = output diverges from GCC reference or golden MD5
#   4 = bootstrap bit-identity failed (if --selfhost given)

set -eu

ZCC="${1:-./zcc}"
SRC="${2:-tests/fractal.c}"
REF_CC="${REF_CC:-gcc}"
GOLDEN_MD5="9fe81c3d00c986b2882e8973bb3c15a2"

C="\033[36m"  # cyan
M="\033[35m"  # magenta
G="\033[32m"
R="\033[31m"
Y="\033[33m"
N="\033[0m"

hr() { printf "${C}%s${N}\n" "────────────────────────────────────────────────────────────"; }
hdr() { hr; printf "${C}🔱 %s${N}\n" "$1"; hr; }

# Locate source file whether invoked from repo root or tests/
if [ ! -f "$SRC" ]; then
    if [ -f "fractal.c" ]; then
        SRC="fractal.c"
    elif [ -f "../$SRC" ]; then
        SRC="../$SRC"
    elif [ -f "tests/fractal.c" ]; then
        SRC="tests/fractal.c"
    else
        echo -e "${R}ERROR: $SRC not found${N}" >&2
        exit 1
    fi
fi

# Locate ZCC executable
if [ ! -x "$ZCC" ]; then
    if [ -x "./zcc" ]; then
        ZCC="./zcc"
    elif [ -x "../zcc" ]; then
        ZCC="../zcc"
    else
        echo -e "${R}ERROR: ZCC binary ($ZCC) not executable${N}" >&2
        exit 1
    fi
fi

TMP_DIR="$(mktemp -d /tmp/zcc_xmm_validate_XXXXXX)"
cleanup() {
    if [ "${PRESERVE_TMP:-0}" != "1" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT

S_OUT="$TMP_DIR/fractal.s"
IR_OUT="$TMP_DIR/fractal.ir"
JSON_OUT="$TMP_DIR/zcc_ir.json"
LOG_OUT="$TMP_DIR/zcc.log"
BIN_ZCC="$TMP_DIR/fractal_zcc"
OUT_ZCC="$TMP_DIR/fractal_zcc.out"
BIN_REF="$TMP_DIR/fractal_ref"
OUT_REF="$TMP_DIR/fractal_ref.out"
DIFF_OUT="$TMP_DIR/fractal.diff"

hdr "PHASE 1: ZCC compile → fractal.s"
COMPILE_ARGS=("-S" "$SRC" "-o" "$S_OUT")
if [ "${USE_IR:-0}" = "1" ]; then
    COMPILE_ARGS=("--ir" "--telemetry" "${COMPILE_ARGS[@]}")
fi

if ! "$ZCC" "${COMPILE_ARGS[@]}" >"$LOG_OUT" 2>&1; then
    echo -e "${R}✗ ZCC compile failed. Log:${N}" >&2
    cat "$LOG_OUT" >&2
    exit 1
fi
echo -e "${G}✓ fractal.s emitted ($(wc -l < "$S_OUT") lines)${N}"
[ -f "$IR_OUT" ]   && echo -e "${G}✓ fractal.ir     ($(wc -l < "$IR_OUT") lines)${N}"
[ -f "$JSON_OUT" ]  && echo -e "${G}✓ zcc_ir.json    ($(stat -c%s "$JSON_OUT") bytes)${N}"

hdr "PHASE 2: XMM register + FP opcode scan"
declare -A fp_counts
count_op() { grep -cE "\b$1[lq]?\b" "$S_OUT" 2>/dev/null || true; }
for op in movsd addsd subsd mulsd divsd ucomisd comisd \
          cvtsi2sd cvttsd2si cvtsd2ss cvtss2sd xorpd andpd; do
    fp_counts[$op]=$(count_op "$op")
done

xmm_regs=$(grep -oE '%xmm[0-9]+' "$S_OUT" | sort -u | tr '\n' ' ')
xmm_count=$(grep -oE '%xmm[0-9]+' "$S_OUT" | sort -u | wc -l)

printf "  %-12s %s\n" "XMM regs:" "$xmm_regs"
printf "  %-12s %s (higher = more register pressure handled)\n" "distinct:" "$xmm_count"
printf "  %-12s\n" "FP opcodes:"
for op in movsd addsd subsd mulsd divsd ucomisd comisd \
          cvtsi2sd cvttsd2si xorpd; do
    c=${fp_counts[$op]}
    if [ "$c" -gt 0 ]; then
        printf "    ${G}✓${N} %-10s %6d\n" "$op" "$c"
    else
        printf "    ${Y}·${N} %-10s %6d\n" "$op" "$c"
    fi
done

missing=""
for req in movsd addsd mulsd ucomisd cvtsi2sd; do
    [ "${fp_counts[$req]}" -eq 0 ] && missing="$missing $req"
done
if [ -n "$missing" ]; then
    echo -e "${R}✗ REQUIRED FP OPS MISSING:$missing${N}" >&2
    echo -e "${R}  This means ZCC is not emitting scalar SSE for doubles.${N}" >&2
    exit 2
fi
echo -e "${G}✓ All required FP opcodes present${N}"

spill_count=$(grep -cE 'movsd.*-[0-9]+\(%rbp\)|movsd.*\(%rsp\)' "$S_OUT" 2>/dev/null || true)
printf "  %-12s %d  (stack-relative movsd)\n" "FP spills:" "$spill_count"
if [ "$spill_count" -lt 10 ]; then
    echo -e "${Y}⚠ Fewer than 10 FP spills for 10 live doubles in mandel_iter${N}"
fi

al_sets=$(grep -cE '^\s*mov[blq]?\s+\$[0-9]+,\s*%(al|eax|rax)\b' "$S_OUT" 2>/dev/null || true)
printf "  %-12s %d  (mov \$N, %%al/%%eax/%%rax before varargs)\n" "varargs al:" "$al_sets"
if [ "$al_sets" -eq 0 ]; then
    echo -e "${Y}⚠ No varargs count register set — SysV varargs rule may be violated${N}"
fi

hdr "PHASE 3: assemble + link + execute ZCC output"
if ! gcc -no-pie -o "$BIN_ZCC" "$S_OUT" 2>"$TMP_DIR/asm.log"; then
    echo -e "${R}✗ assembler/linker failed:${N}" >&2
    cat "$TMP_DIR/asm.log" >&2
    exit 2
fi
"$BIN_ZCC" > "$OUT_ZCC"
zcc_bytes=$(wc -c < "$OUT_ZCC")
zcc_md5=$(md5sum "$OUT_ZCC" | awk '{print $1}')
echo -e "${G}✓ fractal_zcc ran, $zcc_bytes bytes output (md5: $zcc_md5)${N}"

hdr "PHASE 4: reference compile with $REF_CC"
$REF_CC -O0 -no-pie -o "$BIN_REF" "$SRC"
"$BIN_REF" > "$OUT_REF"
ref_md5=$(md5sum "$OUT_REF" | awk '{print $1}')
echo -e "${G}✓ fractal_ref ran (md5: $ref_md5)${N}"

hdr "PHASE 5: output diff & golden checksum check"
if ! diff -u "$OUT_REF" "$OUT_ZCC" > "$DIFF_OUT"; then
    echo -e "${R}✗ OUTPUT DIVERGES FROM GCC REFERENCE${N}" >&2
    head -30 "$DIFF_OUT" >&2
    exit 3
fi
echo -e "${G}✓ ✓ ✓  IDENTICAL OUTPUT TO GCC REFERENCE${N}"

if [ "$zcc_md5" != "$GOLDEN_MD5" ]; then
    echo -e "${R}✗ OUTPUT MD5 ($zcc_md5) DOES NOT MATCH GOLDEN ($GOLDEN_MD5)${N}" >&2
    exit 3
fi
echo -e "${G}✓ ✓ ✓  GOLDEN MD5 VERIFIED ($GOLDEN_MD5)${N}"

hdr "SUMMARY"
printf "  ZCC binary     : ${M}%s${N}\n" "$ZCC"
printf "  .s lines       : %s\n" "$(wc -l < "$S_OUT")"
printf "  XMM regs used  : %s  (${M}%s${N})\n" "$xmm_count" "$xmm_regs"
printf "  FP spills      : %s\n" "$spill_count"
printf "  Varargs al sets: %s\n" "$al_sets"
printf "  Output match   : ${G}YES (bit-identical to GCC reference)${N}\n"
printf "  Golden MD5     : ${G}%s (MATCH)${N}\n" "$GOLDEN_MD5"
echo
echo -e "${G}🔱 ZCC XMM STRESS TEST: PASS${N}"
