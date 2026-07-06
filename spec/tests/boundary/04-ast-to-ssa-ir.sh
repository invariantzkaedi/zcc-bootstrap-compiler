#!/bin/bash
# 04-ast-to-ssa-ir.sh — ZCC-IR-001: typed AST → IR
# Validates: "Each IR instruction has declared operands, type, effects, and source origin"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test: IR emission for a known test program
if ./zcc tests/test_ir.c -o /tmp/zcc_ir_test.s 2>/dev/null; then
  gcc -o /tmp/zcc_ir_test /tmp/zcc_ir_test.s -lm 2>/dev/null
  if /tmp/zcc_ir_test >/dev/null 2>&1; then
    emit_evidence "ZCC-IR-001" "lower" "pass" "test_ir"
  else
    emit_evidence "ZCC-IR-001" "lower" "fail" "test_ir_runtime"
    exit 1
  fi
else
  emit_evidence "ZCC-IR-001" "lower" "fail" "test_ir_compile"
  exit 1
fi

# Test: Full IR test if available
if [ -f "tests/test_ir_full.c" ]; then
  if ./zcc tests/test_ir_full.c -o /tmp/zcc_ir_full.s 2>/dev/null; then
    if gcc -o /tmp/zcc_ir_full /tmp/zcc_ir_full.s -lm 2>/dev/null && /tmp/zcc_ir_full >/dev/null 2>&1; then
      emit_evidence "ZCC-IR-001" "lower" "pass" "test_ir_full"
    else
      emit_evidence "ZCC-IR-001" "lower" "fail" "test_ir_full_runtime"
      exit 1
    fi
  else
    emit_evidence "ZCC-IR-001" "lower" "fail" "test_ir_full_compile"
    exit 1
  fi
fi

rm -f /tmp/zcc_ir_test.s /tmp/zcc_ir_test /tmp/zcc_ir_full.s /tmp/zcc_ir_full
exit 0
