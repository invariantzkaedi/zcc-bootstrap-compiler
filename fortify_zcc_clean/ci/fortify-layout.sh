#!/usr/bin/env bash
# Fortify layout development-mode orchestrator.
#
# Runs the full pipeline:
#   preflight  -> no-placeholders -> policy-validate -> core-freeze
#   -> oracle   -> (optional differential) -> hash artifacts
#   -> write subject -> validate manifest -> sign (if signer set)
#   -> write attestation bundle -> validate bundle -> verify evidence
#   -> verify bundle (no policy gates in dev mode).
#
# Production mode lives in ci/fortify-layout-production.sh.
set -euo pipefail

ROOT="${ROOT:-$PWD}"
ARTIFACTS="${ARTIFACTS:-$ROOT/artifacts}"
TARGET="${TARGET:-x86_64-linux-gnu}"
SEED="${SEED:-0}"
RANDOM_COUNT="${RANDOM_COUNT:-8}"

mkdir -p "$ARTIFACTS"

echo "[FORTIFY] root=$ROOT target=$TARGET seed=$SEED artifacts=$ARTIFACTS"

# 1. preflight: required tools, canonical policy filename
ci/preflight-fortify.sh

# 2. enforce no-placeholders across the tree
ci/no-placeholders.sh

# 3. validate the fortify policy schema
ci/validate-fortify-policy.sh

# 4. layout-core-freeze: report on layout-engine surface
ci/layout-core-freeze.sh

# 5. host-compiler oracle (best-effort; only if a host compiler is present)
if command -v cc >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1 \
   || command -v clang >/dev/null 2>&1; then
    python3 tools/gen_layout_oracle.py
    python3 tools/gen_random_layout_oracle.py \
        --seed "$SEED" --count "$RANDOM_COUNT" >/dev/null 2>&1 || true
else
    echo "[FORTIFY] no host C compiler; skipping oracle generation"
fi

# 6. differential layout (only when zcc binary is on PATH)
if [ "${SKIP_DIFFERENTIAL:-0}" != "1" ] && command -v zcc >/dev/null 2>&1; then
    ci/differential-layout.sh || true
fi

# 7. hash the artifacts directory deterministically
python3 tools/hash_fortify_artifacts.py \
    --root "$ARTIFACTS" \
    --out  "$ARTIFACTS/fortify-artifact-hashes.json"

# 8. write the immutable subject manifest
python3 tools/write_fortify_manifest.py \
    --target "$TARGET" \
    --seed   "$SEED" \
    --policy "$ROOT/.zcc-fortify-policy.json" \
    --hashes "$ARTIFACTS/fortify-artifact-hashes.json" \
    --out    "$ARTIFACTS/fortify-layout.subject.json"

# 9. validate the manifest
python3 tools/validate_fortify_manifest.py \
    --manifest "$ARTIFACTS/fortify-layout.subject.json"

# 10. signing (skipped in dev unless FORTIFY_SIGNER set)
MANIFEST="$ARTIFACTS/fortify-layout.subject.json" \
    FORTIFY_SIGNING_STATUS="$ARTIFACTS/fortify-signing-status.json" \
    ci/sign-fortify-manifest.sh

# 11. write the attestation bundle
python3 tools/write_attestation_bundle.py \
    --subject        "$ARTIFACTS/fortify-layout.subject.json" \
    --hashes         "$ARTIFACTS/fortify-artifact-hashes.json" \
    --signing-status "$ARTIFACTS/fortify-signing-status.json" \
    --out            "$ARTIFACTS/fortify-attestation.bundle.json"

# 12. validate bundle structure
python3 tools/validate_attestation_bundle.py \
    --bundle "$ARTIFACTS/fortify-attestation.bundle.json"

# 13. re-hash artifacts on disk against the manifest's recorded hashes
python3 tools/verify_fortify_evidence.py \
    --manifest      "$ARTIFACTS/fortify-layout.subject.json" \
    --artifact-root "$ARTIFACTS"

# 14. end-to-end bundle verification (no policy gates in dev mode)
python3 tools/verify_attestation_bundle.py \
    --bundle        "$ARTIFACTS/fortify-attestation.bundle.json" \
    --artifact-root "$ARTIFACTS"

echo "[FORTIFY] development-mode pipeline complete"
echo "[FORTIFY] bundle: $ARTIFACTS/fortify-attestation.bundle.json"
