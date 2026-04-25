#!/usr/bin/env bash
# Fortify layout production-mode orchestrator.
#
# Hard gates:
#   * GITHUB_SHA must be set
#   * TARGET must be set
#   * FORTIFY_SIGNER must be set (cosign|minisign|gpg)
#   * FORTIFY_REQUIRE_SIGNATURE=1 must be set
#   * fortify-verify-policy.json (or FORTIFY_VERIFY_POLICY) must exist
#
# Pipeline:
#   verify-production-env -> selftest-attestation-negative
#   -> dev pipeline (which also signs because FORTIFY_SIGNER is set)
#   -> verify-fortify-signature
#   -> verify-attestation-bundle with --require-signature, --verify-policy,
#      --expected-git-commit, --expected-target
set -euo pipefail

ROOT="${ROOT:-$PWD}"
ARTIFACTS="${ARTIFACTS:-$ROOT/artifacts}"
VERIFY_POLICY="${FORTIFY_VERIFY_POLICY:-fortify-verify-policy.json}"

# 1. verify production environment (GITHUB_SHA, TARGET, FORTIFY_SIGNER, ...)
ci/verify-production-env.sh

# 2. validate the verify-policy schema before depending on it
python3 tools/validate_verify_policy.py --policy "$VERIFY_POLICY"

# 3. negative-attestation selftest must pass first: the verifier must
#    reject a tampered bundle BEFORE we trust it on a real one.
ci/selftest-attestation-negative.sh

# 4. run the dev pipeline (which signs because FORTIFY_SIGNER is set)
TARGET="$TARGET" SEED="${SEED:-0}" ci/fortify-layout.sh

# 5. explicit signature verification using the configured signer
MANIFEST="$ARTIFACTS/fortify-layout.subject.json" \
    FORTIFY_SIGNER="$FORTIFY_SIGNER" \
    FORTIFY_VERIFY_KEY="${FORTIFY_VERIFY_KEY:-}" \
    FORTIFY_REQUIRE_SIGNATURE=1 \
    ci/verify-fortify-signature.sh

# 6. full bundle verification with policy gates
EXTRA=()
if [ -n "${FORTIFY_EXPECTED_POLICY_SHA:-}" ]; then
    EXTRA+=( --expected-policy-sha "$FORTIFY_EXPECTED_POLICY_SHA" )
fi

python3 tools/verify_attestation_bundle.py \
    --bundle               "$ARTIFACTS/fortify-attestation.bundle.json" \
    --artifact-root        "$ARTIFACTS" \
    --verify-policy        "$VERIFY_POLICY" \
    --require-signature \
    --expected-git-commit  "$GITHUB_SHA" \
    --expected-target      "$TARGET" \
    "${EXTRA[@]}"

echo "[FORTIFY-PROD] production pipeline complete: signed and verified"
echo "[FORTIFY-PROD] bundle: $ARTIFACTS/fortify-attestation.bundle.json"
