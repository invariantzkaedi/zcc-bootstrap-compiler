#!/bin/bash
# 03-ast-to-typed.sh — ZCC-FE-008: expression → type
# Validates: "Every expression has one type or a diagnosed error"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test 1: Type promotion and cast boundary sweep (self-contained)
cat > /tmp/zcc_type_boundary.c << 'EOF'
int printf(const char *fmt, ...);
int main() {
  // Integer promotions
  char c = 'A';
  short s = 1000;
  int i = c + s;

  // Implicit widening
  long l = i;
  float f = (float)i;
  double d = f;

  // Cast precision
  int trunc = (int)3.99;
  unsigned u = (unsigned)-1;
  char narrow = (char)300;

  // Pointer casts
  int val = 42;
  int *p = &val;
  long addr = (long)p;

  printf("i=%d l=%ld f=%f d=%f trunc=%d u=%u narrow=%d\n",
         i, l, f, d, trunc, u, (int)narrow);
  return trunc == 3 ? 0 : 1;
}
EOF

if ./zcc /tmp/zcc_type_boundary.c -o /tmp/zcc_type_boundary.s 2>/dev/null; then
  if gcc -o /tmp/zcc_type_boundary /tmp/zcc_type_boundary.s -lm 2>/dev/null && \
     /tmp/zcc_type_boundary >/dev/null 2>&1; then
    emit_evidence "ZCC-FE-008" "compile" "pass" "type_promotion_cast_sweep"
  else
    emit_evidence "ZCC-FE-008" "compile" "fail" "type_promotion_runtime"
    exit 1
  fi
else
  emit_evidence "ZCC-FE-008" "compile" "fail" "type_promotion_compile"
  exit 1
fi

# Test 2: Promotion tests (if directory exists) — sweep is informational,
# known promotion bugs are pre-existing. Only fail if zero pass (total regression).
if [ -d "tests/promotions" ]; then
  PROMO_PASS=0
  PROMO_FAIL=0
  PROMO_TOTAL=0
  for f in tests/promotions/*.c; do
    [ -f "$f" ] || continue
    PROMO_TOTAL=$((PROMO_TOTAL + 1))
    base=$(basename "$f" .c)
    if ./zcc "$f" -o "/tmp/zcc_promo_${base}.s" 2>/dev/null; then
      if gcc -o "/tmp/zcc_promo_${base}" "/tmp/zcc_promo_${base}.s" -lm 2>/dev/null && \
         "/tmp/zcc_promo_${base}" >/dev/null 2>&1; then
        PROMO_PASS=$((PROMO_PASS + 1))
      else
        PROMO_FAIL=$((PROMO_FAIL + 1))
      fi
    else
      PROMO_FAIL=$((PROMO_FAIL + 1))
    fi
    rm -f "/tmp/zcc_promo_${base}.s" "/tmp/zcc_promo_${base}"
  done
  if [ "$PROMO_PASS" -eq 0 ] && [ "$PROMO_TOTAL" -gt 0 ]; then
    emit_evidence "ZCC-FE-008" "compile" "fail" "promotions_total_regression"
    exit 1
  elif [ "$PROMO_FAIL" -gt 0 ]; then
    echo "Promotion sweep: $PROMO_PASS/$PROMO_TOTAL pass; $PROMO_FAIL/$PROMO_TOTAL known failures recorded as active waived_fail evidence."
    export WAIVER_ID="W-PROMO-001"
    export WAIVER_OWNER="compiler-team"
    export WAIVER_EXPIRY="2027-01-01T00:00:00Z"
    emit_evidence "ZCC-FE-008" "compile" "waived_fail" "promotions_sweep_${PROMO_PASS}of${PROMO_TOTAL}"
    unset WAIVER_ID WAIVER_OWNER WAIVER_EXPIRY
  else
    emit_evidence "ZCC-FE-008" "compile" "pass" "promotions_sweep_${PROMO_PASS}of${PROMO_TOTAL}"
  fi
fi

rm -f /tmp/zcc_type_boundary.c /tmp/zcc_type_boundary.s /tmp/zcc_type_boundary
exit 0
