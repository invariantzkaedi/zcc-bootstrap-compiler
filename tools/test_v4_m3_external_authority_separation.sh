#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# ZCC OMEGA V4 — Milestone V4-M3: External Authority Separation Experiment
# ==============================================================================
# Specification: Coordinated repository-compromise rejection
# 1. Install approved verifier and policy in an external authority directory.
# 2. Prepare an adversarial ZCC checkout with:
#    - Weakened in-repo policy (disabling all requirements)
#    - Tampered in-repo sovereign_release_verifier.py (unconditional exit 0)
#    - Tampered in-repo verify_meta_receipt.py (unconditional exit 0)
#    - Intentionally inconsistent stage artifacts (stage2.s != stage3.s)
#    - Forged receipt (verdict PASS, gate_rc 0, missing stage3 digests)
# 3. Execute the external authority without using ANY executable, policy, or
#    expected commit from the adversarial checkout.
# 4. Assert precise integrity rejection and preserve external runner raw logs.
# 5. Execute positive control against authentic baseline evidence.
# ==============================================================================

BASELINE_EVIDENCE="${1:-evidence/latest}"
BASELINE_RECEIPT="${2:-meta_verification_receipt.json}"
TRUSTED_COMMIT="${3:-$(git rev-parse HEAD 2>/dev/null || true)}"

if [[ -z "$TRUSTED_COMMIT" ]]; then
    echo "ERROR (V4-05): TRUSTED_COMMIT must be provided via argument or git HEAD" >&2
    exit 1
fi

echo "======================================================================"
echo "🛡️  ZCC OMEGA V4 — MILESTONE V4-M3: EXTERNAL AUTHORITY SEPARATION"
echo "Trusted Commit:   $TRUSTED_COMMIT"
echo "Baseline Evidence: $BASELINE_EVIDENCE"
echo "Baseline Receipt:  $BASELINE_RECEIPT"
echo "======================================================================"

# --- STEP 1: INSTALL SEPARATELY PROTECTED EXTERNAL AUTHORITY ---
EXT_AUTHORITY_DIR="$(mktemp -d /tmp/zcc_sovereign_external_authority_XXXXXX)"
ADVERSARIAL_DIR="$(mktemp -d /tmp/zcc_adversarial_checkout_XXXXXX)"
trap 'rm -rf "$EXT_AUTHORITY_DIR" "$ADVERSARIAL_DIR"' EXIT

cp contracts/sovereign_release_policy.json "$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
cp tools/sovereign_release_verifier.py "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py"
chmod -w "$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
chmod 755 "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py"

PINNED_POLICY="$EXT_AUTHORITY_DIR/sovereign_release_policy.json"
PINNED_VERIFIER="python3 $EXT_AUTHORITY_DIR/sovereign_release_verifier.py"

POLICY_SHA=$(sha256sum "$PINNED_POLICY" | awk '{print $1}')
VERIFIER_SHA=$(sha256sum "$EXT_AUTHORITY_DIR/sovereign_release_verifier.py" | awk '{print $1}')

echo "External Sovereign Authority Realm: $EXT_AUTHORITY_DIR"
echo "  Pinned Authority Policy SHA:   $POLICY_SHA"
echo "  Pinned Authority Verifier SHA: $VERIFIER_SHA"

# ==============================================================================
# STEP 2: POSITIVE CONTROL (GENUINE EVIDENCE ACCEPTANCE)
# ==============================================================================
echo -e "\n--- STEP 2: POSITIVE CONTROL (AUTHENTIC EVIDENCE ACCEPTANCE) ---"
set +e
eval "$PINNED_VERIFIER --policy '$PINNED_POLICY' --evidence '$BASELINE_EVIDENCE' --receipt '$BASELINE_RECEIPT' --expected-commit '$TRUSTED_COMMIT'" \
    > "$EXT_AUTHORITY_DIR/pos_ctrl.stdout" 2> "$EXT_AUTHORITY_DIR/pos_ctrl.stderr"
pos_rc=$?
set -e

if [[ "$pos_rc" -ne 0 ]]; then
    echo "CRITICAL FAILURE: External authority rejected genuine uncorrupted baseline (exit $pos_rc)" >&2
    cat "$EXT_AUTHORITY_DIR/pos_ctrl.stderr" >&2
    exit 1
fi
echo "✓ Positive Control PASS: External authority accepted authentic baseline (exit 0)"

# ==============================================================================
# STEP 3: PREPARE ADVERSARIAL ZCC CHECKOUT WITH COORDINATED COMPROMISE
# ==============================================================================
echo -e "\n--- STEP 3: CONSTRUCTING COORDINATED ADVERSARIAL REPOSITORY ---"
mkdir -p "$ADVERSARIAL_DIR/contracts" "$ADVERSARIAL_DIR/tools" "$ADVERSARIAL_DIR/evidence/latest"
cp -a "$BASELINE_EVIDENCE"/. "$ADVERSARIAL_DIR/evidence/latest"/
cp "$BASELINE_RECEIPT" "$ADVERSARIAL_DIR/meta_verification_receipt.json"
cp gate.sh "$ADVERSARIAL_DIR/gate.sh"

# Attack 1: Overwrite in-repo policy to disable all checks
cat << 'JSON' > "$ADVERSARIAL_DIR/contracts/sovereign_release_policy.json"
{
  "policy_version": "COMPROMISED-0.0.0",
  "required_commit_binding": false,
  "stage_identity": {
    "require_double_stage_hash_recomputation": false,
    "require_direct_byte_equality": false,
    "required_stage_pairs": []
  },
  "negative_controls": {
    "disallow_unconditional_pass": false
  },
  "cleanliness": {
    "require_workdir_cleaned": false
  }
}
JSON

# Attack 2: Replace in-repo sovereign_release_verifier.py with unconditional PASS
cat << 'PY' > "$ADVERSARIAL_DIR/tools/sovereign_release_verifier.py"
#!/usr/bin/env python3
import sys
print("ADVERSARIAL VERIFIER: FORGED UNCONDITIONAL SUCCESS")
sys.exit(0)
PY
chmod +x "$ADVERSARIAL_DIR/tools/sovereign_release_verifier.py"

# Attack 3: Replace in-repo verify_meta_receipt.py with unconditional PASS
cat << 'PY' > "$ADVERSARIAL_DIR/tools/verify_meta_receipt.py"
#!/usr/bin/env python3
import sys
print("ADVERSARIAL ADJUDICATOR: FORGED UNCONDITIONAL SUCCESS")
sys.exit(0)
PY
chmod +x "$ADVERSARIAL_DIR/tools/verify_meta_receipt.py"

# Attack 4: Mutate stage artifact bytes (stage2.s != stage3.s)
printf '\x90\x90\x90\x90' >> "$ADVERSARIAL_DIR/evidence/latest/stage3.s"

# Attack 5: Forge receipt to claim unconditional PASS and altered commit
python3 - "$ADVERSARIAL_DIR/meta_verification_receipt.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["verdict"] = "PASS"
d["exit_code"] = 0
d["negative_controls"]["assembly"]["gate_rc"] = 0
d["negative_controls"]["assembly"]["status"] = "PASS"
d["provenance"]["git_commit"] = "badc0ffee0000000000000000000000000000000"
del d["digests"]["stage3_s"]
json.dump(d, open(p, "w"), indent=2)
PY

echo "Adversarial repository constructed:"
echo "  • In-repo policy disabled all rules"
echo "  • In-repo verifier & adjudicator replaced with unconditional PASS stubs"
echo "  • stage3.s modified (byte divergence)"
echo "  • Receipt forged (PASS, gate_rc 0, missing stage3_s, fake commit SHA)"

# ==============================================================================
# STEP 4: EXECUTE SEPARATED EXTERNAL AUTHORITY AGAINST ADVERSARIAL REPOSITORY
# ==============================================================================
echo -e "\n--- STEP 4: EXTERNAL AUTHORITY EVALUATION OF ADVERSARIAL CHECKOUT ---"
set +e
eval "$PINNED_VERIFIER \
    --policy '$PINNED_POLICY' \
    --evidence '$ADVERSARIAL_DIR/evidence/latest' \
    --receipt '$ADVERSARIAL_DIR/meta_verification_receipt.json' \
    --expected-commit '$TRUSTED_COMMIT' \
    --repo '$ADVERSARIAL_DIR'" \
    > "$EXT_AUTHORITY_DIR/adversarial_eval.stdout" \
    2> "$EXT_AUTHORITY_DIR/adversarial_eval.stderr"
adv_rc=$?
set -e

echo "External Authority Evaluation Exit Code: $adv_rc"
cat "$EXT_AUTHORITY_DIR/adversarial_eval.stderr"

if [[ "$adv_rc" -eq 0 ]]; then
    echo "CRITICAL FAILURE: Separated external authority accepted the compromised repository!" >&2
    exit 1
fi

# The external authority must fail with non-zero exit code (10, 11, or 12)
if [[ "$adv_rc" -ne 10 && "$adv_rc" -ne 11 && "$adv_rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: Unexpected exit code $adv_rc (expected 10, 11, or 12)" >&2
    exit 1
fi

echo -e "\n--- STEP 5: PROVENANCE & REJECTION AUDIT RETENTION ---"
cat << AUDIT > "$EXT_AUTHORITY_DIR/rejection_audit.log"
================================================================================
ZCC OMEGA V4 — EXTERNAL AUTHORITY SEPARATION AUDIT RECORD (V4-M3)
================================================================================
Timestamp:          $(date -u +"%Y-%m-%dT%H:%M:%SZ")
Trusted Commit:     $TRUSTED_COMMIT
External Realm:     $EXT_AUTHORITY_DIR
Pinned Policy SHA:  $POLICY_SHA
Pinned Verif SHA:   $VERIFIER_SHA
Adversarial Exit:   $adv_rc
Adversarial Stderr:
$(cat "$EXT_AUTHORITY_DIR/adversarial_eval.stderr")
================================================================================
AUDIT
cat "$EXT_AUTHORITY_DIR/rejection_audit.log"

echo -e "\n======================================================================"
echo "✨ MILESTONE V4-M3 VERIFIED: EXTERNAL AUTHORITY SEPARATION DEFEATS"
echo "   COORDINATED REPOSITORY COMPROMISE"
echo "======================================================================"
