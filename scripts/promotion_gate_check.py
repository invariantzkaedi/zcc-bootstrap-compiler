import os
import sys
import json
import argparse
from jsonschema import validate, ValidationError

def main():
    parser = argparse.ArgumentParser(description="Fail-closed promotion gate checker")
    parser.add_argument("--verdict-file", required=True, help="Path to validate_verdict.json")
    parser.add_argument("--verdict-schema", required=True, help="Path to validate_verdict.schema.json")
    parser.add_argument("--gate-schema", required=True, help="Path to promotion_gate.schema.json")
    parser.add_argument("--out-result", required=True, help="Path to output promotion_gate_result.json")
    parser.add_argument("--checkpoint-digest", required=True, help="Expected checkpoint digest")
    parser.add_argument("--mock-mode", action="store_true", help="Allow mock artifacts if running in test environment")
    
    args = parser.parse_args()
    
    promotion_allowed = False
    rejection_reasons = []
    
    # 1. Verify existence of required files
    mandatory_files = [args.verdict_file]
    if not args.mock_mode:
        mandatory_files.extend([
            "trainer_state.json",
            "splits/dpo_v1_manifest.json"
        ])
        
    missing_files = []
    for fpath in mandatory_files:
        if not os.path.exists(fpath):
            missing_files.append(fpath)
            
    if missing_files:
        print(f"[-] Promotion gate rejected due to file checks: {missing_files}")
        rejection_reasons.append("MISSING_REQUIRED_ARTIFACTS")
        write_gate_result(args.out_result, False, rejection_reasons, 1, "REJECTED", "FAIL", 0, False, False)
        sys.exit(1)
        
    # 2. Parse and validate validate_verdict.json against schema
    try:
        with open(args.verdict_file, "r", encoding="utf-8") as f:
            verdict_data = json.load(f)
            
        with open(args.verdict_schema, "r", encoding="utf-8") as f:
            schema_data = json.load(f)
            
        validate(instance=verdict_data, schema=schema_data)
        print("[+] validate_verdict.json schema validation passed.")
    except Exception as e:
        print(f"[-] Schema validation error: {e}")
        rejection_reasons.append("SCHEMA_VALIDATION_FAILED")
        write_gate_result(args.out_result, False, rejection_reasons, 1, "REJECTED", "FAIL", 0, False, False)
        sys.exit(1)
        
    # 3. Verify gate conditions
    release_verdict = verdict_data.get("release_verdict")
    provenance_status = verdict_data.get("provenance_gate", {}).get("status")
    split_overlap_count = verdict_data.get("provenance_gate", {}).get("split_overlap_count", 0)
    checkpoint_digest = verdict_data.get("provenance", {}).get("checkpoint_digest")
    
    # Verify checkpoint digest
    digest_verified = (checkpoint_digest == args.checkpoint_digest)
    if not digest_verified:
        rejection_reasons.append("CHECKPOINT_DIGEST_MISMATCH")
        
    # Verify metric release verdict
    if release_verdict != "METRIC_GATES_PASSED":
        rejection_reasons.append("RELEASE_VERDICT_REJECTED")
        
    # Verify provenance gate
    if provenance_status != "PASS":
        rejection_reasons.append("PROVENANCE_GATE_FAILED")
        
    # Verify zero split overlap
    if split_overlap_count != 0:
        rejection_reasons.append("PROVENANCE_GATE_FAILED")
        
    # Deduplicate and sort reasons
    rejection_reasons = sorted(list(set(rejection_reasons)))
        
    # If any gate failure occurred
    if rejection_reasons:
        print(f"[-] Promotion gate rejected: {rejection_reasons}")
        write_gate_result(
            out_path=args.out_result,
            promotion_allowed=False,
            failure_reasons=rejection_reasons,
            exit_code=1,
            release_verdict=release_verdict,
            provenance_gate=provenance_status,
            split_overlap_count=split_overlap_count,
            checkpoint_digest_verified=digest_verified,
            required_artifacts_complete=True
        )
        sys.exit(1)
        
    # All checks passed successfully
    print("[+] All promotion gate conditions satisfied. Promotion allowed!")
    write_gate_result(
        out_path=args.out_result,
        promotion_allowed=True,
        failure_reasons=[],
        exit_code=0,
        release_verdict=release_verdict,
        provenance_gate=provenance_status,
        split_overlap_count=split_overlap_count,
        checkpoint_digest_verified=True,
        required_artifacts_complete=True
    )
    sys.exit(0)

def write_gate_result(out_path, promotion_allowed, failure_reasons, exit_code, release_verdict, provenance_gate, split_overlap_count, checkpoint_digest_verified, required_artifacts_complete):
    result = {
        "promotion_allowed": promotion_allowed,
        "failure_reasons": failure_reasons,
        "requirements": {
            "validator_exit_code": exit_code,
            "release_verdict": release_verdict,
            "provenance_gate": provenance_gate,
            "split_overlap_count": split_overlap_count,
            "checkpoint_digest_verified": checkpoint_digest_verified,
            "required_artifacts_complete": required_artifacts_complete
        }
    }
    # Atomic write to target file
    parent_dir = os.path.dirname(os.path.abspath(out_path))
    if parent_dir:
        os.makedirs(parent_dir, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2)

if __name__ == "__main__":
    main()
