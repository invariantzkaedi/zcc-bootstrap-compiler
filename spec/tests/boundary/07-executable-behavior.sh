#!/bin/bash
# 07-executable-behavior.sh — ZCC-IR-024: generated code → reference execution
# Validates: "Mismatch is evidence"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Differential test: compile same program with ZCC and GCC, compare output
cat > /tmp/zcc_diff_test.c << 'EOF'
#include <stdio.h>
int fib(int n) {
  if (n <= 1) return n;
  return fib(n-1) + fib(n-2);
}
int main() {
  for (int i = 0; i < 15; i++) {
    printf("%d ", fib(i));
  }
  printf("\n");
  return 0;
}
EOF

# Compile with GCC
if ! gcc -o /tmp/zcc_diff_gcc /tmp/zcc_diff_test.c -lm 2>/dev/null; then
  emit_evidence "ZCC-IR-024" "verify" "error" "gcc_compile_failed"
  exit 1
fi
GCC_OUT=$(/tmp/zcc_diff_gcc 2>/dev/null)

# Compile with ZCC
if ! ./zcc /tmp/zcc_diff_test.c -o /tmp/zcc_diff_test.s 2>/dev/null; then
  emit_evidence "ZCC-IR-024" "verify" "fail" "zcc_compile_failed"
  exit 1
fi
if ! gcc -o /tmp/zcc_diff_zcc /tmp/zcc_diff_test.s -lm 2>/dev/null; then
  emit_evidence "ZCC-IR-024" "verify" "fail" "zcc_asm_link_failed"
  exit 1
fi
ZCC_OUT=$(/tmp/zcc_diff_zcc 2>/dev/null)

# Compare
if [ "$GCC_OUT" = "$ZCC_OUT" ]; then
  emit_evidence "ZCC-IR-024" "verify" "pass" "differential_fib_match"
else
  emit_evidence "ZCC-IR-024" "verify" "fail" "differential_fib_mismatch"
  echo "  GCC: $GCC_OUT"
  echo "  ZCC: $ZCC_OUT"
  exit 1
fi

rm -f /tmp/zcc_diff_test.c /tmp/zcc_diff_test.s /tmp/zcc_diff_gcc /tmp/zcc_diff_zcc
exit 0
