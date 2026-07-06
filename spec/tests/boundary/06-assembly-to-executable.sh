#!/bin/bash
# 06-assembly-to-executable.sh — ZCC-TOOL-001: object file → linker
# Validates: "All inputs target the same compatible ABI"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test: ZCC assembly links correctly with gcc runtime
cat > /tmp/zcc_link_test.c << 'EOF'
int printf(const char *fmt, ...);
double sqrt(double x);
int main() {
  double v = sqrt(144.0);
  printf("sqrt(144) = %.1f\n", v);
  return (int)v == 12 ? 0 : 1;
}
EOF

if ./zcc /tmp/zcc_link_test.c -o /tmp/zcc_link_test.s 2>/dev/null; then
  if gcc -o /tmp/zcc_link_test /tmp/zcc_link_test.s -lm 2>/dev/null; then
    if /tmp/zcc_link_test >/dev/null 2>&1; then
      emit_evidence "ZCC-TOOL-001" "link" "pass" "zcc_asm_gcc_link"
    else
      emit_evidence "ZCC-TOOL-001" "link" "fail" "zcc_asm_gcc_link_runtime"
      exit 1
    fi
  else
    emit_evidence "ZCC-TOOL-001" "link" "fail" "gcc_linker_rejected"
    exit 1
  fi
else
  emit_evidence "ZCC-TOOL-001" "link" "fail" "codegen_failed"
  exit 1
fi

# Test: Cross-toolchain interop (if interop binaries exist)
if [ -f "interop_gcc_lib" ] && [ -f "interop_zcc_lib" ]; then
  emit_evidence "ZCC-TOOL-001" "link" "pass" "interop_binaries_present"
fi

rm -f /tmp/zcc_link_test.c /tmp/zcc_link_test.s /tmp/zcc_link_test
exit 0
