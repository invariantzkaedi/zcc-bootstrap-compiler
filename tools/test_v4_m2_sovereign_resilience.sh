#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# ZCC OMEGA V4 — Milestone V4-M2: Sovereign Resilience Gauntlet
# ==============================================================================
# Tests external sovereign authority independence and resistance against:
#   1. In-repo policy weakening attacks (V4-01)
#   2. Missing or mismatched commit binding (V4-02)
#   3. Missing or malformed stage-3 receipt digests (V4-03)
#   4. Compromised adjudicator code replacement / NC-21B (V4-04)
#   5. Forged negative control unconditional PASS / NC-21A
# ==============================================================================

EVIDENCE="${1:-evidence/latest}"
RECEIPT="${2:-meta_verification_receipt.json}"

if [[ ! -d "$EVIDENCE" ]]; then
    echo "ERROR: Evidence directory not found: $EVIDENCE" >&2
    exit 1
fi

if [[ ! -f "$RECEIPT" ]]; then
    echo "ERROR: Receipt file not found: $RECEIPT" >&2
    exit 1
fi

EXPECTED_COMMIT="${3:-$(python3 -c 'import json; print(json.load(open("'"$RECEIPT"'")).get("provenance",{}).get("git_commit",""))' 2>/dev/null || true)}"
if [[ -z "$EXPECTED_COMMIT" ]]; then
    EXPECTED_COMMIT="$(git rev-parse HEAD 2>/dev/null || true)"
fi

if [[ -z "$EXPECTED_COMMIT" ]]; then
    echo "ERROR: Could not resolve EXPECTED_COMMIT from receipt or git" >&2
    exit 1
fi

echo "======================================================================"
echo "🛡️  ZCC OMEGA V4 — MILESTONE V4-M2: SOVEREIGN RESILIENCE GAUNTLET"
echo "Target Evidence: $EVIDENCE"
echo "Target Receipt:  $RECEIPT"
echo "Target Commit:   $EXPECTED_COMMIT"
echo "======================================================================"

# --- ESTABLISH ISOLATED EXTERNAL SOVEREIGN AUTHORITY (V4-01) ---
EXT_AUTHORITY_DIR="$(mktemp -d /tmp/zcc_sovereign_authority_XXXXXX)"
trap 'rm -rf "$EXT_AUTHORITY_DIR"' EXIT

cp contracts/sovereign_release_policy.json "$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
cp tools/sovereign_release_verifier.py "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py"
chmod -w "$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
chmod 755 "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py"

PINNED_POLICY="$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
PINNED_VERIFIER="python3 $EXT_AUTHORITY_DIR/sovereign_release_verifier.py"

POLICY_SHA=$(sha256sum "$PINNED_POLICY" | awk '{print $1}')
VERIFIER_SHA=$(sha256sum "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py" | awk '{print $1}')
echo "External Sovereign Authority Established at: $EXT_AUTHORITY_DIR"
echo "  Pinned Policy SHA:   $POLICY_SHA"
echo "  Pinned Verifier SHA: $VERIFIER_SHA"

# ==============================================================================
# TEST 1: POSITIVE CONTROL (GENUINE EVIDENCE ACCEPTANCE)
# ==============================================================================
echo -e "\n--- TEST 1: POSITIVE CONTROL (GENUINE UNCORRUPTED EVIDENCE) ---"
set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$EVIDENCE' --receipt '$RECEIPT' --expected-commit '$EXPECTED_COMMIT'" \
    > "$EXT_AUTHORITY_DIR/test1.stdout" 2> "$EXT_AUTHORITY_DIR/test1.stderr"
t1_rc=$?
set -e

if [[ "$t1_rc" -ne 0 ]]; then
    echo "CRITICAL FAILURE: External verifier rejected genuine uncorrupted evidence (exit $t1_rc)" >&2
    cat "$EXT_AUTHORITY_DIR/test1.stderr" >&2
    exit 1
fi
echo "✓ Test 1 PASS: External authority accepted authentic evidence bundle (exit 0)"

# ==============================================================================
# TEST 2: IN-REPO POLICY WEAKENING RESISTANCE (V4-01)
# ==============================================================================
echo -e "\n--- TEST 2: IN-REPO POLICY WEAKENING RESISTANCE (V4-01) ---"
WORK2="$(mktemp -d)"
cp -a "$EVIDENCE"/. "$WORK2"/
cp "$RECEIPT" "$WORK2"/meta_verification_receipt.json

# Attacker injects weakened policy into repo and forges negative control
python3 - "$WORK2/meta_verification_receipt.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["negative_controls"]["assembly"]["gate_rc"] = 0
d["negative_controls"]["assembly"]["status"] = "PASS"
json.dump(d, open(p, "w"), indent=2)
PY

# Attacker tries evaluating with their weakened in-repo policy
# BUT external authority evaluates against PINNED policy
set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$WORK2' --receipt '$WORK2/meta_verification_receipt.json' --expected-commit '$EXPECTED_COMMIT'" \
    > "$WORK2/t2.stdout" 2> "$WORK2/t2.stderr"
t2_rc=$?
set -e
rm -rf "$WORK2"

if [[ "$t2_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Weakened policy allowed forgery to pass or wrong exit code: $t2_rc" >&2
    exit 1
fi
echo "✓ Test 2 PASS: External authority enforced pinned policy, defeating in-repo weakening attempt (exit 12)"

# ==============================================================================
# TEST 3: COMMIT BINDING ENFORCEMENT & MISMATCH (V4-02)
# ==============================================================================
echo -e "\n--- TEST 3A: COMMIT BINDING OMISSION REJECTION (V4-02) ---"
set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$EVIDENCE' --receipt '$RECEIPT'" \
    > "$EXT_AUTHORITY_DIR/t3a.stdout" 2> "$EXT_AUTHORITY_DIR/t3a.stderr"
t3a_rc=$?
set -e

if [[ "$t3a_rc" -ne 10 ]]; then
    echo "CRITICAL FAILURE: Omitted commit binding was not rejected with exit code 10 (got $t3a_rc)" >&2
    cat "$EXT_AUTHORITY_DIR/t3a.stderr" >&2
    exit 1
fi
if ! grep -q "POLICY VIOLATION (V4-02)" "$EXT_AUTHORITY_DIR/t3a.stderr"; then
    echo "CRITICAL FAILURE: Missing expected diagnostic in stderr for omitted commit binding" >&2
    cat "$EXT_AUTHORITY_DIR/t3a.stderr" >&2
    exit 1
fi
echo "✓ Test 3A PASS: External authority rejected omitted commit binding (exit 10)"

echo -e "\n--- TEST 3B: COMMIT BINDING MISMATCH REJECTION (V4-02) ---"
set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$EVIDENCE' --receipt '$RECEIPT' --expected-commit '0123456789abcdef0123456789abcdef01234567'" \
    > "$EXT_AUTHORITY_DIR/t3b.stdout" 2> "$EXT_AUTHORITY_DIR/t3b.stderr"
t3b_rc=$?
set -e

if [[ "$t3b_rc" -ne 10 ]]; then
    echo "CRITICAL FAILURE: Mismatched commit binding was not rejected with exit code 10 (got $t3b_rc)" >&2
    cat "$EXT_AUTHORITY_DIR/t3b.stderr" >&2
    exit 1
fi
if ! grep -q "LINEAGE MISMATCH (V4-02)" "$EXT_AUTHORITY_DIR/t3b.stderr"; then
    echo "CRITICAL FAILURE: Missing expected diagnostic in stderr for mismatched commit" >&2
    cat "$EXT_AUTHORITY_DIR/t3b.stderr" >&2
    exit 1
fi
echo "✓ Test 3B PASS: External authority rejected commit lineage mismatch (exit 10)"

# ==============================================================================
# TEST 4: MISSING STAGE-3 DIGEST REJECTION (V4-03)
# ==============================================================================
echo -e "\n--- TEST 4: MISSING STAGE-3 DIGEST REJECTION (V4-03) ---"
WORK4="$(mktemp -d)"
cp -a "$EVIDENCE"/. "$WORK4"/
cp "$RECEIPT" "$WORK4"/meta_verification_receipt.json

# Remove stage3_s from receipt digests
python3 - "$WORK4/meta_verification_receipt.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
del d["digests"]["stage3_s"]
json.dump(d, open(p, "w"), indent=2)
PY

set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$WORK4' --receipt '$WORK4/meta_verification_receipt.json' --expected-commit '$EXPECTED_COMMIT'" \
    > "$WORK4/t4.stdout" 2> "$WORK4/t4.stderr"
t4_rc=$?
set -e

if [[ "$t4_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Missing stage-3 digest was not rejected with exit code 12 (got $t4_rc)" >&2
    cat "$WORK4/t4.stderr" >&2
    rm -rf "$WORK4"
    exit 1
fi
if ! grep -q "RECEIPT INCOMPLETE (V4-03)" "$WORK4/t4.stderr"; then
    echo "CRITICAL FAILURE: Missing expected diagnostic in stderr for missing digest" >&2
    cat "$WORK4/t4.stderr" >&2
    rm -rf "$WORK4"
    exit 1
fi
rm -rf "$WORK4"
echo "✓ Test 4 PASS: External authority rejected missing stage-3 digest (exit 12)"

# ==============================================================================
# TEST 5: COMPROMISED ADJUDICATOR SCRIPT MUTATION REJECTION (NC-21B / V4-04)
# ==============================================================================
echo -e "\n--- TEST 5: COMPROMISED ADJUDICATOR SCRIPT MUTATION (NC-21B / V4-04) ---"
WORK5="$(mktemp -d)"
cp -a "$EVIDENCE"/. "$WORK5"/
cp "$RECEIPT" "$WORK5"/meta_verification_receipt.json

# Create mock repository root with tampered verify_meta_receipt.py
MOCK_REPO="$WORK5/mock_repo"
mkdir -p "$MOCK_REPO/tools" "$MOCK_REPO/evidence/latest"
cp -a "$EVIDENCE"/. "$MOCK_REPO/evidence/latest"/
cp "$RECEIPT" "$MOCK_REPO/meta_verification_receipt.json"
cp gate.sh "$MOCK_REPO/gate.sh"

# Tamper with verify_meta_receipt.py to return unconditional PASS
cat << 'PY' > "$MOCK_REPO/tools/verify_meta_receipt.py"
#!/usr/bin/env python3
import sys
print("COMPROMISED ADJUDICATOR: FORGED UNCONDITIONAL PASS")
sys.exit(0)
PY
chmod +x "$MOCK_REPO/tools/verify_meta_receipt.py"

set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$MOCK_REPO/evidence/latest' --receipt '$MOCK_REPO/meta_verification_receipt.json' --expected-commit '$EXPECTED_COMMIT'" \
    > "$WORK5/t5.stdout" 2> "$WORK5/t5.stderr"
t5_rc=$?
set -e

if [[ "$t5_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Tampered adjudicator script was not rejected with exit code 12 (got $t5_rc)" >&2
    cat "$WORK5/t5.stderr" >&2
    rm -rf "$WORK5"
    exit 1
fi
if ! grep -q "ADJUDICATOR COMPROMISED" "$WORK5/t5.stderr"; then
    echo "CRITICAL FAILURE: Missing expected diagnostic in stderr for compromised adjudicator" >&2
    cat "$WORK5/t5.stderr" >&2
    rm -rf "$WORK5"
    exit 1
fi
rm -rf "$WORK5"
echo "✓ Test 5 PASS: External authority detected and rejected compromised adjudicator script (exit 12)"

# ==============================================================================
# TEST 6: FORGED NEGATIVE CONTROL UNCONDITIONAL PASS (NC-21A)
# ==============================================================================
echo -e "\n--- TEST 6: FORGED NEGATIVE CONTROL PASS (NC-21A) ---"
WORK6="$(mktemp -d)"
cp -a "$EVIDENCE"/. "$WORK6"/
cp "$RECEIPT" "$WORK6"/meta_verification_receipt.json

python3 - "$WORK6/meta_verification_receipt.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["negative_controls"]["assembly"]["gate_rc"] = 0
d["negative_controls"]["assembly"]["status"] = "PASS"
json.dump(d, open(p, "w"), indent=2)
PY

set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$WORK6' --receipt '$WORK6/meta_verification_receipt.json' --expected-commit '$EXPECTED_COMMIT'" \
    > "$WORK6/t6.stdout" 2> "$WORK6/t6.stderr"
t6_rc=$?
set -e

if [[ "$t6_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Forged negative control was not rejected with exit code 12 (got $t6_rc)" >&2
    cat "$WORK6/t6.stderr" >&2
    rm -rf "$WORK6"
    exit 1
fi
if ! grep -q "CRITICAL FORGERY DETECTED (NC-21)" "$WORK6/t6.stderr"; then
    echo "CRITICAL FAILURE: Missing expected diagnostic in stderr for NC-21" >&2
    cat "$WORK6/t6.stderr" >&2
    rm -rf "$WORK6"
    exit 1
fi
rm -rf "$WORK6"
echo "✓ Test 6 PASS: External authority rejected forged negative control unconditional PASS (exit 12)"

echo -e "\n======================================================================"
echo "✨ MILESTONE V4-M2 VERIFIED: SOVEREIGN RESILIENCE & COMPROMISED VERIFIER/POLICY RESISTANCE DEFEATED"
echo "======================================================================"
