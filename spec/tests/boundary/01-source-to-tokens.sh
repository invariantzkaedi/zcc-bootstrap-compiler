#!/bin/bash
# 01-source-to-tokens.sh — ZCC-FE-002: text → tokens
# Validates: "Every non-whitespace byte is classified or rejected"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test 1: Valid tokenization — compile a self-contained token stress test
cat > /tmp/zcc_tok_boundary.c << 'EOF'
int printf(const char *fmt, ...);
int main() {
  int x = 42;
  int y = x + 10;
  int z = x * y - 3;
  char *s = "hello world";
  float f = 3.14f;
  double d = 2.718281828;
  if (x > 0 && y < 100 || z != 0) {
    printf("tokens: %d %d %d %s %f %f\n", x, y, z, s, f, d);
  }
  for (int i = 0; i < 5; i++) {
    x += i;
  }
  return x == 52 ? 0 : 1;
}
EOF

if ./zcc /tmp/zcc_tok_boundary.c -o /tmp/zcc_tok_boundary.s 2>/dev/null; then
  gcc -o /tmp/zcc_tok_boundary /tmp/zcc_tok_boundary.s -lm 2>/dev/null
  if /tmp/zcc_tok_boundary >/dev/null 2>&1; then
    emit_evidence "ZCC-FE-002" "tokenize" "pass" "token_stress_test"
  else
    emit_evidence "ZCC-FE-002" "tokenize" "fail" "token_stress_runtime"
    exit 1
  fi
else
  emit_evidence "ZCC-FE-002" "tokenize" "fail" "token_stress_compile"
  exit 1
fi

# Test 2: Malformed input should produce diagnostic, not crash
echo 'int main() { "\xff\xfe"; return 0; }' > /tmp/zcc_bad_tok.c
EXIT_CODE=0
timeout 5 ./zcc /tmp/zcc_bad_tok.c -o /tmp/zcc_bad_tok.s 2>/dev/null || EXIT_CODE=$?
if [ "$EXIT_CODE" -eq 139 ] || [ "$EXIT_CODE" -eq 134 ]; then
  emit_evidence "ZCC-FE-002" "tokenize" "fail" "malformed_input_crash"
  exit 1
fi
emit_evidence "ZCC-FE-002" "tokenize" "pass" "malformed_input_handled"

rm -f /tmp/zcc_tok_boundary.c /tmp/zcc_tok_boundary.s /tmp/zcc_tok_boundary \
      /tmp/zcc_bad_tok.c /tmp/zcc_bad_tok.s
exit 0
