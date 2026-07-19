import json
import hashlib
import time
import os
import copy
import secrets
from typing import Optional, Any, Mapping
from dataclasses import dataclass, asdict
from online_types import (
    append_jsonl_durable,
    canonical_json_bytes,
    lock_file_ex,
    lock_file_sh,
    unlock_file
)

@dataclass(frozen=True, slots=True)
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

@dataclass(frozen=True, slots=True)
class LedgerEnvelope:
    sequence: int
    previous_hash: Optional[str]
    payload: Mapping[str, Any]
    payload_hash: str
    entry_hash: str
    recorded_at_unix: float

@dataclass(frozen=True, slots=True)
class ArtifactNode:
    artifact_id: str
    artifact_type: str  # e.g., "checkpoint", "replay_buffer", "validation_report", "receipt"
    parents: tuple[str, ...]
    created_at: float
    metadata_hash: str

@dataclass(frozen=True, slots=True)
class LedgerVerification:
    valid: bool
    records_verified: int
    head_hash: Optional[str]
    failures: tuple[str, ...]

def content_hash(domain: str, value: Any) -> str:
    material = {
        "domain": domain,
        "schema_version": 1,
        "value": value,
    }
    return "sha256:" + hashlib.sha256(canonical_json_bytes(material)).hexdigest()

def compute_experiment_id(manifest: dict[str, Any]) -> str:
    return content_hash("zkaedi.experiment-manifest", manifest)

def build_ledger_envelope(
    sequence: int,
    previous_hash: Optional[str],
    payload: dict[str, Any],
    recorded_at_unix: float
) -> LedgerEnvelope:
    payload_copy = copy.deepcopy(payload)
    payload_hash = content_hash("zkaedi.ledger-payload", payload_copy)
    
    header = {
        "sequence": sequence,
        "previous_hash": previous_hash,
        "payload_hash": payload_hash,
        "recorded_at_unix": recorded_at_unix,
    }
    entry_hash = content_hash("zkaedi.ledger-header", header)
    
    return LedgerEnvelope(
        sequence=sequence,
        previous_hash=previous_hash,
        payload=payload_copy,
        payload_hash=payload_hash,
        entry_hash=entry_hash,
        recorded_at_unix=recorded_at_unix
    )

def verify_records_sequence(records: list[dict]) -> tuple[bool, list[str]]:
    failures = []
    
    for idx, rec in enumerate(records):
        seq = rec.get("sequence")
        if seq != idx:
            failures.append(f"Record index {idx} has invalid sequence: {seq}")
            
        # Verify payload hash
        payload = rec.get("payload")
        payload_hash = rec.get("payload_hash")
        expected_p_hash = content_hash("zkaedi.ledger-payload", payload)
        if not secrets.compare_digest(payload_hash, expected_p_hash):
            failures.append(f"Record sequence {seq} payload hash mismatch")
            
        # Verify header/entry hash
        recorded_at = rec.get("recorded_at_unix")
        prev_hash = rec.get("previous_hash")
        entry_hash = rec.get("entry_hash")
        
        header = {
            "sequence": seq,
            "previous_hash": prev_hash,
            "payload_hash": payload_hash,
            "recorded_at_unix": recorded_at,
        }
        expected_entry_hash = content_hash("zkaedi.ledger-header", header)
        if not secrets.compare_digest(entry_hash, expected_entry_hash):
            failures.append(f"Record sequence {seq} entry hash mismatch")
            
        # Verify chain linking
        if idx == 0:
            if prev_hash is not None:
                failures.append("Genesis record previous_hash must be None")
        else:
            prev_rec = records[idx - 1]
            prev_entry_hash = prev_rec.get("entry_hash")
            if not secrets.compare_digest(prev_hash, prev_entry_hash):
                failures.append(f"Record sequence {seq} points to invalid parent hash: {prev_hash}")
                
    return len(failures) == 0, failures

def verify_ledger(path: str) -> LedgerVerification:
    abspath = os.path.abspath(path)
    if not os.path.exists(abspath):
        return LedgerVerification(valid=True, records_verified=0, head_hash=None, failures=())

    with open(abspath, "rb") as fh:
        lock_file_sh(fh)
        try:
            content_bytes = fh.read()
        finally:
            unlock_file(fh)

    lines = content_bytes.decode("utf-8").splitlines()
    records = []
    skipped_torn = False
    
    for index, line in enumerate(lines):
        line_str = line.strip()
        if not line_str:
            continue
        try:
            records.append(json.loads(line_str))
        except json.JSONDecodeError:
            # Tolerable only at final line if the last write was interrupted
            if index == len(lines) - 1:
                skipped_torn = True
                continue
            raise

    if not records:
        return LedgerVerification(valid=True, records_verified=0, head_hash=None, failures=())

    valid, failures = verify_records_sequence(records)
    head_hash = records[-1].get("entry_hash") if valid else None
    
    return LedgerVerification(
        valid=valid,
        records_verified=len(records),
        head_hash=head_hash,
        failures=tuple(failures)
    )

def append_ledger_payload(path: str, payload: dict[str, Any]) -> LedgerEnvelope:
    abspath = os.path.abspath(path)
    directory = os.path.dirname(abspath)
    if directory:
        os.makedirs(directory, exist_ok=True)

    with open(abspath, "a+b") as fh:
        lock_file_ex(fh)
        try:
            # Read and parse records under the exclusive write lock
            fh.seek(0)
            content_bytes = fh.read()
            lines = content_bytes.decode("utf-8").splitlines()
            records = []
            
            for index, line in enumerate(lines):
                line_str = line.strip()
                if not line_str:
                    continue
                try:
                    records.append(json.loads(line_str))
                except json.JSONDecodeError:
                    if index == len(lines) - 1:
                        # Skip trailing torn tail under exclusive lock
                        continue
                    raise

            # Verify existing chain integrity before append
            valid, failures = verify_records_sequence(records)
            if not valid:
                raise ValueError(f"Ledger history is invalid. Failures: {failures}")

            sequence = len(records)
            previous_hash = records[-1].get("entry_hash") if sequence > 0 else None
            
            envelope = build_ledger_envelope(
                sequence=sequence,
                previous_hash=previous_hash,
                payload=payload,
                recorded_at_unix=time.time()
            )
            
            # Write and sync the envelope canonical representation
            serialized = canonical_json_bytes(asdict(envelope)) + b"\n"
            fh.seek(0, os.SEEK_END)
            fh.write(serialized)
            fh.flush()
            os.fsync(fh.fileno())
            
            return envelope
        finally:
            unlock_file(fh)

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
    cert["content_hash"] = content_hash("zkaedi.promotion-certificate", cert)
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
    cert["content_hash"] = content_hash("zkaedi.rollback-certificate", cert)
    return cert

def verify_certificate(cert: dict[str, Any], domain: str) -> bool:
    supplied = cert.get("content_hash")
    unsigned = {
        key: value
        for key, value in cert.items()
        if key != "content_hash"
    }
    expected = content_hash(domain, unsigned)
    return isinstance(supplied, str) and secrets.compare_digest(
        supplied,
        expected
    )
