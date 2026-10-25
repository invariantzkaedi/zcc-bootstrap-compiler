#!/usr/bin/env bash
# ==============================================================================
# ZCC OMEGA V4 — MILESTONE V4-M4: OUT-OF-TREE AUTHORITY ISOLATION SIMULATION &
# SINGLE-FAULT ISOLATION GAUNTLET (Rule ZCC-EF-0+ Compliance)
#
# Adjudication Scope:
# 1. Out-of-Tree Authority Isolation:
#    - Harness simulates external authority isolation by initializing an out-of-tree
#      Git repository populated from source policy and verifier, checking it out
#      at immutable tag v1.0.0-sovereign-authority.
#    - Full independent authority provenance remains pending deployment of an
#      externally governed, pre-approved release authority repository.
#    - Release controller must supply the expected commit explicitly (V4-13: no git HEAD fallback).
# 2. Single-Fault Isolation Gauntlet (Seven Independently Observable Detection Branches):
#    - Positive Control: Genuine uncorrupted baseline evidence -> Exit 0
#    - Gate 1: Corrupted stage artifact bytes (stage3.s != stage2.s) -> Exit 11
#    - Gate 2: Counterfeit stage digest with clean bytes -> Exit 11
#    - Gate 3: Commit lineage mismatch with clean artifacts -> Exit 10
#    - Gate 4: Missing required stage digest field with clean artifacts -> Exit 12
#    - Gate 5: Compromised in-repo adjudicator script with clean artifacts -> Exit 12
#    - Gate 6: Forged negative control (gate_rc=0) with clean artifacts -> Exit 12
#    - Gate 7: Coordinated multi-fault repository compromise -> Exit 12
# 3. No 'eval' invocation; explicit argument expansion throughout (V4-11).
# 4. Retain full provenance and rejection audit ledger.
# ==============================================================================

set -euo pipefail

BASELINE_EVIDENCE="${1:-evidence/latest}"
BASELINE_RECEIPT="${2:-meta_verification_receipt.json}"
TRUSTED_COMMIT="${3:-}"
SOURCE_POLICY="${4:-contracts/sovereign_release_policy.json}"
SOURCE_VERIFIER="${5:-tools/sovereign_release_verifier.py}"

# V4-13: Require explicit commit supplied by trusted release controller (no silent fallback)
if [[ -z "$TRUSTED_COMMIT" ]]; then
    echo "ERROR (V4-13): TRUSTED_COMMIT must be explicitly provided by the external release controller" >&2
    exit 1
fi

echo "======================================================================"
echo "🛡️  ZCC OMEGA V4 — MILESTONE V4-M4: AUTHORITY ISOLATION & FAULT GAUNTLET"
echo "Trusted Commit:    $TRUSTED_COMMIT"
echo "Baseline Evidence: $BASELINE_EVIDENCE"
echo "Baseline Receipt:  $BASELINE_RECEIPT"
echo "======================================================================"

# ==============================================================================
# STEP 1: INITIALIZE OUT-OF-TREE AUTHORITY REPOSITORY SIMULATION
# ==============================================================================
AUTH_REPO_DIR="$(mktemp -d /tmp/zcc_release_authority_repo_XXXXXX)"
EXT_AUTHORITY_DIR="$(mktemp -d /tmp/zcc_sovereign_authority_XXXXXX)"
WORK_BASE="$(mktemp -d /tmp/zcc_v4_m4_work_XXXXXX)"

cleanup() {
    rm -rf "$AUTH_REPO_DIR" "$EXT_AUTHORITY_DIR" "$WORK_BASE"
}
trap cleanup EXIT

echo -e "\n--- STEP 1: INITIALIZING OUT-OF-TREE AUTHORITY REPOSITORY ---"
git init -q "$AUTH_REPO_DIR"
git -C "$AUTH_REPO_DIR" checkout -q -b main

cp "$SOURCE_POLICY" "$AUTH_REPO_DIR/sovereign_release_policy.json"
cp "$SOURCE_VERIFIER" "$AUTH_REPO_DIR/sovereign_release_verifier.py"

cat << EOF > "$AUTH_REPO_DIR/authority_manifest.json"
{
  "authority_name": "ZCC Sovereign Release Verification Authority",
  "governance_mode": "OUT_OF_TREE_ISOLATION_SIMULATION",
  "authority_standard": "Rule ZCC-EF-0+",
  "immutable_tag": "v1.0.0-sovereign-authority",
  "created_at": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
}
EOF

git -C "$AUTH_REPO_DIR" config user.email "sovereign-authority@zcc.internal"
git -C "$AUTH_REPO_DIR" config user.name "ZCC Sovereign Release Authority"
git -C "$AUTH_REPO_DIR" add .
git -C "$AUTH_REPO_DIR" commit -q -m "feat(authority): initialize immutable sovereign release authority"
git -C "$AUTH_REPO_DIR" tag -a "v1.0.0-sovereign-authority" -m "Immutable Sovereign Release Authority v1.0.0"

# Clone verifier and policy from authority repository at immutable tag
git clone -q --branch "v1.0.0-sovereign-authority" --depth 1 "file://$AUTH_REPO_DIR" "$EXT_AUTHORITY_DIR"

AUTHORITY_COMMIT=$(git -C "$EXT_AUTHORITY_DIR" rev-parse HEAD)
PINNED_POLICY="$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
chmod -w "$PINNED_POLICY"
chmod 755 "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py"

POLICY_SHA=$(sha256sum "$PINNED_POLICY" | awk '{print $1}')
VERIFIER_SHA=$(sha256sum "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py" | awk '{print $1}')

echo "Out-of-Tree Authority Established:"
echo "  Authority Git Repo:      $AUTH_REPO_DIR"
echo "  Immutable Tag:           v1.0.0-sovereign-authority"
echo "  Authority Commit SHA:    $AUTHORITY_COMMIT"
echo "  Pinned Authority Policy: $POLICY_SHA"
echo "  Pinned Authority Verif:  $VERIFIER_SHA"

# V4-11: Explicit function invocation without eval
run_authority_verifier() {
    python3 "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py" \
        --policy "$PINNED_POLICY" \
        "$@"
}

# ==============================================================================
# STEP 2: POSITIVE CONTROL (GENUINE EVIDENCE ACCEPTANCE)
# ==============================================================================
echo -e "\n--- STEP 2: POSITIVE CONTROL (AUTHENTIC EVIDENCE ACCEPTANCE) ---"
set +e
run_authority_verifier \
    --evidence "$BASELINE_EVIDENCE" \
    --receipt "$BASELINE_RECEIPT" \
    --expected-commit "$TRUSTED_COMMIT" \
    > "$EXT_AUTHORITY_DIR/pos_ctrl.stdout" 2> "$EXT_AUTHORITY_DIR/pos_ctrl.stderr"
pos_rc=$?
set -e

if [[ "$pos_rc" -ne 0 ]]; then
    echo "CRITICAL FAILURE: Authority rejected authentic uncorrupted baseline (exit $pos_rc)" >&2
    cat "$EXT_AUTHORITY_DIR/pos_ctrl.stderr" >&2
    exit 1
fi
echo "✓ Positive Control PASS: Authority accepted authentic baseline (exit 0)"

# Helper to populate a clean test tree
prepare_clean_test_tree() {
    local target_dir="$1"
    mkdir -p "$target_dir/evidence/latest" "$target_dir/contracts" "$target_dir/tools"
    cp -a "$BASELINE_EVIDENCE"/. "$target_dir/evidence/latest"/
    cp "$BASELINE_RECEIPT" "$target_dir/meta_verification_receipt.json"
    cp gate.sh "$target_dir/gate.sh"
    cp tools/verify_meta_receipt.py "$target_dir/tools/verify_meta_receipt.py"

    # Inject weakened producer verifier and policy to test resistance against producer collusion
    cat << 'JSON' > "$target_dir/contracts/sovereign_release_policy.json"
{
  "policy_version": "COMPROMISED-0.0.0",
  "required_commit_binding": false,
  "stage_identity": {
    "require_direct_byte_equality": false,
    "require_double_stage_hash_recomputation": false,
    "required_stage_pairs": []
  },
  "negative_controls": {
    "disallow_unconditional_pass": false
  }
}
JSON
    cat << 'PY' > "$target_dir/tools/sovereign_release_verifier.py"
#!/usr/bin/env python3
import sys
print("ADVERSARIAL PRODUCER VERIFIER: FORGED UNCONDITIONAL SUCCESS")
sys.exit(0)
PY
    chmod +x "$target_dir/tools/sovereign_release_verifier.py"
}

# ==============================================================================
# STEP 3: DECISIVE GATE 1 — CORRUPTED ARTIFACT REJECTION UNDER PRODUCER PASS
# ==============================================================================
echo -e "\n--- STEP 3: DECISIVE GATE 1 (CORRUPTED ARTIFACT REJECTION) ---"
TREE1="$WORK_BASE/tree1"
prepare_clean_test_tree "$TREE1"

# Invalidate stage3.s bytes (append 4 NOP bytes: byte divergence)
printf '\x90\x90\x90\x90' >> "$TREE1/evidence/latest/stage3.s"

set +e
run_authority_verifier \
    --evidence "$TREE1/evidence/latest" \
    --receipt "$TREE1/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE1" \
    > "$TREE1/stdout.log" 2> "$TREE1/stderr.log"
g1_rc=$?
set -e

if [[ "$g1_rc" -ne 11 ]]; then
    echo "CRITICAL FAILURE: Corrupted artifact was not rejected with exit code 11 (got $g1_rc)" >&2
    cat "$TREE1/stderr.log" >&2
    exit 1
fi
if ! grep -q "RAW BYTE INEQUALITY: stage2.s and stage3.s bytes do not match" "$TREE1/stderr.log" && \
   ! grep -q "COUNTERFEIT STAGE 3 DIGEST" "$TREE1/stderr.log"; then
    echo "CRITICAL FAILURE: Expected artifact inequality diagnostic not found in stderr" >&2
    cat "$TREE1/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 1 PASS: Authority detected corrupted artifact and rejected with exit 11"
echo "  Observed stderr: $(cat "$TREE1/stderr.log")"

# ==============================================================================
# STEP 4: DECISIVE GATE 2 — CORRUPTED DIGEST REJECTION
# ==============================================================================
echo -e "\n--- STEP 4: DECISIVE GATE 2 (CORRUPTED DIGEST REJECTION) ---"
TREE2="$WORK_BASE/tree2"
prepare_clean_test_tree "$TREE2"

# Tamper with stage3_s digest in receipt while leaving raw bytes identical
python3 - "$TREE2/meta_verification_receipt.json" << 'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["digests"]["stage3_s"] = "0000000000000000000000000000000000000000000000000000000000000000"
json.dump(d, open(p, "w"), indent=2)
PY

set +e
run_authority_verifier \
    --evidence "$TREE2/evidence/latest" \
    --receipt "$TREE2/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE2" \
    > "$TREE2/stdout.log" 2> "$TREE2/stderr.log"
g2_rc=$?
set -e

if [[ "$g2_rc" -ne 11 ]]; then
    echo "CRITICAL FAILURE: Corrupted digest was not rejected with exit code 11 (got $g2_rc)" >&2
    cat "$TREE2/stderr.log" >&2
    exit 1
fi
if ! grep -q "COUNTERFEIT STAGE 3 DIGEST (V4-03)" "$TREE2/stderr.log"; then
    echo "CRITICAL FAILURE: Expected digest counterfeit diagnostic not found in stderr" >&2
    cat "$TREE2/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 2 PASS: Authority detected counterfeit digest and rejected with exit 11"
echo "  Observed stderr: $(cat "$TREE2/stderr.log")"

# ==============================================================================
# STEP 5: DECISIVE GATE 3 — LINEAGE COMMIT MISMATCH REJECTION
# ==============================================================================
echo -e "\n--- STEP 5: DECISIVE GATE 3 (COMMIT LINEAGE MISMATCH REJECTION) ---"
TREE3="$WORK_BASE/tree3"
prepare_clean_test_tree "$TREE3"

# Alter receipt commit to fake SHA
python3 - "$TREE3/meta_verification_receipt.json" << 'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["provenance"]["git_commit"] = "deadbeef00000000000000000000000000000000"
json.dump(d, open(p, "w"), indent=2)
PY

set +e
run_authority_verifier \
    --evidence "$TREE3/evidence/latest" \
    --receipt "$TREE3/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE3" \
    > "$TREE3/stdout.log" 2> "$TREE3/stderr.log"
g3_rc=$?
set -e

if [[ "$g3_rc" -ne 10 ]]; then
    echo "CRITICAL FAILURE: Commit lineage mismatch was not rejected with exit code 10 (got $g3_rc)" >&2
    cat "$TREE3/stderr.log" >&2
    exit 1
fi
if ! grep -q "LINEAGE MISMATCH (V4-02)" "$TREE3/stderr.log"; then
    echo "CRITICAL FAILURE: Expected lineage mismatch diagnostic not found in stderr" >&2
    cat "$TREE3/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 3 PASS: Authority detected lineage mismatch and rejected with exit 10"
echo "  Observed stderr: $(cat "$TREE3/stderr.log")"

# ==============================================================================
# STEP 6: DECISIVE GATE 4 — MISSING DIGEST FIELD REJECTION
# ==============================================================================
echo -e "\n--- STEP 6: DECISIVE GATE 4 (MISSING DIGEST FIELD REJECTION) ---"
TREE4="$WORK_BASE/tree4"
prepare_clean_test_tree "$TREE4"

# Remove stage3_bin_norm from receipt
python3 - "$TREE4/meta_verification_receipt.json" << 'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
del d["digests"]["stage3_bin_norm"]
json.dump(d, open(p, "w"), indent=2)
PY

set +e
run_authority_verifier \
    --evidence "$TREE4/evidence/latest" \
    --receipt "$TREE4/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE4" \
    > "$TREE4/stdout.log" 2> "$TREE4/stderr.log"
g4_rc=$?
set -e

if [[ "$g4_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Missing digest field was not rejected with exit code 12 (got $g4_rc)" >&2
    cat "$TREE4/stderr.log" >&2
    exit 1
fi
if ! grep -q "RECEIPT INCOMPLETE (V4-03)" "$TREE4/stderr.log"; then
    echo "CRITICAL FAILURE: Expected missing digest diagnostic not found in stderr" >&2
    cat "$TREE4/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 4 PASS: Authority detected incomplete receipt and rejected with exit 12"
echo "  Observed stderr: $(cat "$TREE4/stderr.log")"

# ==============================================================================
# STEP 7: DECISIVE GATE 5 — COMPROMISED ADJUDICATOR SCRIPT MUTATION
# ==============================================================================
echo -e "\n--- STEP 7: DECISIVE GATE 5 (COMPROMISED ADJUDICATOR REJECTION) ---"
TREE5="$WORK_BASE/tree5"
prepare_clean_test_tree "$TREE5"

# Mutate verify_meta_receipt.py in repo
cat << 'PY' > "$TREE5/tools/verify_meta_receipt.py"
#!/usr/bin/env python3
import sys
print("COMPROMISED ADJUDICATOR: FORGED PASS")
sys.exit(0)
PY
chmod +x "$TREE5/tools/verify_meta_receipt.py"

set +e
run_authority_verifier \
    --evidence "$TREE5/evidence/latest" \
    --receipt "$TREE5/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE5" \
    > "$TREE5/stdout.log" 2> "$TREE5/stderr.log"
g5_rc=$?
set -e

if [[ "$g5_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Compromised adjudicator was not rejected with exit code 12 (got $g5_rc)" >&2
    cat "$TREE5/stderr.log" >&2
    exit 1
fi
if ! grep -q "ADJUDICATOR COMPROMISED (NC-21B / NC-25)" "$TREE5/stderr.log"; then
    echo "CRITICAL FAILURE: Expected adjudicator compromise diagnostic not found in stderr" >&2
    cat "$TREE5/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 5 PASS: Authority detected adjudicator tampering and rejected with exit 12"
echo "  Observed stderr: $(cat "$TREE5/stderr.log")"

# ==============================================================================
# STEP 8: DECISIVE GATE 6 — FORGED NEGATIVE CONTROL PASS REJECTION
# ==============================================================================
echo -e "\n--- STEP 8: DECISIVE GATE 6 (FORGED NEGATIVE CONTROL REJECTION) ---"
TREE6="$WORK_BASE/tree6"
prepare_clean_test_tree "$TREE6"

# Inject gate_rc=0 into assembly negative control
python3 - "$TREE6/meta_verification_receipt.json" << 'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["negative_controls"]["assembly"]["gate_rc"] = 0
json.dump(d, open(p, "w"), indent=2)
PY

set +e
run_authority_verifier \
    --evidence "$TREE6/evidence/latest" \
    --receipt "$TREE6/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE6" \
    > "$TREE6/stdout.log" 2> "$TREE6/stderr.log"
g6_rc=$?
set -e

if [[ "$g6_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Forged negative control was not rejected with exit code 12 (got $g6_rc)" >&2
    cat "$TREE6/stderr.log" >&2
    exit 1
fi
if ! grep -q "CRITICAL FORGERY DETECTED (NC-21)" "$TREE6/stderr.log"; then
    echo "CRITICAL FAILURE: Expected negative control forgery diagnostic not found in stderr" >&2
    cat "$TREE6/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 6 PASS: Authority detected forged negative control and rejected with exit 12"
echo "  Observed stderr: $(cat "$TREE6/stderr.log")"

# ==============================================================================
# STEP 9: DECISIVE GATE 7 — COORDINATED COMPROMISE REJECTION
# ==============================================================================
echo -e "\n--- STEP 9: DECISIVE GATE 7 (COORDINATED COMPROMISE REJECTION) ---"
TREE7="$WORK_BASE/tree7"
prepare_clean_test_tree "$TREE7"

printf '\x90\x90\x90\x90' >> "$TREE7/evidence/latest/stage3.s"
cat << 'PY' > "$TREE7/tools/verify_meta_receipt.py"
#!/usr/bin/env python3
import sys
sys.exit(0)
PY
python3 - "$TREE7/meta_verification_receipt.json" << 'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["verdict"] = "PASS"
d["exit_code"] = 0
d["negative_controls"]["assembly"]["gate_rc"] = 0
d["provenance"]["git_commit"] = "badc0ffee0000000000000000000000000000000"
del d["digests"]["stage3_s"]
json.dump(d, open(p, "w"), indent=2)
PY

set +e
run_authority_verifier \
    --evidence "$TREE7/evidence/latest" \
    --receipt "$TREE7/meta_verification_receipt.json" \
    --expected-commit "$TRUSTED_COMMIT" \
    --repo "$TREE7" \
    > "$TREE7/stdout.log" 2> "$TREE7/stderr.log"
g7_rc=$?
set -e

if [[ "$g7_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Coordinated compromise was not rejected with exit code 12 (got $g7_rc)" >&2
    cat "$TREE7/stderr.log" >&2
    exit 1
fi
echo "✓ Gate 7 PASS: Authority rejected coordinated multi-fault compromise (exit 12)"

# ==============================================================================
# STEP 10: PROVENANCE & AUDIT LOG RETENTION
# ==============================================================================
echo -e "\n--- STEP 10: RETAINING AUTHORITY AUDIT LEDGER ---"
AUDIT_LOG="evidence/latest/v4_m4_authority_audit.log"
mkdir -p "$(dirname "$AUDIT_LOG")"
cat << AUDIT > "$AUDIT_LOG"
================================================================================
ZCC OMEGA V4 — AUTHORITY ISOLATION & SINGLE-FAULT AUDIT (V4-M4)
================================================================================
Timestamp:            $(date -u +"%Y-%m-%dT%H:%M:%SZ")
Authority Repository: $AUTH_REPO_DIR
Authority Tag:        v1.0.0-sovereign-authority
Authority Commit SHA: $AUTHORITY_COMMIT
Authority Policy SHA: $POLICY_SHA
Authority Verif SHA:  $VERIFIER_SHA
Trusted Commit Under Test: $TRUSTED_COMMIT
Execution Results:
  - Positive Control (Genuine Baseline):   PASS (Exit $pos_rc)
  - Gate 1 (Corrupted Artifact Bytes):     REJECTED (Exit $g1_rc, $(cat "$TREE1/stderr.log"))
  - Gate 2 (Counterfeit Digest):           REJECTED (Exit $g2_rc, $(cat "$TREE2/stderr.log"))
  - Gate 3 (Lineage Commit Mismatch):      REJECTED (Exit $g3_rc, $(cat "$TREE3/stderr.log"))
  - Gate 4 (Missing Required Digest):      REJECTED (Exit $g4_rc, $(cat "$TREE4/stderr.log"))
  - Gate 5 (Compromised Adjudicator):      REJECTED (Exit $g5_rc, $(cat "$TREE5/stderr.log"))
  - Gate 6 (Forged Negative Control):      REJECTED (Exit $g6_rc, $(cat "$TREE6/stderr.log"))
  - Gate 7 (Coordinated Multi-Fault):      REJECTED (Exit $g7_rc, $(cat "$TREE7/stderr.log"))
================================================================================
AUDIT
cat "$AUDIT_LOG"

echo -e "\n======================================================================"
echo "✨ MILESTONE V4-M4 VERIFIED: AUTHORITY ISOLATION &"
echo "   SINGLE-FAULT ISOLATION GAUNTLET PASSING ACROSS ALL 7 GATES"
echo "======================================================================"
