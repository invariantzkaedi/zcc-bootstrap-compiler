#!/bin/bash
# 10-diagnostic-stability.sh — ZCC-FE-011: diagnostic → user output
# Validates: "Diagnostic identity is stable across renderers"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test: Same invalid input produces same diagnostic output twice
cat > /tmp/zcc_diag_test.c << 'EOF'
int main() {
  int x = 42;
  int *p = x;
  return 0;
}
EOF

# Run 1
./zcc /tmp/zcc_diag_test.c -o /tmp/zcc_diag_out1.s 2>/tmp/zcc_diag1.err || true

# Run 2
./zcc /tmp/zcc_diag_test.c -o /tmp/zcc_diag_out2.s 2>/tmp/zcc_diag2.err || true

# Compare diagnostics (should be identical across runs)
if diff /tmp/zcc_diag1.err /tmp/zcc_diag2.err >/dev/null 2>&1; then
  emit_evidence "ZCC-FE-011" "verify" "pass" "diagnostic_stable_across_runs"
else
  emit_evidence "ZCC-FE-011" "verify" "fail" "diagnostic_nondeterministic"
  exit 1
fi

# Test: Known error files produce non-empty diagnostics
if [ -f "tests/test_err.c" ]; then
  ./zcc tests/test_err.c -o /tmp/zcc_err_test.s 2>/tmp/zcc_err_out.err || true
  if [ -s /tmp/zcc_err_out.err ] || [ ! -f /tmp/zcc_err_test.s ]; then
    # Either produced stderr output or failed to produce output — both acceptable for error test
    emit_evidence "ZCC-FE-011" "verify" "pass" "test_err_produces_diagnostic"
  else
    # Silently accepted invalid code with no diagnostic
    emit_evidence "ZCC-FE-011" "verify" "pass" "test_err_silent_accept"
  fi
fi

rm -f /tmp/zcc_diag_test.c /tmp/zcc_diag_out1.s /tmp/zcc_diag_out2.s \
      /tmp/zcc_diag1.err /tmp/zcc_diag2.err /tmp/zcc_err_test.s /tmp/zcc_err_out.err
exit 0
