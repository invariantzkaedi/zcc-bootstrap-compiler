#!/usr/bin/env bash
# ==============================================================================
# test_adjudication_negative_control.sh
# Verification Architect Adjudication Gate — Rule ZCC-EF-0 Hardened Negative Controls
# ==============================================================================
# Invariants Enforced (EF-01 through EF-06):
#   - EF-01: Artifact directory is a mandatory, explicit CLI parameter (no fallback).
#   - EF-02: Universal EXIT trap guarantees failure-receipt emission even on unexpected exit.
#   - EF-03: Workdir cleanup is executed after receipt emission and cleanup status is recorded.
#   - EF-04: Harness script's own SHA-256 is measured and anchored in receipt.
#   - EF-05: Git commit, working tree dirty status, and branch are recorded.
#   - EF-06: Exact toolchain provenance (GCC, AS, LD, Python, OS) captured.
# ==============================================================================

set -uo pipefail

RECEIPT_PATH="${1:?receipt path required as argument 1}"
EVIDENCE_SRC="${2:?artifact directory required as argument 2}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR=$(mktemp -d -t zcc_neg_ctrl_XXXXXX)

# State Tracking for Universal Receipt Finalization
CURRENT_STAGE="PREFLIGHT"
FINAL_VERDICT="INCOMPLETE"
EXIT_CODE=0
ERROR_MESSAGE=""
LAST_DIAGNOSTIC=""
CLEANUP_STATUS="PENDING"

# Toolchain & Provenance Metadata (EF-04, EF-05, EF-06)
GATE_SCRIPT="$REPO_ROOT/gate.sh"
HARNESS_SCRIPT="$(realpath "$0")"
HARNESS_SHA256=$(sha256sum "$HARNESS_SCRIPT" | awk '{print $1}')
GATE_SHA256=$(sha256sum "$GATE_SCRIPT" 2>/dev/null | awk '{print $1}' || echo "UNKNOWN")
EXPECTED_GATE_SHA256=$(git -C "$REPO_ROOT" show HEAD:gate.sh 2>/dev/null | sha256sum | awk '{print $1}' || echo "UNKNOWN")
if [ "$GATE_SHA256" = "$EXPECTED_GATE_SHA256" ] && [ "$GATE_SHA256" != "UNKNOWN" ]; then
    GATE_HASH_VERIFIED="MATCH_COMMITTED_HEAD"
else
    GATE_HASH_VERIFIED="LOCAL_WORKING_TREE_MODIFIED"
fi
COMMIT_SHA=$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo "UNKNOWN_COMMIT")
if [ -z "$(git -C "$REPO_ROOT" status --porcelain 2>/dev/null)" ]; then
    GIT_DIRTY="CLEAN"
else
    DIRTY_COUNT=$(git -C "$REPO_ROOT" status --porcelain 2>/dev/null | wc -l)
    GIT_DIRTY="DIRTY ($DIRTY_COUNT modified/untracked files)"
fi
GIT_BRANCH=$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "UNKNOWN_BRANCH")

GCC_VER=$(gcc --version 2>/dev/null | head -n 1 || echo "unknown")
AS_VER=$(as --version 2>/dev/null | head -n 1 || echo "unknown")
LD_VER=$(ld --version 2>/dev/null | head -n 1 || echo "unknown")
PYTHON_VER=$(python3 --version 2>/dev/null || echo "unknown")
OS_NAME=$(grep PRETTY_NAME /etc/os-release 2>/dev/null | cut -d= -f2 | tr -d '"' || uname -s)

CI_GITHUB_SHA="${GITHUB_SHA:-LOCAL_DEV}"
CI_GITHUB_RUN_ID="${GITHUB_RUN_ID:-LOCAL_RUN}"
CI_GITHUB_RUN_ATTEMPT="${GITHUB_RUN_ATTEMPT:-1}"

# Test Data Holders
S2_SHA="UNCOMPUTED"; S3_SHA="UNCOMPUTED"
O2_SHA="UNCOMPUTED"; O3_SHA="UNCOMPUTED"
B2_SHA="UNCOMPUTED"; B3_SHA="UNCOMPUTED"
CMP_ASM_RC=-1; GATE_ASM_RC=-1; DIAG_ASM=""
OBJ_ORIG_BYTE="UNKNOWN"; GATE_OBJ_RC=-1; DIAG_OBJ=""
CMP_BIN_RC=-1; BIN_ORIG_BYTE="UNKNOWN"; GATE_BIN_RC=-1; DIAG_BIN=""

# Finalization Trap: Always cleans up workdir, validates cleanup, then writes receipt (EF-02, EF-03)
finalize_receipt() {
    local trap_rc=$?
    if [ "$trap_rc" -ne 0 ] && [ "$FINAL_VERDICT" != "PASS" ]; then
        FINAL_VERDICT="FAIL"
        EXIT_CODE=$trap_rc
        if [ -z "$ERROR_MESSAGE" ]; then
            ERROR_MESSAGE="Script aborted during stage: $CURRENT_STAGE"
        fi
    fi

    # EF-03: Execute cleanup FIRST, verify it, then record verified cleanup status
    local cleanup_ok=false
    if [ -d "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR"
        if [ ! -d "$WORK_DIR" ]; then
            cleanup_ok=true
        fi
    else
        cleanup_ok=true
    fi

    # Emit JSON receipt safely using python3 json.dump (P2 - eliminates shell interpolation syntax breakage)
    mkdir -p "$(dirname "$RECEIPT_PATH")"
    export FINAL_VERDICT EXIT_CODE CURRENT_STAGE ERROR_MESSAGE
    export COMMIT_SHA GIT_BRANCH GIT_DIRTY GATE_SHA256 EXPECTED_GATE_SHA256 GATE_HASH_VERIFIED HARNESS_SHA256
    export CI_GITHUB_SHA CI_GITHUB_RUN_ID CI_GITHUB_RUN_ATTEMPT
    export GCC_VER AS_VER LD_VER PYTHON_VER OS_NAME EVIDENCE_SRC
    export S2_SHA S3_SHA O2_SHA O3_SHA B2_SHA B3_SHA
    export CMP_ASM_RC GATE_ASM_RC DIAG_ASM
    export OBJ_ORIG_BYTE GATE_OBJ_RC DIAG_OBJ
    export BIN_ORIG_BYTE CMP_BIN_RC GATE_BIN_RC DIAG_BIN
    export WORK_DIR CLEANUP_OK="$cleanup_ok" RECEIPT_PATH

    python3 - <<'PY'
import json
import os
from datetime import datetime, timezone

now_iso = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
final_verdict = os.environ.get("FINAL_VERDICT", "INCOMPLETE")
exit_code = int(os.environ.get("EXIT_CODE", "1"))
last_stage = os.environ.get("CURRENT_STAGE", "UNKNOWN")
error_message = os.environ.get("ERROR_MESSAGE", "")

def safe_int(val, default=-1):
    try:
        return int(val)
    except Exception:
        return default

gate_asm_rc = safe_int(os.environ.get("GATE_ASM_RC"))
gate_obj_rc = safe_int(os.environ.get("GATE_OBJ_RC"))
gate_bin_rc = safe_int(os.environ.get("GATE_BIN_RC"))
cmp_asm_rc = safe_int(os.environ.get("CMP_ASM_RC"))
cmp_bin_rc = safe_int(os.environ.get("CMP_BIN_RC"))

receipt = {
    "timestamp_utc": now_iso,
    "verdict": final_verdict,
    "exit_code": exit_code,
    "last_stage": last_stage,
    "error_message": error_message,
    "provenance": {
        "git_commit": os.environ.get("COMMIT_SHA", "UNKNOWN"),
        "git_branch": os.environ.get("GIT_BRANCH", "UNKNOWN"),
        "git_dirty": os.environ.get("GIT_DIRTY", "CLEAN"),
        "gate_script_sha256": os.environ.get("GATE_SHA256", "UNKNOWN"),
        "expected_gate_script_sha256": os.environ.get("EXPECTED_GATE_SHA256", "UNKNOWN"),
        "gate_hash_status": os.environ.get("GATE_HASH_VERIFIED", "UNKNOWN"),
        "harness_script_sha256": os.environ.get("HARNESS_SHA256", "UNKNOWN"),
        "ci_context": {
            "github_sha": os.environ.get("CI_GITHUB_SHA", "LOCAL_DEV"),
            "github_run_id": os.environ.get("CI_GITHUB_RUN_ID", "LOCAL_RUN"),
            "github_run_attempt": os.environ.get("CI_GITHUB_RUN_ATTEMPT", "1"),
        },
        "toolchain": {
            "gcc": os.environ.get("GCC_VER", "unknown"),
            "as": os.environ.get("AS_VER", "unknown"),
            "ld": os.environ.get("LD_VER", "unknown"),
            "python": os.environ.get("PYTHON_VER", "unknown"),
            "os": os.environ.get("OS_NAME", "unknown"),
        },
        "source_artifacts_dir": os.environ.get("EVIDENCE_SRC", "UNRESOLVED"),
    },
    "digests": {
        "stage2_s": os.environ.get("S2_SHA", "UNCOMPUTED"),
        "stage3_s": os.environ.get("S3_SHA", "UNCOMPUTED"),
        "stage2_o": os.environ.get("O2_SHA", "UNCOMPUTED"),
        "stage3_o": os.environ.get("O3_SHA", "UNCOMPUTED"),
        "stage2_bin_norm": os.environ.get("B2_SHA", "UNCOMPUTED"),
        "stage3_bin_norm": os.environ.get("B3_SHA", "UNCOMPUTED"),
    },
    "positive_controls": {
        "cmp_assembly": 0,
        "cmp_object": 0,
        "cmp_binary_norm": 0,
        "status": "PASS" if final_verdict == "PASS" else "UNVERIFIED",
    },
    "negative_controls": {
        "assembly": {
            "mutation": "Appended comment '# ZCC_NEGATIVE_CONTROL' to stage3.s",
            "cmp_rc": cmp_asm_rc,
            "gate_rc": gate_asm_rc,
            "expected_gate_rc": 20,
            "designated_error": "EXIT_ASM_DIVERG",
            "diagnostic": os.environ.get("DIAG_ASM", ""),
            "status": "PASS" if gate_asm_rc == 20 else "FAIL",
        },
        "object": {
            "mutation": f"XOR bitflip at byte offset 128 ({os.environ.get('OBJ_ORIG_BYTE', 'UNKNOWN')})",
            "gate_rc": gate_obj_rc,
            "expected_gate_rc": 21,
            "designated_error": "EXIT_OBJ_DIVERG",
            "diagnostic": os.environ.get("DIAG_OBJ", ""),
            "status": "PASS" if gate_obj_rc == 21 else "FAIL",
        },
        "binary": {
            "mutation": f"XOR bitflip at byte offset 256 ({os.environ.get('BIN_ORIG_BYTE', 'UNKNOWN')}) in stage3.bin, re-normalized via objcopy",
            "cmp_rc": cmp_bin_rc,
            "gate_rc": gate_bin_rc,
            "expected_gate_rc": 22,
            "designated_error": "EXIT_BIN_DIVERG",
            "diagnostic": os.environ.get("DIAG_BIN", ""),
            "status": "PASS" if gate_bin_rc == 22 else "FAIL",
        },
    },
    "cleanup": {
        "work_dir": os.environ.get("WORK_DIR", ""),
        "harness_workdir_cleaned": os.environ.get("CLEANUP_OK") == "true",
    },
}

with open(os.environ["RECEIPT_PATH"], "w") as f:
    json.dump(receipt, f, indent=2)
    f.write("\n")
PY

    exit "$EXIT_CODE"
}
trap 'finalize_receipt' EXIT

echo "================================================================================"
echo "   🔬 ZCC-EF-0 HARNESS MUTATION SENSITIVITY & ADJUDICATION GAUNTLET (v2.3)"
echo "================================================================================"
echo "Git Commit:    $COMMIT_SHA"
echo "gate.sh SHA:   $GATE_SHA256"
echo "Harness SHA:   $HARNESS_SHA256"
echo "Isolated Dir:  $WORK_DIR"
echo "Receipt Path:  $RECEIPT_PATH"
echo ""

# Stage 0: Validate Mandatory Explicit Input Contract (EF-01)
CURRENT_STAGE="VALIDATE_INPUTS"
if [ ! -d "$EVIDENCE_SRC" ]; then
    ERROR_MESSAGE="Input directory does not exist: $EVIDENCE_SRC"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 30
fi
EVIDENCE_SRC="$(realpath -e "$EVIDENCE_SRC")"
echo "Artifact Directory: $EVIDENCE_SRC"

REQUIRED_ARTIFACTS=(
    "stage2.s" "stage3.s"
    "stage2.o" "stage3.o"
    "stage2.bin" "stage3.bin"
    "stage2.bin.norm" "stage3.bin.norm"
    "provenance.jsonl"
)
for artifact in "${REQUIRED_ARTIFACTS[@]}"; do
    target_file="$EVIDENCE_SRC/$artifact"
    if [ ! -f "$target_file" ]; then
        ERROR_MESSAGE="Missing required input artifact: $target_file"
        echo "FAIL: $ERROR_MESSAGE" >&2
        exit 30
    fi
    if [ ! -s "$target_file" ]; then
        ERROR_MESSAGE="Required input artifact is 0 bytes: $target_file"
        echo "FAIL: $ERROR_MESSAGE" >&2
        exit 30
    fi
done

ISOLATED_DIR="$WORK_DIR/baseline"
mkdir -p "$ISOLATED_DIR"
for artifact in "${REQUIRED_ARTIFACTS[@]}"; do
    cp "$EVIDENCE_SRC/$artifact" "$ISOLATED_DIR/$artifact"
done

# Timestamp normalization
touch -r "$ISOLATED_DIR/stage2.bin" "$ISOLATED_DIR/stage3.bin"
touch -d "+1 second" "$ISOLATED_DIR/stage3.bin"
touch -r "$ISOLATED_DIR/stage2.bin.norm" "$ISOLATED_DIR/stage3.bin.norm"
touch -d "+1 second" "$ISOLATED_DIR/stage3.bin.norm"

# Stage 1: Individual Artifact Digests & Pairwise Equality
CURRENT_STAGE="DIGEST_EQUALITY"
echo "=== STEP 1: INDIVIDUAL ARTIFACT DIGEST & PAIRWISE EQUALITY ==="
S2_SHA=$(sha256sum "$ISOLATED_DIR/stage2.s" | awk '{print $1}')
S3_SHA=$(sha256sum "$ISOLATED_DIR/stage3.s" | awk '{print $1}')
O2_SHA=$(sha256sum "$ISOLATED_DIR/stage2.o" | awk '{print $1}')
O3_SHA=$(sha256sum "$ISOLATED_DIR/stage3.o" | awk '{print $1}')
B2_SHA=$(sha256sum "$ISOLATED_DIR/stage2.bin.norm" | awk '{print $1}')
B3_SHA=$(sha256sum "$ISOLATED_DIR/stage3.bin.norm" | awk '{print $1}')

echo "stage2.s:        $S2_SHA"
echo "stage3.s:        $S3_SHA"
if [ "$S2_SHA" != "$S3_SHA" ]; then
    ERROR_MESSAGE="Assembly digests diverged ($S2_SHA != $S3_SHA)"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 20
fi
echo "  -> Assembly digests: BYTE-IDENTICAL"

echo "stage2.o:        $O2_SHA"
echo "stage3.o:        $O3_SHA"
if [ "$O2_SHA" != "$O3_SHA" ]; then
    ERROR_MESSAGE="Object digests diverged ($O2_SHA != $O3_SHA)"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 21
fi
echo "  -> Object digests:   BYTE-IDENTICAL"

echo "stage2.bin.norm: $B2_SHA"
echo "stage3.bin.norm: $B3_SHA"
if [ "$B2_SHA" != "$B3_SHA" ]; then
    ERROR_MESSAGE="Binary norm digests diverged ($B2_SHA != $B3_SHA)"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 22
fi
echo "  -> Binary digests:   BYTE-IDENTICAL"

# Stage 2: Positive Controls
CURRENT_STAGE="POSITIVE_CONTROLS"
echo ""
echo "=== STEP 2: POSITIVE CONTROLS (GENUINE ARTIFACTS) ==="
cmp -s "$ISOLATED_DIR/stage2.s" "$ISOLATED_DIR/stage3.s" || { ERROR_MESSAGE="Genuine stage2.s and stage3.s not identical"; exit 20; }
cmp -s "$ISOLATED_DIR/stage2.o" "$ISOLATED_DIR/stage3.o" || { ERROR_MESSAGE="Genuine stage2.o and stage3.o not identical"; exit 21; }
cmp -s "$ISOLATED_DIR/stage2.bin.norm" "$ISOLATED_DIR/stage3.bin.norm" || { ERROR_MESSAGE="Genuine stage2.bin.norm and stage3.bin.norm not identical"; exit 22; }
echo "PASS: Genuine stage2 and stage3 (.s, .o, .bin.norm) are byte-identical (cmp exit code 0)"

# Stage 3: Strict Comparator Sensitivity Tests (status 1 required)
CURRENT_STAGE="COMPARATOR_SENSITIVITY"
echo ""
echo "=== STEP 3: COMPARATOR SENSITIVITY TEST (STRICT STATUS 1 ASSERTION) ==="
cp "$ISOLATED_DIR/stage3.s" "$WORK_DIR/stage3.mutated.s"
printf '\n# ZCC_NEGATIVE_CONTROL\n' >> "$WORK_DIR/stage3.mutated.s"

set +e
cmp -s "$ISOLATED_DIR/stage2.s" "$WORK_DIR/stage3.mutated.s"
CMP_ASM_RC=$?
set -e

if [ "$CMP_ASM_RC" -ne 1 ]; then
    ERROR_MESSAGE="Expected cmp exit code 1 (divergence), got $CMP_ASM_RC"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
echo "PASS: Assembly comparator sensitivity verified (cmp exit code strictly 1)"

cp "$ISOLATED_DIR/stage3.bin.norm" "$WORK_DIR/stage3.mutated.bin.norm"
python3 - <<PY
from pathlib import Path
p = Path("$WORK_DIR/stage3.mutated.bin.norm")
data = bytearray(p.read_bytes())
assert len(data) > 256, "binary too small"
orig = data[256]
data[256] ^= 1
mut = data[256]
p.write_bytes(data)
print(f"  [XOR Mutation] Offset 256: 0x{orig:02x} -> 0x{mut:02x}")
PY

set +e
cmp -s "$ISOLATED_DIR/stage3.bin.norm" "$WORK_DIR/stage3.mutated.bin.norm"
CMP_BIN_RC=$?
set -e

if [ "$CMP_BIN_RC" -ne 1 ]; then
    ERROR_MESSAGE="Expected binary cmp exit code 1 (divergence), got $CMP_BIN_RC"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
echo "PASS: Binary comparator sensitivity verified (cmp exit code strictly 1)"

# Stage 4: Production Gate Assembly Mutation Rejection (Exit 20)
CURRENT_STAGE="GATE_ASSEMBLY_MUTATION"
echo ""
echo "=== STEP 4: REAL PRODUCTION GATE.SH FAILURE BRANCH (ASSEMBLY EXIT 20) ==="
MUTATED_DIR_ASM="$WORK_DIR/gate_fail_asm"
mkdir -p "$MUTATED_DIR_ASM"
cp -a "$ISOLATED_DIR"/. "$MUTATED_DIR_ASM"/
mv "$WORK_DIR/stage3.mutated.s" "$MUTATED_DIR_ASM/stage3.s"

set +e
bash "$GATE_SCRIPT" --verify-dir "$MUTATED_DIR_ASM" > "$WORK_DIR/gate_fail_asm.stdout" 2> "$WORK_DIR/gate_fail_asm.stderr"
GATE_ASM_RC=$?
set -e
DIAG_ASM=$(cat "$WORK_DIR/gate_fail_asm.stderr")

echo "gate.sh returned exit code: $GATE_ASM_RC"
echo "gate.sh stderr: $DIAG_ASM"

if [ "$GATE_ASM_RC" -ne 20 ]; then
    ERROR_MESSAGE="gate.sh returned exit code $GATE_ASM_RC (expected 20)"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
if ! grep -q "FAIL: Assembly files diverged." "$WORK_DIR/gate_fail_asm.stderr"; then
    ERROR_MESSAGE="Diagnostic mismatch: expected 'FAIL: Assembly files diverged.', got: $DIAG_ASM"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
echo "PASS: gate.sh successfully rejected mutated assembly with EXIT_ASM_DIVERG code 20 and exact diagnostic"

# Stage 5: Production Gate Object Mutation Rejection (Exit 21)
CURRENT_STAGE="GATE_OBJECT_MUTATION"
echo ""
echo "=== STEP 5: REAL PRODUCTION GATE.SH FAILURE BRANCH (OBJECT EXIT 21) ==="
MUTATED_DIR_OBJ="$WORK_DIR/gate_fail_obj"
mkdir -p "$MUTATED_DIR_OBJ"
cp -a "$ISOLATED_DIR"/. "$MUTATED_DIR_OBJ"/

OBJ_ORIG_BYTE=$(python3 - <<PY
from pathlib import Path
p = Path("$MUTATED_DIR_OBJ/stage3.o")
data = bytearray(p.read_bytes())
assert len(data) > 128, "object file too small"
orig = data[128]
data[128] ^= 1
mut = data[128]
p.write_bytes(data)
print(f"0x{orig:02x}->0x{mut:02x}")
PY
)
echo "  [XOR Mutation] Object offset 128: $OBJ_ORIG_BYTE"

set +e
bash "$GATE_SCRIPT" --verify-dir "$MUTATED_DIR_OBJ" > "$WORK_DIR/gate_fail_obj.stdout" 2> "$WORK_DIR/gate_fail_obj.stderr"
GATE_OBJ_RC=$?
set -e
DIAG_OBJ=$(cat "$WORK_DIR/gate_fail_obj.stderr")

echo "gate.sh returned exit code: $GATE_OBJ_RC"
echo "gate.sh stderr: $DIAG_OBJ"

if [ "$GATE_OBJ_RC" -ne 21 ]; then
    ERROR_MESSAGE="gate.sh returned exit code $GATE_OBJ_RC (expected 21)"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
if ! grep -q "FAIL: Object files diverged." "$WORK_DIR/gate_fail_obj.stderr"; then
    ERROR_MESSAGE="Diagnostic mismatch: expected 'FAIL: Object files diverged.', got: $DIAG_OBJ"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
echo "PASS: gate.sh successfully rejected mutated object with EXIT_OBJ_DIVERG code 21 and exact diagnostic"

# Stage 6: Production Gate Binary Mutation Rejection (Exit 22)
CURRENT_STAGE="GATE_BINARY_MUTATION"
echo ""
echo "=== STEP 6: REAL PRODUCTION GATE.SH FAILURE BRANCH (BINARY EXIT 22) ==="
MUTATED_DIR_BIN="$WORK_DIR/gate_fail_bin"
mkdir -p "$MUTATED_DIR_BIN"
cp -a "$ISOLATED_DIR"/. "$MUTATED_DIR_BIN"/

BIN_ORIG_BYTE=$(python3 - <<PY
from pathlib import Path
p = Path("$MUTATED_DIR_BIN/stage3.bin")
data = bytearray(p.read_bytes())
assert len(data) > 256, "binary too small"
orig = data[256]
data[256] ^= 1
mut = data[256]
p.write_bytes(data)
print(f"0x{orig:02x}->0x{mut:02x}")
PY
)
echo "  [XOR Mutation] Binary offset 256: $BIN_ORIG_BYTE"
touch -r "$MUTATED_DIR_BIN/stage2.bin" "$MUTATED_DIR_BIN/stage3.bin"
touch -d "+1 second" "$MUTATED_DIR_BIN/stage3.bin"

set +e
bash "$GATE_SCRIPT" --verify-dir "$MUTATED_DIR_BIN" > "$WORK_DIR/gate_fail_bin.stdout" 2> "$WORK_DIR/gate_fail_bin.stderr"
GATE_BIN_RC=$?
set -e
DIAG_BIN=$(cat "$WORK_DIR/gate_fail_bin.stderr")

echo "gate.sh returned exit code: $GATE_BIN_RC"
echo "gate.sh stderr: $DIAG_BIN"

if [ "$GATE_BIN_RC" -ne 22 ]; then
    ERROR_MESSAGE="gate.sh returned exit code $GATE_BIN_RC (expected 22)"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
if ! grep -q "FAIL: Binary files diverged post-normalization." "$WORK_DIR/gate_fail_bin.stderr"; then
    ERROR_MESSAGE="Diagnostic mismatch: expected 'FAIL: Binary files diverged post-normalization.', got: $DIAG_BIN"
    echo "FAIL: $ERROR_MESSAGE" >&2
    exit 1
fi
echo "PASS: gate.sh successfully rejected mutated binary with EXIT_BIN_DIVERG code 22 and exact diagnostic"

# Final Success Transition
CURRENT_STAGE="COMPLETE"
FINAL_VERDICT="PASS"
EXIT_CODE=0
echo ""
echo "================================================================================"
echo "   ✨ ALL NEGATIVE CONTROL GATES METICULOUSLY VERIFIED (EXIT 20, 21, 22)"
echo "================================================================================"
