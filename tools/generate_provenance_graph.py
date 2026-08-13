import os
import sys
import json
from pathlib import Path
from datetime import datetime, timezone

sys.path.append(str(Path(__file__).resolve().parent.parent))
from zkaedi_model_registry import get_model_hashes, get_file_sha256

def hash_file(path: Path) -> str:
    return get_file_sha256(path)

def get_script_sha256(script_name: str) -> str:
    script_path = Path(__file__).resolve().parent.parent / script_name
    if not script_path.exists():
        script_path = Path(script_name)
    return hash_file(script_path) if script_path.exists() else "0"*64

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 generate_provenance_graph.py <run_dir> <git_commit> <validator_version>")
        sys.exit(1)
        
    run_dir = Path(sys.argv[1]).resolve()
    git_commit = sys.argv[2]
    validator_version = sys.argv[3]
    
    timestamp = datetime.now(timezone.utc).isoformat()
    
    # 1. Recompute digests
    dataset_hash = hash_file(run_dir / "inputs" / "dataset.parquet")
    split_hash = hash_file(run_dir / "inputs" / "split_manifest.json")
    config_hash = hash_file(run_dir / "inputs" / "training_config.json")
    
    ckpts = list((run_dir / "checkpoints").glob("checkpoint-*"))
    if not ckpts:
        print(f"Error: No checkpoints folder in {run_dir}/checkpoints")
        sys.exit(1)
    checkpoint_path = ckpts[0]
    checkpoint_hash, _ = get_model_hashes(checkpoint_path)
    
    eval_hash = hash_file(run_dir / "evidence" / "evaluation_metrics.json")
    attestation_hash = hash_file(run_dir / "evidence" / "dpo_security_attestation.json")
    receipt_hash = hash_file(run_dir / "evidence" / "release_receipt.json")
    registry_hash = hash_file(run_dir / "evidence" / "best_statistically_valid_checkpoint.json")
    
    # Script SHA256s
    gen_script_sha = get_script_sha256("tests/generate_mock_dpo_data.py")
    train_script_sha = get_script_sha256("train_hf_dpo_adamw_hardened_v3.py")
    run_script_sha = get_script_sha256("tests/run_e2e_dpo_smoke.sh")
    
    nodes = [
        {
            "node_id": f"sha256:{dataset_hash}",
            "node_type": "dataset",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": dataset_hash,
            "parents": [],
            "producer": {
                "script": "tests/generate_mock_dpo_data.py",
                "script_sha256": gen_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{split_hash}",
            "node_type": "split",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": split_hash,
            "parents": [f"sha256:{dataset_hash}"],
            "producer": {
                "script": "tests/generate_mock_dpo_data.py",
                "script_sha256": gen_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{config_hash}",
            "node_type": "config",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": config_hash,
            "parents": [],
            "producer": {
                "script": "tests/run_e2e_dpo_smoke.sh",
                "script_sha256": run_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{checkpoint_hash}",
            "node_type": "checkpoint",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": checkpoint_hash,
            "parents": [
                f"sha256:{dataset_hash}",
                f"sha256:{split_hash}",
                f"sha256:{config_hash}"
            ],
            "producer": {
                "script": "train_hf_dpo_adamw_hardened_v3.py",
                "script_sha256": train_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{eval_hash}",
            "node_type": "evaluation",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": eval_hash,
            "parents": [f"sha256:{checkpoint_hash}"],
            "producer": {
                "script": "train_hf_dpo_adamw_hardened_v3.py",
                "script_sha256": train_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{attestation_hash}",
            "node_type": "attestation",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": attestation_hash,
            "parents": [
                f"sha256:{checkpoint_hash}",
                f"sha256:{eval_hash}"
            ],
            "producer": {
                "script": "train_hf_dpo_adamw_hardened_v3.py",
                "script_sha256": train_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{receipt_hash}",
            "node_type": "receipt",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": receipt_hash,
            "parents": [f"sha256:{attestation_hash}"],
            "producer": {
                "script": "train_hf_dpo_adamw_hardened_v3.py",
                "script_sha256": train_script_sha,
                "validator_version": validator_version
            }
        },
        {
            "node_id": f"sha256:{registry_hash}",
            "node_type": "registry",
            "created_at": timestamp,
            "git_commit": git_commit,
            "payload_sha256": registry_hash,
            "parents": [f"sha256:{receipt_hash}"],
            "producer": {
                "script": "train_hf_dpo_adamw_hardened_v3.py",
                "script_sha256": train_script_sha,
                "validator_version": validator_version
            }
        }
    ]
    
    graph_data = {
        "run_id": run_dir.name,
        "nodes": nodes
    }
    
    out_path = run_dir / "evidence" / "provenance_graph.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(graph_data, f, indent=2)
        
    print(f"[ZKAEDI PROVENANCE] Generated provenance graph at {out_path}")

if __name__ == "__main__":
    main()
