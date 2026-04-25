# Fortify Quickstart

## Development mode

```bash
chmod +x ci/*.sh
ci/fortify-layout.sh
```

Development mode may skip signing.

## Production mode

Production mode requires:

- `GITHUB_SHA`
- `FORTIFY_SIGNER`
- signer credentials
- `fortify-verify-policy.json`
- valid trusted signer identity

Example:

```bash
export GITHUB_SHA="$(git rev-parse HEAD)"
export TARGET="x86_64-linux-gnu"
export FORTIFY_SIGNER="cosign"
export FORTIFY_REQUIRE_SIGNATURE=1

ci/fortify-layout-production.sh
```

## Replay an attestation bundle

```bash
python3 tools/verify_attestation_bundle.py \
  --bundle artifacts/fortify-attestation.bundle.json \
  --artifact-root artifacts \
  --verify-policy fortify-verify-policy.json \
  --require-signature \
  --expected-git-commit "$GITHUB_SHA" \
  --expected-target "$TARGET"
```

## Main outputs

```text
artifacts/fortify-layout.subject.json
artifacts/fortify-artifact-hashes.json
artifacts/fortify-signing-status.json
artifacts/fortify-attestation.bundle.json
```