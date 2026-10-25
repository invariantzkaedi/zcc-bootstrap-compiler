#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# ZCC OMEGA V4 — Milestone V4-M1: Compromised Adjudicator Rejection (NC-21)
# ==============================================================================
# Proves that an independent sovereign release authority strictly rejects a
# producer-side forgery claiming unconditional PASS with gate_rc=0.
#
# Arguments:
# $1: independently pinned verifier executable (e.g. python3 tools/sovereign_release_verifier.py)
# $2: valid baseline evidence directory (e.g. ./evidence/latest)
# $3: externally maintained adjudicator trust policy (e.g. contracts/sovereign_release_policy.json)
# ==============================================================================

VERIFIER="${1:?verifier required}"
EVIDENCE="${2:?evidence required}"
POLICY="${3:?policy required}"

echo "======================================================================"
echo "🛡️  ZCC OMEGA V4 — MILESTONE V4-M1: NC-21 FORGED ADJUDICATOR TEST"
echo "Verifier: $VERIFIER"
echo "Evidence: $EVIDENCE"
echo "Policy:   $POLICY"
echo "======================================================================"

# --- STEP 1: POSITIVE CONTROL (GENUINE EVIDENCE) ---
echo -e "\n--- STEP 1: POSITIVE CONTROL (GENUINE UNCORRUPTED EVIDENCE) ---"
set +e
eval "$VERIFIER --policy '$POLICY' --evidence '$EVIDENCE'" > /tmp/pos_ctrl.stdout 2> /tmp/pos_ctrl.stderr
pos_rc=$?
set -e

if [[ "$pos_rc" -ne 0 ]]; then
    echo "FAIL: Positive control failed on genuine evidence (exit $pos_rc)" >&2
    cat /tmp/pos_ctrl.stderr >&2
    exit 1
fi
echo "✓ Positive control PASS: External verifier accepted genuine uncorrupted evidence (exit 0)"

# --- STEP 2: ADVERSARIAL FORGERY ATTACK (NC-21) ---
echo -e "\n--- STEP 2: ADVERSARIAL FORGERY INJECTION (NC-21) ---"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK" /tmp/pos_ctrl.stdout /tmp/pos_ctrl.stderr' EXIT

cp -a "$EVIDENCE"/. "$WORK"/
# If meta_verification_receipt.json is at repo root, copy it into WORK
if [[ ! -f "$WORK/meta_verification_receipt.json" && -f "meta_verification_receipt.json" ]]; then
    cp -a "meta_verification_receipt.json" "$WORK"/
fi

# Invert / forge the producer-controlled verdict
python3 - "$WORK/meta_verification_receipt.json" <<'PY'
import json
import sys
from pathlib import Path

p = Path(sys.argv[1])
d = json.loads(p.read_text())
d["verdict"] = "PASS"
d["exit_code"] = 0
d["negative_controls"]["assembly"]["gate_rc"] = 0
d["negative_controls"]["assembly"]["status"] = "PASS"
p.write_text(json.dumps(d, indent=2))
PY

echo "Injected forged unconditional PASS (gate_rc=0) into receipt in isolated workdir."

set +e
eval "$VERIFIER --policy '$POLICY' --evidence '$WORK'" \
    > "$WORK/verifier.stdout" \
    2> "$WORK/verifier.stderr"
rc=$?
set -e

if [[ "$rc" -eq 0 ]]; then
    echo "CRITICAL FAILURE: forged evidence was accepted by the verifier!" >&2
    exit 1
fi

if [[ "$rc" -ne 12 ]]; then
    echo "CRITICAL FAILURE: verifier rejected forgery with exit code $rc instead of expected EXIT_FORGERY_DETECTED (12)" >&2
    cat "$WORK/verifier.stderr" >&2
    exit 1
fi

if ! grep -q "CRITICAL FORGERY DETECTED" "$WORK/verifier.stderr"; then
    echo "CRITICAL FAILURE: verifier rejected forgery but expected diagnostic missing from stderr" >&2
    cat "$WORK/verifier.stderr" >&2
    exit 1
fi

echo "✓ NC-21 PASS: External verifier successfully rejected the forged unconditional PASS"
echo "Verifier exit code: $rc (EXIT_FORGERY_DETECTED)"
cat "$WORK/verifier.stderr"
echo -e "\n======================================================================"
echo "✨ MILESTONE V4-M1 VERIFIED: FORGED PRODUCER ADJUDICATOR DEFEATED"
echo "======================================================================"
