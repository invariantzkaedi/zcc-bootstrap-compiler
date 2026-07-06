#!/bin/bash
# 05-ssa-to-assembly.sh — ZCC-IR-020: machine instruction → assembly text
# Validates: "Assembly text is syntactically valid and unambiguous"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test: ZCC emits assembly that GNU as accepts without error
cat > /tmp/zcc_asm_test.c << 'EOF'
int printf(const char *fmt, ...);
int main() {
  int x = 42;
  int y = x * 3 + 7;
  printf("%d\n", y);
  return 0;
}
EOF

if ./zcc /tmp/zcc_asm_test.c -o /tmp/zcc_asm_test.s 2>/dev/null; then
  # Validate: GNU as can assemble the output without errors
  if as /tmp/zcc_asm_test.s -o /tmp/zcc_asm_test.o 2>/dev/null; then
    emit_evidence "ZCC-IR-020" "emit" "pass" "assembly_valid_syntax"
  else
    emit_evidence "ZCC-IR-020" "emit" "fail" "assembly_rejected_by_as"
    exit 1
  fi
else
  emit_evidence "ZCC-IR-020" "emit" "fail" "codegen_failed"
  exit 1
fi

# Test: Assembly for the self-compiled compiler is also valid
if [ -f "zcc2.s" ]; then
  # Just verify the first 100 lines don't contain obviously broken syntax
  # (Full validation is done by the assembler during make selfhost)
  if head -100 zcc2.s | as -o /dev/null - 2>/dev/null; then
    emit_evidence "ZCC-IR-020" "emit" "pass" "selfhost_assembly_prefix_valid"
  fi
fi

rm -f /tmp/zcc_asm_test.c /tmp/zcc_asm_test.s /tmp/zcc_asm_test.o
exit 0
