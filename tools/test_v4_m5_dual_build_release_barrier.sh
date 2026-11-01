#!/usr/bin/env bash
# ==============================================================================
# 🛡️  ZCC OMEGA V4-M5 — DUAL-BUILD ASYMMETRIC RELEASE BARRIER ADJUDICATION
# ==============================================================================
# Validates Rule ZCC-EF-0 and RFC 8032 Asymmetric Non-Repudiation:
# 1. Genuine producer build certified by external release authority (invariantzkaedi/
#    zcc-release-authority) passes release barrier.
# 2. Corrupted producer build CANNOT be released, even if the producer workflow
#    deliberately reports green (PASS).
# 3. Cryptographically bound asymmetric lineage prevents artifact substitution,
#    stale attestation reuse, recomputed-digest forgery, and authority rejections.
# 4. Enforces V4-M5-20: Producer environment possesses ZERO authority private keys.
#    All test fixtures utilize disposable ephemeral keys or strict fail-closed paths.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

WORK_DIR="$(mktemp -d /tmp/zcc_v4_m5_barrier_XXXXXX)"
cleanup() {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

echo "======================================================================"
echo "🛡️  ZCC OMEGA V4-M5 — DUAL-BUILD RELEASE BARRIER ADJUDICATION"
echo "======================================================================"

# ------------------------------------------------------------------------------
# Ephemeral Test Authority Key Fixture (V4-M5-20 Remediation)
# ------------------------------------------------------------------------------
# The producer test harness MUST NEVER possess or access the production private key.
# We generate a disposable Ed25519 keypair exclusively for test harness execution.
# ------------------------------------------------------------------------------
TEST_AUTH_PRIV="$WORK_DIR/test_authority_priv.pem"
TEST_AUTH_PUB="$WORK_DIR/test_authority_pub.pem"
TEST_AUTH_FP_FILE="$WORK_DIR/test_authority_fp.txt"

python3 -c "
import hashlib
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization

priv = ed25519.Ed25519PrivateKey.generate()
pub = priv.public_key()

priv_pem = priv.private_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PrivateFormat.PKCS8,
    encryption_algorithm=serialization.NoEncryption()
)
pub_pem = pub.public_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PublicFormat.SubjectPublicKeyInfo
)
raw_pub = pub.public_bytes(
    encoding=serialization.Encoding.Raw,
    format=serialization.PublicFormat.Raw
)
fp = hashlib.sha256(raw_pub).hexdigest()

with open('$TEST_AUTH_PRIV', 'wb') as f:
    f.write(priv_pem)
with open('$TEST_AUTH_PUB', 'wb') as f:
    f.write(pub_pem)
with open('$TEST_AUTH_FP_FILE', 'w') as f:
    f.write(fp)
"
TEST_AUTH_FP=$(cat "$TEST_AUTH_FP_FILE")
echo "Generated disposable test authority fixture: FP=$TEST_AUTH_FP"

GENUINE_VERDICT="$WORK_DIR/genuine_verdict.json"
GENUINE_BUNDLE="$WORK_DIR/genuine_bundle.zip"

echo "COMPILER_RELEASE_BINARY_GENUINE_V4_M5_2026" > "$GENUINE_BUNDLE"
PROD_COMMIT="42d5b595cdeccb3107f48f13e5ead853f946be77"
PROD_RUN_ID="40172220199"
ART_ID="380998822"
AUTH_COMMIT="400e809000000000000000000000000000000000"

# Synthesize candidate verdict and sign using disposable test authority fixture
python3 -c "
import json, hashlib
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization

with open('$TEST_AUTH_PRIV', 'rb') as f:
    priv = serialization.load_pem_private_key(f.read(), password=None)

with open('$GENUINE_BUNDLE', 'rb') as f:
    art_sha = hashlib.sha256(f.read()).hexdigest()

verdict_data = {
    'authority': 'invariantzkaedi/zcc-release-authority',
    'authority_workflow': 'adjudicate-release.yml',
    'authority_commit': '$AUTH_COMMIT',
    'authority_verifier_sha256': '3f748bf392493d9d68981078829d3826656103e5cb85047c22ba2780ac95c5e9',
    'authority_policy_sha256': '969b462f1e3fd72a06424edc82c60ead499778c68d29b717cdda5f5024a482e7',
    'authority_pubkey_fingerprint': '$TEST_AUTH_FP',
    'evaluated_producer_repo': 'invariantzkaedi/zcc-bootstrap-compiler',
    'evaluated_producer_commit': '$PROD_COMMIT',
    'evaluated_producer_run_id': '$PROD_RUN_ID',
    'evaluated_artifact_id': '$ART_ID',
    'evaluated_artifact_sha256': art_sha,
    'verdict': 'SOVEREIGN_RELEASE_CERTIFIED',
    'status': 'success',
    'adjudication_timestamp': '2026-10-25T02:00:00Z',
}

canon_bytes = json.dumps(verdict_data, sort_keys=True, indent=2).encode('utf-8')
canon_sha = hashlib.sha256(canon_bytes).hexdigest()
sig = priv.sign(canon_bytes).hex()

verdict_data['canonical_payload_sha256'] = canon_sha
verdict_data['ed25519_signature'] = {
    'algorithm': 'Ed25519 (RFC 8032)',
    'public_key_fingerprint': '$TEST_AUTH_FP',
    'signed_canonical_payload_sha256': canon_sha,
    'signature_hex': sig
}

with open('$GENUINE_VERDICT', 'w') as f:
    json.dump(verdict_data, f, indent=2)
"

cat "$GENUINE_VERDICT"
echo "Candidate artifact bundle: $(sha256sum "$GENUINE_BUNDLE")"

# ==============================================================================
# TEST 1: BUILD A (GENUINE CANDIDATE ACCESS GRANTED)
# ==============================================================================
echo -e "\n--- TEST 1: BUILD A (GENUINE CANDIDATE ACCESS GRANTED) ---"
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP"
echo "✓ TEST 1 PASS: Valid build approved by sovereign release barrier (Exit 0)"

# ==============================================================================
# TEST 2: BUILD B — ATTACK VECTOR 1 (CORRUPTED ARTIFACT SUBSTITUTION)
# ==============================================================================
echo -e "\n--- TEST 2: BUILD B (CORRUPTED ARTIFACT SUBSTITUTION) ---"
CORRUPTED_BUNDLE="$WORK_DIR/corrupted_bundle.zip"
cp "$GENUINE_BUNDLE" "$CORRUPTED_BUNDLE"
python3 -c "
with open('$CORRUPTED_BUNDLE', 'r+b') as f:
    f.seek(0)
    b = f.read(1)
    f.seek(0)
    f.write(bytes([b[0] ^ 1]))
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$CORRUPTED_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t2_out.txt" 2>&1
T2_RC=$?
set -e

if [[ $T2_RC -ne 1 ]]; then
    echo "FATAL: Corrupted artifact substitution was not blocked! Exit $T2_RC" >&2
    cat "$WORK_DIR/t2_out.txt" >&2
    exit 1
fi
if ! grep -q "Artifact digest mismatch" "$WORK_DIR/t2_out.txt"; then
    echo "FATAL: Expected artifact digest mismatch diagnostic missing!" >&2
    cat "$WORK_DIR/t2_out.txt" >&2
    exit 1
fi
echo "✓ TEST 2 PASS: Corrupted artifact substitution strictly blocked (Exit 1)"

# ==============================================================================
# TEST 3: BUILD B — ATTACK VECTOR 2 (STALE/WRONG PRODUCER COMMIT LINEAGE)
# ==============================================================================
echo -e "\n--- TEST 3: BUILD B (STALE / FORGED PRODUCER COMMIT LINEAGE) ---"
set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "badc0ffee0000000000000000000000000000000" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t3_out.txt" 2>&1
T3_RC=$?
set -e

if [[ $T3_RC -ne 1 ]]; then
    echo "FATAL: Stale commit lineage was not blocked! Exit $T3_RC" >&2
    cat "$WORK_DIR/t3_out.txt" >&2
    exit 1
fi
if ! grep -q "Lineage commit mismatch" "$WORK_DIR/t3_out.txt"; then
    echo "FATAL: Expected lineage commit mismatch diagnostic missing!" >&2
    cat "$WORK_DIR/t3_out.txt" >&2
    exit 1
fi
echo "✓ TEST 3 PASS: Stale commit lineage strictly blocked (Exit 1)"

# ==============================================================================
# TEST 4: BUILD B — ATTACK VECTOR 3 (DECISIVE RECOMPUTED-DIGEST FORGERY ATTACK)
# ==============================================================================
echo -e "\n--- TEST 4: BUILD B (DECISIVE RECOMPUTED-DIGEST FORGERY ATTACK) ---"
FORGED_VERDICT="$WORK_DIR/forged_verdict.json"
# Attacker alters verdict payload to match their malicious artifact digest
# and recomputes all unkeyed SHA-256 digests
python3 -c "
import json, hashlib
with open('$GENUINE_VERDICT') as f:
    d = json.load(f)
with open('$CORRUPTED_BUNDLE', 'rb') as f:
    bad_sha = hashlib.sha256(f.read()).hexdigest()

d['evaluated_artifact_sha256'] = bad_sha
d['verdict'] = 'SOVEREIGN_RELEASE_CERTIFIED'
d['status'] = 'success'

signable = {
    'authority': d['authority'],
    'authority_workflow': d['authority_workflow'],
    'authority_commit': d['authority_commit'],
    'authority_verifier_sha256': d['authority_verifier_sha256'],
    'authority_policy_sha256': d['authority_policy_sha256'],
    'authority_pubkey_fingerprint': d.get('authority_pubkey_fingerprint', ''),
    'evaluated_producer_repo': d['evaluated_producer_repo'],
    'evaluated_producer_commit': d['evaluated_producer_commit'],
    'evaluated_producer_run_id': str(d['evaluated_producer_run_id']),
    'evaluated_artifact_id': str(d['evaluated_artifact_id']),
    'evaluated_artifact_sha256': bad_sha,
    'verdict': d['verdict'],
    'status': d['status'],
    'adjudication_timestamp': d['adjudication_timestamp'],
}
recomputed_sha = hashlib.sha256(json.dumps(signable, sort_keys=True, indent=2).encode('utf-8')).hexdigest()
d['canonical_payload_sha256'] = recomputed_sha
d['ed25519_signature']['signed_canonical_payload_sha256'] = recomputed_sha

with open('$FORGED_VERDICT', 'w') as f:
    json.dump(d, f, indent=2)
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$FORGED_VERDICT" \
    --artifact "$CORRUPTED_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t4_out.txt" 2>&1
T4_RC=$?
set -e

if [[ $T4_RC -ne 1 ]]; then
    echo "FATAL: Recomputed-digest forgery attack was not blocked! Exit $T4_RC" >&2
    cat "$WORK_DIR/t4_out.txt" >&2
    exit 1
fi
if ! grep -q "Cryptographic Ed25519 signature verification failed" "$WORK_DIR/t4_out.txt"; then
    echo "FATAL: Expected cryptographic signature forgery diagnostic missing!" >&2
    cat "$WORK_DIR/t4_out.txt" >&2
    exit 1
fi
echo "✓ TEST 4 PASS: Recomputed-digest forgery strictly blocked by Ed25519 signature (Exit 1)"

# ==============================================================================
# TEST 5: BUILD B — ATTACK VECTOR 4 (AUTHORITY-REJECTED BUILD POLICY ENFORCEMENT)
# ==============================================================================
echo -e "\n--- TEST 5: BUILD B (AUTHENTIC SIGNED REJECTION STRICTLY BLOCKED BY POLICY) ---"
REJECTED_VERDICT="$WORK_DIR/rejected_verdict.json"
# Sign a rejection using the authorized test key fixture to verify that rejection
# is blocked by release policy and NOT because the signature is invalid
python3 -c "
import json, hashlib
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization

with open('$TEST_AUTH_PRIV', 'rb') as f:
    priv = serialization.load_pem_private_key(f.read(), password=None)

with open('$GENUINE_VERDICT') as f:
    d = json.load(f)

d['verdict'] = 'SOVEREIGN_RELEASE_REJECTED'
d['status'] = 'failure'

signable = {
    'authority': d['authority'],
    'authority_workflow': d['authority_workflow'],
    'authority_commit': d['authority_commit'],
    'authority_verifier_sha256': d['authority_verifier_sha256'],
    'authority_policy_sha256': d['authority_policy_sha256'],
    'authority_pubkey_fingerprint': '$TEST_AUTH_FP',
    'evaluated_producer_repo': d['evaluated_producer_repo'],
    'evaluated_producer_commit': d['evaluated_producer_commit'],
    'evaluated_producer_run_id': str(d['evaluated_producer_run_id']),
    'evaluated_artifact_id': str(d['evaluated_artifact_id']),
    'evaluated_artifact_sha256': d['evaluated_artifact_sha256'],
    'verdict': 'SOVEREIGN_RELEASE_REJECTED',
    'status': 'failure',
    'adjudication_timestamp': d['adjudication_timestamp'],
}
canon_bytes = json.dumps(signable, sort_keys=True, indent=2).encode('utf-8')
canon_sha = hashlib.sha256(canon_bytes).hexdigest()
sig = priv.sign(canon_bytes).hex()

d['canonical_payload_sha256'] = canon_sha
d['ed25519_signature'] = {
    'algorithm': 'Ed25519 (RFC 8032)',
    'public_key_fingerprint': '$TEST_AUTH_FP',
    'signed_canonical_payload_sha256': canon_sha,
    'signature_hex': sig
}

with open('$REJECTED_VERDICT', 'w') as f:
    json.dump(d, f, indent=2)
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$REJECTED_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t5_out.txt" 2>&1
T5_RC=$?
set -e

if [[ $T5_RC -ne 1 ]]; then
    echo "FATAL: Authority-rejected verdict was not blocked! Exit $T5_RC" >&2
    cat "$WORK_DIR/t5_out.txt" >&2
    exit 1
fi
if ! grep -q "SOVEREIGN_RELEASE_REJECTED" "$WORK_DIR/t5_out.txt"; then
    echo "FATAL: Expected SOVEREIGN_RELEASE_REJECTED diagnostic missing!" >&2
    cat "$WORK_DIR/t5_out.txt" >&2
    exit 1
fi
echo "✓ TEST 5 PASS: Authority-signed rejection strictly blocked by release policy (Exit 1)"

# ==============================================================================
# TEST 6: BUILD B — ATTACK VECTOR 5 (UNAUTHORIZED ATTACKER SIGNING KEY)
# ==============================================================================
echo -e "\n--- TEST 6: BUILD B (UNAUTHORIZED ATTACKER SIGNING KEY BLOCKED) ---"
ATTACKER_SIGNED_VERDICT="$WORK_DIR/attacker_signed_verdict.json"
cp "$GENUINE_VERDICT" "$ATTACKER_SIGNED_VERDICT"
# Sign with an unauthorized attacker-generated private key
python3 -c "
import json, hashlib
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.primitives import serialization

attacker_priv = ed25519.Ed25519PrivateKey.generate()
attacker_pub = attacker_priv.public_key()
raw_pub = attacker_pub.public_bytes(
    encoding=serialization.Encoding.Raw,
    format=serialization.PublicFormat.Raw
)
fingerprint = hashlib.sha256(raw_pub).hexdigest()

with open('$ATTACKER_SIGNED_VERDICT') as f:
    d = json.load(f)

d['authority_pubkey_fingerprint'] = fingerprint
signable = {
    'authority': d['authority'],
    'authority_workflow': d['authority_workflow'],
    'authority_commit': d['authority_commit'],
    'authority_verifier_sha256': d['authority_verifier_sha256'],
    'authority_policy_sha256': d['authority_policy_sha256'],
    'authority_pubkey_fingerprint': fingerprint,
    'evaluated_producer_repo': d['evaluated_producer_repo'],
    'evaluated_producer_commit': d['evaluated_producer_commit'],
    'evaluated_producer_run_id': str(d['evaluated_producer_run_id']),
    'evaluated_artifact_id': str(d['evaluated_artifact_id']),
    'evaluated_artifact_sha256': d['evaluated_artifact_sha256'],
    'verdict': d['verdict'],
    'status': d['status'],
    'adjudication_timestamp': d['adjudication_timestamp'],
}
canon_bytes = json.dumps(signable, sort_keys=True, indent=2).encode('utf-8')
canon_sha = hashlib.sha256(canon_bytes).hexdigest()
sig = attacker_priv.sign(canon_bytes).hex()

d['canonical_payload_sha256'] = canon_sha
d['ed25519_signature'] = {
    'algorithm': 'Ed25519 (RFC 8032)',
    'public_key_fingerprint': fingerprint,
    'signed_canonical_payload_sha256': canon_sha,
    'signature_hex': sig
}
with open('$ATTACKER_SIGNED_VERDICT', 'w') as f:
    json.dump(d, f, indent=2)
attacker_pub_bytes = attacker_pub.public_bytes(
    encoding=serialization.Encoding.PEM,
    format=serialization.PublicFormat.SubjectPublicKeyInfo
)
with open('$WORK_DIR/attacker_pub.pem', 'wb') as f:
    f.write(attacker_pub_bytes)
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$ATTACKER_SIGNED_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$WORK_DIR/attacker_pub.pem" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t6_out.txt" 2>&1
T6_RC=$?
set -e

if [[ $T6_RC -ne 1 ]]; then
    echo "FATAL: Attacker key signature was not blocked! Exit $T6_RC" >&2
    cat "$WORK_DIR/t6_out.txt" >&2
    exit 1
fi
if ! grep -q "Authority public key fingerprint mismatch" "$WORK_DIR/t6_out.txt"; then
    echo "FATAL: Expected fingerprint mismatch diagnostic missing!" >&2
    cat "$WORK_DIR/t6_out.txt" >&2
    exit 1
fi
echo "✓ TEST 6 PASS: Unauthorized signing key strictly blocked by pinned fingerprint (Exit 1)"

# ==============================================================================
# TEST 7: BUILD B — ATTACK VECTOR 6 (WRONG AUTHORITY COMMIT REVISION)
# ==============================================================================
echo -e "\n--- TEST 7: BUILD B (WRONG AUTHORITY COMMIT REVISION BLOCKED) ---"
set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "deadbeef00000000000000000000000000000000" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t7_out.txt" 2>&1
T7_RC=$?
set -e

if [[ $T7_RC -ne 1 ]]; then
    echo "FATAL: Wrong authority commit was not blocked! Exit $T7_RC" >&2
    cat "$WORK_DIR/t7_out.txt" >&2
    exit 1
fi
if ! grep -q "Authority revision mismatch" "$WORK_DIR/t7_out.txt"; then
    echo "FATAL: Expected authority revision mismatch diagnostic missing!" >&2
    cat "$WORK_DIR/t7_out.txt" >&2
    exit 1
fi
echo "✓ TEST 7 PASS: Wrong authority revision strictly blocked by barrier policy (Exit 1)"

# ==============================================================================
# TEST 8: BUILD B — ATTACK VECTOR 7 (MISSING AUTHORITY VERDICT FILE)
# ==============================================================================
echo -e "\n--- TEST 8: BUILD B (MISSING AUTHORITY VERDICT STRICTLY BLOCKED) ---"
set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$WORK_DIR/non_existent_verdict.json" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" \
    --expected-authority-commit "$AUTH_COMMIT" \
    --authority-pubkey "$TEST_AUTH_PUB" \
    --pinned-fingerprint "$TEST_AUTH_FP" > "$WORK_DIR/t8_out.txt" 2>&1
T8_RC=$?
set -e

if [[ $T8_RC -ne 1 ]]; then
    echo "FATAL: Missing verdict was not blocked! Exit $T8_RC" >&2
    cat "$WORK_DIR/t8_out.txt" >&2
    exit 1
fi
if ! grep -q "Missing authority verdict file" "$WORK_DIR/t8_out.txt"; then
    echo "FATAL: Expected missing verdict diagnostic missing!" >&2
    cat "$WORK_DIR/t8_out.txt" >&2
    exit 1
fi
echo "✓ TEST 8 PASS: Missing authority verdict strictly blocked (Exit 1)"

echo -e "\n======================================================================"
echo "✨ ALL 8 DUAL-BUILD ASYMMETRIC RELEASE BARRIER GAUNTLETS PASSED"
echo "   Build A (Valid Candidate): Approved via Pinned Ed25519 Signature"
echo "   Build B (Corrupted Candidate / Green Producer): Strictly Blocked"
echo "   Build B (Recomputed-Digest Forgery Attack): Decisively Defeated"
echo "   Build B (Signed Rejection): Blocked by Policy Check"
echo "   Build B (Unauthorized Signing Key): Blocked by Fingerprint Check"
echo "   Build B (Wrong Authority Revision): Blocked by Revision Check"
echo "   Build B (Missing Verdict File): Strictly Blocked (Fail Closed)"
echo "   V4-M5-20: Possesses ZERO authority private keys."
echo "   Producer self-certification is mathematically impossible."
echo "======================================================================"
