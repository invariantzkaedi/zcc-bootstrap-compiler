import json
import hashlib
import time
from typing import Optional, Any
from dataclasses import dataclass, asdict
from online_types import append_jsonl_durable

@dataclass(frozen=True)
class TrainingReceipt:
    cycle_id: str
    parent_checkpoint: str
    candidate_checkpoint: str
    replay_buffer_hash: str
    replay_records: int
    trainer_version: str
    tokenizer_version: str
    sandbox_version: str
    validation_summary_hash: str
    policy_kl: float
    heldout_delta: float
    promoted: bool
    rollback_target: Optional[str]
    created_at: float

@dataclass(frozen=True)
class ArtifactNode:
    artifact_id: str
    artifact_type: str  # e.g., "checkpoint", "replay_buffer", "validation_report", "receipt"
    parents: list[str]
    created_at: float
    metadata_hash: str

def compute_experiment_id(
    replay_buffer_hash: str,
    parent_checkpoint: str,
    trainer_version: str,
    sandbox_version: str,
    validation_hash: str,
) -> str:
    basis = f"{replay_buffer_hash}|{parent_checkpoint}|{trainer_version}|{sandbox_version}|{validation_hash}"
    return "sha256:" + hashlib.sha256(basis.encode()).hexdigest()

def record_training_receipt(path: str, receipt: TrainingReceipt) -> str:
    payload = asdict(receipt)
    append_jsonl_durable(path, payload)
    
    # Return hash-address of the receipt
    receipt_bytes = json.dumps(payload, sort_keys=True).encode()
    return "sha256:" + hashlib.sha256(receipt_bytes).hexdigest()

def generate_promotion_certificate(
    checkpoint: str,
    validation_hash: str,
    replay_hash: str,
    policy_kl: float,
) -> dict[str, Any]:
    cert = {
        "promotion": True,
        "checkpoint": checkpoint,
        "validation_hash": validation_hash,
        "replay_hash": replay_hash,
        "policy_kl": policy_kl,
        "timestamp_unix": time.time()
    }
    cert_bytes = json.dumps(cert, sort_keys=True).encode()
    cert["signature"] = "sha256:" + hashlib.sha256(cert_bytes).hexdigest()
    return cert

def generate_rollback_certificate(
    failed_checkpoint: str,
    restore_checkpoint: str,
    reason: str,
) -> dict[str, Any]:
    cert = {
        "rollback": True,
        "failed_checkpoint": failed_checkpoint,
        "restore_checkpoint": restore_checkpoint,
        "reason": reason,
        "timestamp_unix": time.time()
    }
    cert_bytes = json.dumps(cert, sort_keys=True).encode()
    cert["signature"] = "sha256:" + hashlib.sha256(cert_bytes).hexdigest()
    return cert
