import json
import hashlib
import time
import os
import copy
import secrets
import math
from types import MappingProxyType
from collections.abc import Mapping
from dataclasses import dataclass, asdict
from typing import Optional, Any
from online_types import (
    canonical_json_bytes,
    lock_file_ex,
    lock_file_sh,
    unlock_file
)

GENESIS_SEQUENCE = 0
MAX_CLOCK_SKEW = 300.0
MAX_ANCHOR_AGE = 86400.0 * 30.0  # 30 days

class LedgerParseException(ValueError):
    def __init__(self, message: str, records_verified: int):
        super().__init__(message)
        self.records_verified = records_verified

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
class LedgerAnchor:
    ledger_id: str
    sequence: int
    head_hash: str
    anchored_at_unix: float
    signer_key_id: str
    signature: str

@dataclass(frozen=True, slots=True)
class LedgerVerification:
    valid: bool
    records_verified: int
    head_hash: Optional[str]
    failures: tuple[str, ...]

def validate_json_mapping_keys(value: Any) -> None:
    if isinstance(value, set | frozenset):
        raise TypeError("Sets are not supported in canonical ledger payloads")
    if isinstance(value, Mapping):
        for key, item in value.items():
            if not isinstance(key, str):
                raise TypeError("Ledger payload mapping keys must be strings")
            validate_json_mapping_keys(item)
    elif isinstance(value, list | tuple):
        for item in value:
            validate_json_mapping_keys(item)

def deep_freeze(value: Any) -> Any:
    if isinstance(value, set | frozenset):
        raise TypeError("Sets are not supported in canonical ledger payloads")
    if isinstance(value, Mapping):
        return MappingProxyType({
            key: deep_freeze(item)
            for key, item in value.items()
        })
    if isinstance(value, list | tuple):
        return tuple(deep_freeze(item) for item in value)
    return copy.deepcopy(value)

def thaw(val: Any) -> Any:
    if isinstance(val, MappingProxyType) or isinstance(val, dict):
        return {k: thaw(v) for k, v in val.items()}
    if isinstance(val, tuple | list):
        return [thaw(v) for v in val]
    return val

def serialize_envelope(envelope: LedgerEnvelope) -> bytes:
    envelope_dict = {
        "sequence": envelope.sequence,
        "previous_hash": envelope.previous_hash,
        "payload": thaw(envelope.payload),
        "payload_hash": envelope.payload_hash,
        "entry_hash": envelope.entry_hash,
        "recorded_at_unix": envelope.recorded_at_unix,
    }
    return canonical_json_bytes(envelope_dict)

def is_sha256_identifier(value: Any) -> bool:
    if not isinstance(value, str):
        return False
    prefix, separator, digest = value.partition(":")
    return (
        separator == ":"
        and prefix == "sha256"
        and len(digest) == 64
        and all(char in "0123456789abcdef" for char in digest)
    )

def hashes_equal(left: Any, right: Any) -> bool:
    return (
        isinstance(left, str)
        and isinstance(right, str)
        and is_sha256_identifier(left)
        and is_sha256_identifier(right)
        and secrets.compare_digest(left, right)
    )

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
    validate_json_mapping_keys(payload)
    canonical_payload = copy.deepcopy(payload)
    frozen_payload = deep_freeze(canonical_payload)
    payload_hash = content_hash("zkaedi.ledger-payload", canonical_payload)
    
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
        payload=frozen_payload,
        payload_hash=payload_hash,
        entry_hash=entry_hash,
        recorded_at_unix=recorded_at_unix
    )

def verify_records_sequence(records: list[dict]) -> tuple[bool, list[str]]:
    failures = []
    
    for idx, rec in enumerate(records):
        seq = rec.get("sequence")
        
        # Verify sequence type and increment
        if not isinstance(seq, int) or isinstance(seq, bool):
            failures.append(f"Record index {idx} sequence field is not an integer")
            continue
            
        expected_seq = GENESIS_SEQUENCE + idx
        if seq != expected_seq:
            failures.append(f"Record index {idx} has invalid sequence: {seq} (expected: {expected_seq})")
            
        payload = rec.get("payload")
        payload_hash = rec.get("payload_hash")
        entry_hash = rec.get("entry_hash")
        prev_hash = rec.get("previous_hash")
        
        # Safe validations of SHA-256 identifier hashes
        if not is_sha256_identifier(payload_hash):
            failures.append(f"Record index {idx} payload_hash is not a valid SHA-256 identifier")
            continue
        if not is_sha256_identifier(entry_hash):
            failures.append(f"Record index {idx} entry_hash is not a valid SHA-256 identifier")
            continue
        if idx > 0 and not is_sha256_identifier(prev_hash):
            failures.append(f"Record index {idx} previous_hash is not a valid SHA-256 identifier")
            continue

        # Safe verification of payload keys
        try:
            validate_json_mapping_keys(payload)
        except (TypeError, ValueError) as exc:
            failures.append(f"Record index {idx} payload keys validation failed: {exc}")
            continue

        # Verify payload hash with crash-safe catch for non-canonical structures
        try:
            expected_p_hash = content_hash("zkaedi.ledger-payload", payload)
        except (TypeError, ValueError, OverflowError) as exc:
            failures.append(f"Record sequence {seq} payload is not canonical JSON: {exc}")
            continue

        if not hashes_equal(payload_hash, expected_p_hash):
            failures.append(f"Record sequence {seq} payload hash mismatch")
            
        # Verify header/entry hash
        recorded_at = rec.get("recorded_at_unix")
        if (
            isinstance(recorded_at, bool)
            or not isinstance(recorded_at, (int, float))
            or not math.isfinite(float(recorded_at))
        ):
            failures.append(f"Record sequence {seq} recorded_at_unix is invalid")
            continue
            
        header = {
            "sequence": seq,
            "previous_hash": prev_hash,
            "payload_hash": payload_hash,
            "recorded_at_unix": recorded_at,
        }
        
        # Verify header hash with crash-safe catch
        try:
            expected_entry_hash = content_hash("zkaedi.ledger-header", header)
        except (TypeError, ValueError, OverflowError) as exc:
            failures.append(f"Record sequence {seq} header is not canonical JSON: {exc}")
            continue

        if not hashes_equal(entry_hash, expected_entry_hash):
            failures.append(f"Record sequence {seq} entry hash mismatch")
            
        # Verify chain linking
        if idx == 0:
            if prev_hash is not None:
                failures.append("Genesis record previous_hash must be None")
        else:
            prev_rec = records[idx - 1]
            prev_entry_hash = prev_rec.get("entry_hash")
            if not hashes_equal(prev_hash, prev_entry_hash):
                failures.append(f"Record sequence {seq} points to invalid parent hash: {prev_hash}")
                
    return len(failures) == 0, failures

def read_complete_jsonl_prefix(data: bytes) -> tuple[list[dict], int]:
    records: list[dict] = []
    valid_end = 0
    cursor = 0

    for raw_line in data.splitlines(keepends=True):
        cursor += len(raw_line)
        stripped = raw_line.strip()

        if not stripped:
            valid_end = cursor
            continue

        try:
            line_str = raw_line.decode("utf-8")
            records.append(json.loads(line_str))
            valid_end = cursor
        except UnicodeDecodeError as exc:
            is_final = (cursor == len(data))
            unterminated = not raw_line.endswith(b"\n")
            if is_final and unterminated:
                return records, valid_end
            raise LedgerParseException(
                f"ledger contains invalid UTF-8 at record {len(records)}: {exc}",
                len(records)
            ) from exc
        except json.JSONDecodeError as exc:
            is_final = (cursor == len(data))
            unterminated = not raw_line.endswith(b"\n")

            if is_final and unterminated:
                return records, valid_end

            raise LedgerParseException(
                f"ledger contains durable JSON corruption: {exc}",
                len(records)
            ) from exc

    return records, valid_end

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

    try:
        records, valid_end = read_complete_jsonl_prefix(content_bytes)
    except LedgerParseException as err:
        return LedgerVerification(valid=False, records_verified=err.records_verified, head_hash=None, failures=(str(err),))

    failures = []
    if valid_end < len(content_bytes):
        failures.append("ledger contains an incomplete trailing write")

    valid_seq, seq_failures = verify_records_sequence(records)
    failures.extend(seq_failures)
    
    valid = (len(failures) == 0) and valid_seq
    head_hash = records[-1].get("entry_hash") if (valid and records) else None
    
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
            fh.seek(0)
            content_bytes = fh.read()
            
            # Read complete prefix and safely recover from a trailing torn write by truncating it
            records, valid_end = read_complete_jsonl_prefix(content_bytes)
            if valid_end < len(content_bytes):
                fh.seek(valid_end)
                fh.truncate()
                fh.flush()
                os.fsync(fh.fileno())

            # Verify existing chain integrity before appending
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
            serialized = serialize_envelope(envelope) + b"\n"
            fh.seek(0, os.SEEK_END)
            fh.write(serialized)
            fh.flush()
            os.fsync(fh.fileno())
            
            # POSIX directory sync
            if hasattr(os, "O_DIRECTORY") and directory:
                dir_fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(dir_fd)
                finally:
                    os.close(dir_fd)
            
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

def _verify_certificate(cert: dict[str, Any], domain: str) -> bool:
    if not isinstance(cert, dict):
        return False
        
    supplied = cert.get("content_hash")
    if not is_sha256_identifier(supplied):
        return False
        
    unsigned = {
        key: value
        for key, value in cert.items()
        if key != "content_hash"
    }
    
    try:
        expected = content_hash(domain, unsigned)
    except (TypeError, ValueError, OverflowError):
        return False
        
    return secrets.compare_digest(supplied, expected)

def verify_promotion_certificate(cert: dict[str, Any]) -> bool:
    return _verify_certificate(cert, "zkaedi.promotion-certificate")

def verify_rollback_certificate(cert: dict[str, Any]) -> bool:
    return _verify_certificate(cert, "zkaedi.rollback-certificate")

def anchor_signing_bytes(anchor: LedgerAnchor) -> bytes:
    return canonical_json_bytes({
        "domain": "zkaedi.ledger-anchor",
        "schema_version": 1,
        "ledger_id": anchor.ledger_id,
        "sequence": anchor.sequence,
        "head_hash": anchor.head_hash,
        "anchored_at_unix": anchor.anchored_at_unix,
        "signer_key_id": anchor.signer_key_id,
    })

def sign_ledger_anchor(
    *,
    ledger_id: str,
    verification: LedgerVerification,
    signer_key_id: str,
    private_key,  # ed25519.Ed25519PrivateKey
    anchored_at_unix: Optional[float] = None,
) -> LedgerAnchor:
    if not verification.valid or verification.head_hash is None or verification.records_verified <= 0:
        raise ValueError("cannot anchor an invalid or empty ledger")
        
    unsigned = LedgerAnchor(
        ledger_id=ledger_id,
        sequence=verification.records_verified - 1,
        head_hash=verification.head_hash,
        anchored_at_unix=(
            time.time()
            if anchored_at_unix is None
            else anchored_at_unix
        ),
        signer_key_id=signer_key_id,
        signature=""
    )
    
    sig_bytes = private_key.sign(anchor_signing_bytes(unsigned))
    return LedgerAnchor(
        ledger_id=unsigned.ledger_id,
        sequence=unsigned.sequence,
        head_hash=unsigned.head_hash,
        anchored_at_unix=unsigned.anchored_at_unix,
        signer_key_id=unsigned.signer_key_id,
        signature=sig_bytes.hex()
    )

def evaluate_anchor_policy(anchor: LedgerAnchor, *, max_age: float = MAX_ANCHOR_AGE) -> bool:
    now = time.time()
    if anchor.anchored_at_unix > now + MAX_CLOCK_SKEW:
        return False
    if now - anchor.anchored_at_unix > max_age:
        return False
    return True

def verify_ledger_anchor(
    anchor: LedgerAnchor,
    *,
    expected_ledger_id: str,
    verification: LedgerVerification,
    trusted_keys: Mapping[str, bytes],
) -> bool:
    # 1. Type validation on anchor properties
    if isinstance(anchor.sequence, bool) or not isinstance(anchor.sequence, int):
        return False
    if anchor.sequence < GENESIS_SEQUENCE:
        return False
    if (
        isinstance(anchor.anchored_at_unix, bool)
        or not isinstance(anchor.anchored_at_unix, (int, float))
        or not math.isfinite(float(anchor.anchored_at_unix))
    ):
        return False
    if not isinstance(anchor.signer_key_id, str) or not anchor.signer_key_id:
        return False
    if not isinstance(anchor.signature, str) or not anchor.signature:
        return False

    # Verify key signature format explicitly
    try:
        signature_bytes = bytes.fromhex(anchor.signature)
    except ValueError:
        return False
    if len(signature_bytes) != 64:
        return False

    # 2. Check structural validation alignment
    if not verification.valid:
        return False
    if verification.records_verified <= 0:
        return False
    if verification.head_hash is None:
        return False

    if anchor.ledger_id != expected_ledger_id:
        return False
    if anchor.sequence != verification.records_verified - 1:
        return False
    if not hashes_equal(anchor.head_hash, verification.head_hash):
        return False

    # 3. Resolve key against trusted keys registry
    public_key = trusted_keys.get(anchor.signer_key_id)
    if public_key is None:
        return False
    if not isinstance(public_key, bytes) or len(public_key) != 32:
        return False

    # 4. Cryptographic signature check
    data_bytes = anchor_signing_bytes(anchor)
    try:
        from cryptography.hazmat.primitives.asymmetric import ed25519
        from cryptography.exceptions import InvalidSignature
        public_key_obj = ed25519.Ed25519PublicKey.from_public_bytes(public_key)
        public_key_obj.verify(signature_bytes, data_bytes)
        return True
    except (ValueError, TypeError, InvalidSignature):
        return False
