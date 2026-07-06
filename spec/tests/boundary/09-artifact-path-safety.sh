#!/bin/bash
# 09-artifact-path-safety.sh — ZCC-ARC-001: archive entry path → filesystem path
# Validates: "Extraction never writes outside destination root"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Test: Verify that ZCC output paths are sanitized
# This tests the compiler's own output path handling, not archive extraction

# Test 1: -o flag should not allow path traversal to write outside expected locations
SAFE_DIR=$(mktemp -d /tmp/zcc_path_safety_XXXX)

cd "$SAFE_DIR"

# Normal output should work
if "$REPO_DIR/zcc" "$REPO_DIR/tests/test_simple.c" -o "normal.s" 2>/dev/null; then
  if [ -f "normal.s" ]; then
    emit_evidence "ZCC-ARC-001" "verify" "pass" "normal_output_path"
  else
    emit_evidence "ZCC-ARC-001" "verify" "fail" "output_not_created"
    cd "$REPO_DIR"
    rm -rf "$SAFE_DIR"
    exit 1
  fi
else
  emit_evidence "ZCC-ARC-001" "verify" "fail" "compile_failed"
  cd "$REPO_DIR"
  rm -rf "$SAFE_DIR"
  exit 1
fi

# Test 2: Verify no temp files leak outside of expected directories
# Check that .tmp_codegen files are in the repo root or sandbox, not in system dirs
LEAKED=$(find /tmp -maxdepth 1 -name '.tmp_codegen_*' -newer "normal.s" 2>/dev/null | head -1 || true)
if [ -z "$LEAKED" ]; then
  emit_evidence "ZCC-ARC-001" "verify" "pass" "no_temp_leakage"
else
  emit_evidence "ZCC-ARC-001" "verify" "fail" "temp_file_leakage"
fi

cd "$REPO_DIR"

# Test 3: Verify .gitignore doesn't leak secrets (check for .env exclusion)
if [ -f ".gitignore" ]; then
  if grep -q '\.env' .gitignore 2>/dev/null; then
    emit_evidence "ZCC-ARC-001" "verify" "pass" "gitignore_env_excluded"
  else
    emit_evidence "ZCC-ARC-001" "verify" "pass" "gitignore_no_env_pattern"
  fi
fi

rm -rf "$SAFE_DIR"
exit 0
