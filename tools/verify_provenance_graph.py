import os
import sys
import json
import hashlib
from pathlib import Path
import re

# Add repo root to path to import local modules
sys.path.append(str(Path(__file__).resolve().parent.parent))

from zkaedi_model_registry import verify_registry_signature, get_model_hashes, get_file_sha256

def hash_file(path: Path) -> str:
    return get_file_sha256(path)

def validate_node_schema(node: dict) -> list[str]:
    errors = []
    required = ["node_id", "node_type", "created_at", "git_commit", "payload_sha256", "parents", "producer"]
    for field in required:
        if field not in node:
            errors.append(f"Missing required field: '{field}'")
            
    if errors:
        return errors
        
    # Pattern matching
    if not re.match(r"^sha256:[a-f0-9]{64}$", node["node_id"]):
        errors.append(f"Invalid node_id format: '{node['node_id']}'")
    if not re.match(r"^[a-f0-9]{64}$", node["payload_sha256"]):
        errors.append(f"Invalid payload_sha256 format: '{node['payload_sha256']}'")
        
    valid_types = ["dataset", "split", "config", "checkpoint", "evaluation", "attestation", "receipt", "registry"]
    if node["node_type"] not in valid_types:
        errors.append(f"Invalid node_type: '{node['node_type']}'")
        
    if not isinstance(node["parents"], list):
        errors.append("Field 'parents' must be a list")
    else:
        for p in node["parents"]:
            if not re.match(r"^sha256:[a-f0-9]{64}$", p):
                errors.append(f"Invalid parent ID format: '{p}'")
                
    producer = node["producer"]
    if not isinstance(producer, dict):
        errors.append("Field 'producer' must be a dictionary")
    else:
        p_required = ["script", "script_sha256", "validator_version"]
        for pf in p_required:
            if pf not in producer:
                errors.append(f"Missing producer field: '{pf}'")
        if not re.match(r"^[a-f0-9]{64}$", producer.get("script_sha256", "")):
            errors.append("Invalid producer script_sha256 format")
            
    return errors

def main():
    if len(sys.argv) < 4:
        print("Usage: python3 verify_provenance_graph.py <run_dir> <provenance_graph_json> <public_key_path>")
        sys.exit(1)
        
    run_dir = Path(sys.argv[1]).resolve()
    graph_path = Path(sys.argv[2]).resolve()
    public_key = Path(sys.argv[3]).resolve()
    
    if not graph_path.exists():
        print(f"FAIL: Provenance graph file {graph_path} not found")
        sys.exit(1)
        
    with open(graph_path, "r", encoding="utf-8") as f:
        graph = json.load(f)
        
    nodes = graph.get("nodes", [])
    
    print(f"[ZKAEDI PROVENANCE] Loaded graph from {graph_path} containing {len(nodes)} nodes.")
    
    # 1. Schema and content verification
    for node in nodes:
        node_id = node.get("node_id", "unknown")
        errors = validate_node_schema(node)
        if errors:
            print(f"FAIL: Schema validation failed for node {node_id}: {errors}")
            sys.exit(1)
            
        ntype = node["node_type"]
        expected_hash = node["payload_sha256"]
        
        if ntype == "dataset":
            target_path = run_dir / "inputs" / "dataset.parquet"
            actual_hash = hash_file(target_path)
        elif ntype == "split":
            target_path = run_dir / "inputs" / "split_manifest.json"
            actual_hash = hash_file(target_path)
        elif ntype == "config":
            target_path = run_dir / "inputs" / "training_config.json"
            actual_hash = hash_file(target_path)
        elif ntype == "checkpoint":
            ckpts = list((run_dir / "checkpoints").glob("checkpoint-*"))
            if not ckpts:
                print(f"FAIL: No checkpoints found in {run_dir}/checkpoints")
                sys.exit(1)
            target_path = ckpts[0]
            actual_hash, _ = get_model_hashes(target_path)
        elif ntype == "evaluation":
            target_path = run_dir / "evidence" / "evaluation_metrics.json"
            actual_hash = hash_file(target_path)
        elif ntype == "attestation":
            target_path = run_dir / "evidence" / "dpo_security_attestation.json"
            actual_hash = hash_file(target_path)
            
            sig_path = target_path.with_suffix(target_path.suffix + ".sig")
            if not sig_path.exists():
                print(f"FAIL: Missing signature file for attestation: {sig_path}")
                sys.exit(1)
            with open(target_path, "r") as tf:
                att_data = json.load(tf)
            with open(sig_path, "rb") as sf:
                sig_bytes = sf.read()
            if not verify_registry_signature(att_data, sig_bytes, str(public_key)):
                print(f"FAIL: Attestation signature verification failed using {public_key}")
                sys.exit(1)
        elif ntype == "receipt":
            target_path = run_dir / "evidence" / "release_receipt.json"
            actual_hash = hash_file(target_path)
            
            sig_path = target_path.with_suffix(target_path.suffix + ".sig")
            if not sig_path.exists():
                print(f"FAIL: Missing signature file for receipt: {sig_path}")
                sys.exit(1)
            with open(target_path, "r") as tf:
                rec_data = json.load(tf)
            with open(sig_path, "rb") as sf:
                sig_bytes = sf.read()
            if not verify_registry_signature(rec_data, sig_bytes, str(public_key)):
                print(f"FAIL: Release receipt signature verification failed using {public_key}")
                sys.exit(1)
        elif ntype == "registry":
            target_path = run_dir / "evidence" / "best_statistically_valid_checkpoint.json"
            actual_hash = hash_file(target_path)
            
            sig_path = target_path.with_suffix(target_path.suffix + ".sig")
            if not sig_path.exists():
                print(f"FAIL: Missing signature file for best checkpoint metadata: {sig_path}")
                sys.exit(1)
            with open(target_path, "r") as tf:
                reg_data = json.load(tf)
            with open(sig_path, "rb") as sf:
                sig_bytes = sf.read()
            if not verify_registry_signature(reg_data, sig_bytes, str(public_key)):
                print(f"FAIL: Best checkpoint registry signature verification failed using {public_key}")
                sys.exit(1)
        else:
            print(f"FAIL: Unknown node type: {ntype}")
            sys.exit(1)
            
        if actual_hash != expected_hash:
            print(f"FAIL: payload_sha256 mismatch for type '{ntype}'. Expected: {expected_hash}, Actual: {actual_hash}")
            sys.exit(1)
            
        print(f"  [PASS] Node {ntype}: hash {actual_hash[:8]} matches physical file.")
        
    # 2. Graph Continuity verification
    by_type = {}
    for n in nodes:
        by_type.setdefault(n["node_type"], []).append(n)
        
    for mandatory in ("dataset", "split", "config", "checkpoint", "evaluation", "attestation", "receipt", "registry"):
        if mandatory not in by_type:
            print(f"FAIL: Missing node of type {mandatory} in graph")
            sys.exit(1)
            
    receipt_node = by_type["receipt"][0]
    attestation_node = by_type["attestation"][0]
    if f"sha256:{attestation_node['payload_sha256']}" not in receipt_node["parents"]:
        print("FAIL: Release receipt node parent is not the attestation node")
        sys.exit(1)
        
    checkpoint_node = by_type["checkpoint"][0]
    evaluation_node = by_type["evaluation"][0]
    if f"sha256:{checkpoint_node['payload_sha256']}" not in attestation_node["parents"]:
        print("FAIL: Attestation parent is not the checkpoint node")
        sys.exit(1)
    if f"sha256:{evaluation_node['payload_sha256']}" not in attestation_node["parents"]:
        print("FAIL: Attestation parent is not the evaluation node")
        sys.exit(1)
        
    config_node = by_type["config"][0]
    split_node = by_type["split"][0]
    dataset_node = by_type["dataset"][0]
    for n in (config_node, split_node, dataset_node):
        if f"sha256:{n['payload_sha256']}" not in checkpoint_node["parents"]:
            print(f"FAIL: Checkpoint node parent is missing: {n['node_type']}")
            sys.exit(1)
            
    registry_node = by_type["registry"][0]
    if f"sha256:{receipt_node['payload_sha256']}" not in registry_node["parents"]:
        print("FAIL: Registry node parent is not the receipt node")
        sys.exit(1)
        
    print("[ZKAEDI PROVENANCE] Graph verification complete. Unbroken lineage verified successfully.")
    print("STATUS: VERIFIED")

if __name__ == "__main__":
    main()
