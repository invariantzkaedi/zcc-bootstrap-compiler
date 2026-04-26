#!/usr/bin/env bash
set -euo pipefail

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/artifacts"

cat > "$tmp/artifacts/subject.json" <<'EOF'
{
  "kind": "fortify-layout",
  "status": "pass",
  "target": "x86_64-linux-gnu",
  "git": {
    "commit": "GOOD"
  },
  "policy": {
    "sha256": "POLICY"
  },
  "artifact_hashes": {
    "algorithm": "sha256",
    "files": []
  }
}
EOF

subject_hash="$(python3 - <<PY
import hashlib
print(hashlib.sha256(open("$tmp/artifacts/subject.json","rb").read()).hexdigest())
PY
)"

cat > "$tmp/artifacts/bundle.json" <<EOF
{
  "kind": "fortify-attestation-bundle",
  "status": "pass",
  "created_at": "1970-01-01T00:00:00Z",
  "subject": {
    "path": "$tmp/artifacts/subject.json",
    "sha256": "$subject_hash",
    "bytes": 0
  },
  "artifact_hashes": {
    "algorithm": "sha256",
    "files": []
  },
  "signing": {
    "kind": "fortify-signing-status",
    "status": "skipped",
    "signer": "",
    "signature": "",
    "certificate": "",
    "subject": "$tmp/artifacts/subject.json",
    "required": false
  },
  "signing_status_file": {
    "path": "$tmp/artifacts/signing.json",
    "sha256": "$subject_hash",
    "bytes": 0
  },
  "signature": null,
  "certificate": null,
  "host": {}
}
EOF

cat > "$tmp/policy.json" <<'EOF'
{
  "require_signature": true,
  "allowed_signers": {
    "minisign": {
      "public_key": "RWQ000000000000000000000000000000000000000000000000000000000000"
    }
  },
  "expected": {
    "git_commit": "GOOD",
    "target": "x86_64-linux-gnu",
    "policy_sha256": "POLICY"
  }
}
EOF

set +e
python3 tools/verify_attestation_bundle.py \
  --bundle "$tmp/artifacts/bundle.json" \
  --artifact-root "$tmp/artifacts" \
  --verify-policy "$tmp/policy.json" \
  --require-signature >/tmp/attestation-negative.out 2>&1
status=$?
set -e

if [ "$status" -eq 0 ]; then
  echo "expected unsigned bundle to fail when signature is required"
  cat /tmp/attestation-negative.out
  exit 1
fi

if ! grep -q "signature required" /tmp/attestation-negative.out; then
  echo "expected signature required failure"
  cat /tmp/attestation-negative.out
  exit 1
fi

echo "[SELFTEST-ATTESTATION-NEGATIVE] passed"