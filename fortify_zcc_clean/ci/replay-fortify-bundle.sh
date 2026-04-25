#!/usr/bin/env bash
set -euo pipefail

BUNDLE="${1:-artifacts/fortify-attestation.bundle.json}"
POLICY="${2:-fortify-verify-policy.json}"
TARGET="${TARGET:-x86_64-linux-gnu}"

python3 tools/validate_attestation_bundle.py \
  --bundle "$BUNDLE"

python3 tools/verify_attestation_bundle.py \
  --bundle "$BUNDLE" \
  --artifact-root artifacts \
  --verify-policy "$POLICY" \
  --require-signature \
  --expected-target "$TARGET"

echo "[FORTIFY-REPLAY] verified $BUNDLE"