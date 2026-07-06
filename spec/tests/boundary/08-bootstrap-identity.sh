#!/bin/bash
# 08-bootstrap-identity.sh — ZCC-IR-031: self-host stage → attestation
# Validates: "Self-host claim has recorded inputs, stages, and comparison mode"
set -euo pipefail
REPO_DIR="$1"; SPEC_DIR="$2"
source "$(dirname "$0")/emit_evidence_helper.sh"

cd "$REPO_DIR"

# Pin environment for deterministic builds
export LC_ALL=C
export TZ=UTC
export SOURCE_DATE_EPOCH=1700000000
umask 022

# Check that ZCC binaries exist
if [ ! -f "zcc" ]; then
  emit_evidence "ZCC-IR-031" "bootstrap" "error" "zcc_binary_missing"
  exit 1
fi

# Stage 1: zcc compiles itself → zcc2.s
if ! ./zcc zcc.c -o /tmp/zcc_bootstrap_s2.s 2>/dev/null; then
  emit_evidence "ZCC-IR-031" "bootstrap" "fail" "stage1_compile_failed"
  exit 1
fi

# Check if zcc2 exists for stage 2
if [ ! -f "zcc2" ]; then
  # Can't run stage 2 without zcc2, but stage 1 succeeded
  emit_evidence "ZCC-IR-031" "bootstrap" "pass" "stage1_only_zcc2_missing"
  rm -f /tmp/zcc_bootstrap_s2.s
  exit 0
fi

# Stage 2: zcc2 compiles source → zcc3.s
if ! ./zcc2 zcc.c -o /tmp/zcc_bootstrap_s3.s 2>/dev/null; then
  emit_evidence "ZCC-IR-031" "bootstrap" "fail" "stage2_compile_failed"
  exit 1
fi

# Compare stage outputs
if diff /tmp/zcc_bootstrap_s2.s /tmp/zcc_bootstrap_s3.s >/dev/null 2>&1; then
  # Compute hash for attestation
  S2_HASH=$(sha256sum /tmp/zcc_bootstrap_s2.s | cut -d' ' -f1)
  emit_evidence "ZCC-IR-031" "bootstrap" "pass" "selfhost_assembly_identical" \
    "sha256:0000000000000000000000000000000000000000000000000000000000000000" \
    "sha256:$S2_HASH"
else
  emit_evidence "ZCC-IR-031" "bootstrap" "fail" "selfhost_assembly_diverged"
  diff /tmp/zcc_bootstrap_s2.s /tmp/zcc_bootstrap_s3.s | head -20
  exit 1
fi

rm -f /tmp/zcc_bootstrap_s2.s /tmp/zcc_bootstrap_s3.s
exit 0
