# Fortify Production Runbook

## Before enabling production gate

- [ ] Development fortify passes.
- [ ] Production signing mechanism chosen.
- [ ] `fortify-verify-policy.json` created.
- [ ] Trusted signer identity reviewed.
- [ ] `GITHUB_SHA` pin available in CI.
- [ ] Target matrix agreed.
- [ ] Artifact retention configured.

## Production environment variables

```text
TARGET
GITHUB_SHA
FORTIFY_SIGNER
FORTIFY_REQUIRE_SIGNATURE
FORTIFY_VERIFY_KEY
MINISIGN_KEY
```

Depending on signer:

### minisign

```text
FORTIFY_SIGNER=minisign
MINISIGN_KEY=/path/to/minisign.key
FORTIFY_VERIFY_KEY=<public key string or public key file depending on policy>
```

### gpg

```text
FORTIFY_SIGNER=gpg
```

Requires configured private key for signing and trusted public keyring for verification.

### cosign

```text
FORTIFY_SIGNER=cosign
```

Requires GitHub OIDC or configured cosign signing environment.

## Release verification

After CI:

```bash
ci/replay-fortify-bundle.sh \
  artifacts/fortify-attestation.bundle.json \
  fortify-verify-policy.json
```

## Incident response

If production verification fails:

1. Do not release.
2. Preserve `artifacts/`.
3. Preserve CI logs.
4. Inspect bundle subject hash.
5. Verify expected git commit.
6. Verify signer identity.
7. Promote any reduced fuzz case to regression.