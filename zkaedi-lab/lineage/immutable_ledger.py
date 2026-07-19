import json
import hashlib
import time
from typing import Optional, Any
from dataclasses import dataclass, asdict
from online_types import append_jsonl_durable, canonical_json_bytes

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

@dataclass(frozen=True)
class LedgerEnvelope:
    sequence: int
    previous_hash: Optional[str]
    payload: dict[str, Any]
    payload_hash: str
    entry_hash: str
    recorded_at_unix: float

@dataclass(frozen=True)
class ArtifactNode:
    artifact_id: str
    artifact_type: str  # e.g., "checkpoint", "replay_buffer", "validation_report", "receipt"
    parents: list[str]
    created_at: float
    metadata_hash: str

def compute_experiment_id(manifest: dict[str, Any]) -> str:
    # Deterministic content hash of experiment manifest parameters
    manifest_bytes = canonical_json_bytes(manifest)
    return "sha256:" + hashlib.sha256(manifest_bytes).hexdigest()

def build_ledger_envelope(
    sequence: int,
    previous_hash: Optional[str],
    payload: dict[str, Any]
) -> LedgerEnvelope:
    payload_hash = "sha256:" + hashlib.sha256(canonical_json_bytes(payload)).hexdigest()
    
    # Compute chaining hash: previous_hash || sequence || payload_hash
    prev_str = previous_hash if previous_hash else "genesis"
    chain_basis = f"{prev_str}|{sequence}|{payload_hash}"
    entry_hash = "sha256:" + hashlib.sha256(chain_basis.encode()).hexdigest()
    
    return LedgerEnvelope(
        sequence=sequence,
        previous_hash=previous_hash,
        payload=payload,
        payload_hash=payload_hash,
        entry_hash=entry_hash,
        recorded_at_unix=time.time()
    )

def record_ledger_envelope(path: str, envelope: LedgerEnvelope) -> None:
    payload = asdict(envelope)
    append_jsonl_durable(path, payload)

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
        "policy_kl": policy_kl
    }
    cert["content_hash"] = "sha256:" + hashlib.sha256(canonical_json_bytes(cert)).hexdigest()
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
        "reason": reason
    }
    cert["content_hash"] = "sha256:" + hashlib.sha256(canonical_json_bytes(cert)).hexdigest()
    return cert
