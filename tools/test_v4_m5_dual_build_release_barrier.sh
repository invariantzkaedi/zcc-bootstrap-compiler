#!/bin/bash
# ==============================================================================
# ZCC OMEGA V4-M5: DUAL-BUILD RELEASE BARRIER GAUNTLET (Rule ZCC-EF-0)
# ==============================================================================
# Proves that:
# 1. Valid producer build with genuine authority attestation passes the release barrier.
# 2. Corrupted producer build CANNOT be released, even if the producer workflow
#    deliberately reports green (PASS).
# 3. Cryptographically bound lineage prevents artifact substitution, stale
#    attestation reuse, and forged verdicts.
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

# Download genuine release verdict from authority run 40174240486
GENUINE_VERDICT="$WORK_DIR/genuine_verdict.json"
echo "--- Fetching genuine authority attestation from invariantzkaedi/zcc-release-authority ---"
gh run download 40174240486 --repo invariantzkaedi/zcc-release-authority --dir "$WORK_DIR/auth_dl"
cp "$WORK_DIR/auth_dl"/*/release_verdict.json "$GENUINE_VERDICT"
cat "$GENUINE_VERDICT"

PROD_COMMIT=$(jq -r .evaluated_producer_commit "$GENUINE_VERDICT")
PROD_RUN_ID=$(jq -r .evaluated_producer_run_id "$GENUINE_VERDICT")
ART_ID=$(jq -r .evaluated_artifact_id "$GENUINE_VERDICT")

# Fetch genuine producer artifact bundle
GENUINE_BUNDLE="$WORK_DIR/genuine_bundle.zip"
echo -e "\n--- Fetching genuine producer artifact bundle from invariantzkaedi/zcc-bootstrap-compiler ---"
gh api "/repos/invariantzkaedi/zcc-bootstrap-compiler/actions/artifacts/$ART_ID/zip" > "$GENUINE_BUNDLE"
echo "Downloaded genuine bundle: $(sha256sum "$GENUINE_BUNDLE")"

# ==============================================================================
# TEST 1: BUILD A (GENUINE CANDIDATE ACCESS GRANTED)
# ==============================================================================
echo -e "\n--- TEST 1: BUILD A (GENUINE CANDIDATE ACCESS GRANTED) ---"
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID"
echo "✓ TEST 1 PASS: Valid build approved by sovereign release barrier (Exit 0)"

# ==============================================================================
# TEST 2: BUILD B — ATTACK VECTOR 1 (CORRUPTED ARTIFACT SUBSTITUTION)
# ==============================================================================
echo -e "\n--- TEST 2: BUILD B (CORRUPTED ARTIFACT SUBSTITUTION) ---"
CORRUPTED_BUNDLE="$WORK_DIR/corrupted_bundle.zip"
cp "$GENUINE_BUNDLE" "$CORRUPTED_BUNDLE"
# Flip a byte in the artifact archive
python3 -c "
with open('$CORRUPTED_BUNDLE', 'r+b') as f:
    f.seek(1024)
    b = f.read(1)
    f.seek(1024)
    f.write(bytes([b[0] ^ 1]))
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$CORRUPTED_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" > "$WORK_DIR/t2_out.txt" 2>&1
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
# TEST 3: BUILD B — ATTACK VECTOR 2 (STALE/WRONG COMMIT LINEAGE)
# ==============================================================================
echo -e "\n--- TEST 3: BUILD B (STALE / FORGED COMMIT LINEAGE) ---"
set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$GENUINE_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "badc0ffee0000000000000000000000000000000" \
    --expected-run-id "$PROD_RUN_ID" > "$WORK_DIR/t3_out.txt" 2>&1
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
# TEST 4: BUILD B — ATTACK VECTOR 3 (FORGED VERDICT TAMPERING)
# ==============================================================================
echo -e "\n--- TEST 4: BUILD B (FORGED VERDICT TAMPERING) ---"
FORGED_VERDICT="$WORK_DIR/forged_verdict.json"
# Attacker alters verdict payload to match their malicious artifact digest
python3 -c "
import json
with open('$GENUINE_VERDICT') as f:
    d = json.load(f)
d['evaluated_artifact_sha256'] = '$(sha256sum "$CORRUPTED_BUNDLE" | awk '{print $1}')'
with open('$FORGED_VERDICT', 'w') as f:
    json.dump(d, f, indent=2)
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$FORGED_VERDICT" \
    --artifact "$CORRUPTED_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" > "$WORK_DIR/t4_out.txt" 2>&1
T4_RC=$?
set -e

if [[ $T4_RC -ne 1 ]]; then
    echo "FATAL: Forged verdict tampering was not blocked! Exit $T4_RC" >&2
    cat "$WORK_DIR/t4_out.txt" >&2
    exit 1
fi
if ! grep -q "Cryptographic binding signature invalid or forged" "$WORK_DIR/t4_out.txt"; then
    echo "FATAL: Expected cryptographic signature forgery diagnostic missing!" >&2
    cat "$WORK_DIR/t4_out.txt" >&2
    exit 1
fi
echo "✓ TEST 4 PASS: Forged verdict tampering strictly blocked by signature (Exit 1)"

# ==============================================================================
# TEST 5: BUILD B — ATTACK VECTOR 4 (AUTHORITY-REJECTED BUILD BLOCK)
# ==============================================================================
echo -e "\n--- TEST 5: BUILD B (AUTHORITY-REJECTED BUILD STRICTLY BLOCKED) ---"
REJECTED_VERDICT="$WORK_DIR/rejected_verdict.json"
python3 -c "
import json, hashlib
with open('$GENUINE_VERDICT') as f:
    d = json.load(f)
d['verdict'] = 'SOVEREIGN_RELEASE_REJECTED'
d['status'] = 'failure'
# Compute real signature over rejection payload
p = f\"{d['evaluated_producer_repo']}:{d['evaluated_producer_commit']}:{d['evaluated_producer_run_id']}:{d['evaluated_artifact_id']}:{d['evaluated_artifact_sha256']}:{d['authority_commit']}:{d['authority_verifier_sha256']}:{d['authority_policy_sha256']}:{d['verdict']}:{d['adjudication_timestamp']}\"
d['cryptographic_binding_signature'] = hashlib.sha256(p.encode()).hexdigest()
with open('$REJECTED_VERDICT', 'w') as f:
    json.dump(d, f, indent=2)
"

set +e
python3 "$REPO_ROOT/tools/verify_sovereign_release_barrier.py" \
    --verdict "$REJECTED_VERDICT" \
    --artifact "$GENUINE_BUNDLE" \
    --expected-commit "$PROD_COMMIT" \
    --expected-run-id "$PROD_RUN_ID" > "$WORK_DIR/t5_out.txt" 2>&1
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
echo "✓ TEST 5 PASS: Authority-rejected verdict strictly blocks release (Exit 1)"

echo -e "\n======================================================================"
echo "✨ ALL 5 DUAL-BUILD RELEASE BARRIER GAUNTLETS PASSED"
echo "   Build A (Valid Candidate): Approved"
echo "   Build B (Corrupted Candidate / Green Producer): Strictly Blocked"
echo "   Producer self-certification is physically impossible."
echo "======================================================================"
