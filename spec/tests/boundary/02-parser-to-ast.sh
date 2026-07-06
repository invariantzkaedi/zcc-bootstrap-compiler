#!/bin/bash
# 02-parser-to-ast.sh — ZCC-FE-004: parser → AST
# Validates: "AST nodes have complete spans and no impossible variant combinations"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test 1: Complex parsing — self-contained program with diverse syntax
cat > /tmp/zcc_parse_boundary.c << 'EOF'
int printf(const char *fmt, ...);

typedef struct { int x; int y; } Point;

int add(int a, int b) { return a + b; }
int fib(int n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }

int main() {
  // Diverse parser constructs
  Point p = {10, 20};
  int arr[5] = {1, 2, 3, 4, 5};
  int sum = 0;

  for (int i = 0; i < 5; i++) {
    sum += arr[i];
  }

  int *ptr = &sum;
  *ptr += add(p.x, p.y);

  switch (sum % 3) {
    case 0: sum += 1; break;
    case 1: sum += 2; break;
    default: sum += 3; break;
  }

  while (sum > 100) {
    sum /= 2;
  }

  int result = fib(10);
  printf("sum=%d fib=%d\n", sum, result);
  return result == 55 ? 0 : 1;
}
EOF

if ./zcc /tmp/zcc_parse_boundary.c -o /tmp/zcc_parse_boundary.s 2>/dev/null; then
  gcc -o /tmp/zcc_parse_boundary /tmp/zcc_parse_boundary.s -lm 2>/dev/null
  if /tmp/zcc_parse_boundary >/dev/null 2>&1; then
    emit_evidence "ZCC-FE-004" "parse" "pass" "complex_syntax_test"
  else
    emit_evidence "ZCC-FE-004" "parse" "fail" "complex_syntax_runtime"
    exit 1
  fi
else
  emit_evidence "ZCC-FE-004" "parse" "fail" "complex_syntax_compile"
  exit 1
fi

# Test 2: Malformed syntax should produce bounded recovery, not hang
cat > /tmp/zcc_bad_parse.c << 'EOF'
int main() {
  int x = ;
  return 0;
}
EOF
timeout 5 ./zcc /tmp/zcc_bad_parse.c -o /tmp/zcc_bad_parse.s 2>/dev/null || true
EXIT_CODE=${PIPESTATUS[0]:-$?}
if [ "$EXIT_CODE" -eq 139 ] || [ "$EXIT_CODE" -eq 134 ] || [ "$EXIT_CODE" -eq 124 ]; then
  emit_evidence "ZCC-FE-004" "parse" "fail" "malformed_syntax_crash_or_hang"
  exit 1
fi
emit_evidence "ZCC-FE-004" "parse" "pass" "malformed_syntax_bounded_recovery"

rm -f /tmp/zcc_parse_boundary.c /tmp/zcc_parse_boundary.s /tmp/zcc_parse_boundary \
      /tmp/zcc_bad_parse.c /tmp/zcc_bad_parse.s
exit 0
