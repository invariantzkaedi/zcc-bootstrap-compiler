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

GENESIS_SEQUENCE = 0
MAX_CLOCK_SKEW = 300.0
MAX_ANCHOR_AGE = 86400.0 * 30.0  # 30 days
LEDGER_SCHEMA_VERSION = 1
ANCHOR_SCHEMA_VERSION = 1

from lineage.online_types import (
    canonical_json_bytes,
    lock_file_ex,
    lock_file_sh,
    unlock_file
)

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
    schema_version: int
    ledger_id: str
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
    initialized: bool
    records_verified: int
    head_hash: Optional[str]
    failures: tuple[str, ...]

@dataclass(frozen=True, slots=True)
class TrustedSigner:
    public_key: bytes
    valid_from: float
    valid_until: Optional[float]
    revoked_at: Optional[float]
    allowed_ledgers: Optional[frozenset[str]]

def unrestricted_legacy_signer(public_key: bytes) -> TrustedSigner:
    if not isinstance(public_key, bytes) or len(public_key) != 32:
        raise ValueError("Ed25519 public key must contain exactly 32 bytes")
    return TrustedSigner(
        public_key=public_key,
        valid_from=0.0,
        valid_until=None,
        revoked_at=None,
        allowed_ledgers=None
    )

def validate_ledger_id(value: Any) -> str:
    if not isinstance(value, str) or not value:
        raise ValueError("ledger_id must be a non-empty string")
    if len(value) > 128:
        raise ValueError("ledger_id is too long")
    if not value.isprintable():
        raise ValueError("ledger_id contains control characters")
    return value

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
        "schema_version": envelope.schema_version,
        "ledger_id": envelope.ledger_id,
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
    ledger_id: str,
    sequence: int,
    previous_hash: Optional[str],
    payload: dict[str, Any],
    recorded_at_unix: float
) -> LedgerEnvelope:
    # 1. Parameter type and invariant checks
    validate_ledger_id(ledger_id)
    if isinstance(sequence, bool) or not isinstance(sequence, int):
        raise TypeError("sequence must be an integer")
    if sequence < GENESIS_SEQUENCE:
        raise ValueError("sequence cannot be negative")

    if sequence == GENESIS_SEQUENCE:
        if previous_hash is not None:
            raise ValueError("genesis previous_hash must be None")
    elif not is_sha256_identifier(previous_hash):
        raise ValueError("non-genesis previous_hash must be a SHA-256 identifier")

    if (
        isinstance(recorded_at_unix, bool)
        or not isinstance(recorded_at_unix, (int, float))
        or not math.isfinite(float(recorded_at_unix))
    ):
        raise ValueError("recorded_at_unix must be finite")

    if not isinstance(payload, Mapping):
        raise TypeError("Ledger payload must be a mapping")

    validate_json_mapping_keys(payload)
    canonical_payload = copy.deepcopy(payload)
    frozen_payload = deep_freeze(canonical_payload)
    payload_hash = content_hash("zkaedi.ledger-payload", canonical_payload)
    
    header = {
        "schema_version": LEDGER_SCHEMA_VERSION,
        "ledger_id": ledger_id,
        "sequence": sequence,
        "previous_hash": previous_hash,
        "payload_hash": payload_hash,
        "recorded_at_unix": recorded_at_unix,
    }
    entry_hash = content_hash("zkaedi.ledger-header", header)
    
    return LedgerEnvelope(
        schema_version=LEDGER_SCHEMA_VERSION,
        ledger_id=ledger_id,
        sequence=sequence,
        previous_hash=previous_hash,
        payload=frozen_payload,
        payload_hash=payload_hash,
        entry_hash=entry_hash,
        recorded_at_unix=recorded_at_unix
    )

def verify_records_sequence(records: list[dict], expected_ledger_id: str) -> tuple[bool, list[str]]:
    try:
        validate_ledger_id(expected_ledger_id)
    except ValueError as exc:
        return False, [f"invalid expected_ledger_id: {exc}"]

    failures = []
    previous_entry_hash = None
    
    for idx, rec in enumerate(records):
        record_valid = True
        
        if not isinstance(rec, dict):
            failures.append(f"Record index {idx} is not a JSON object")
            previous_entry_hash = None
            continue

        schema_version = rec.get("schema_version")
        if (
            isinstance(schema_version, bool)
            or not isinstance(schema_version, int)
        ):
            failures.append(f"Record index {idx} schema_version must be an integer")
            record_valid = False
            previous_entry_hash = None
            continue
        if schema_version != LEDGER_SCHEMA_VERSION:
            failures.append(f"Record index {idx} invalid schema_version: {schema_version} (expected: {LEDGER_SCHEMA_VERSION})")
            record_valid = False
            previous_entry_hash = None
            continue

        seq = rec.get("sequence")
        
        # Verify sequence type and increment
        if not isinstance(seq, int) or isinstance(seq, bool):
            failures.append(f"Record index {idx} sequence field is not an integer")
            previous_entry_hash = None
            continue
            
        expected_seq = GENESIS_SEQUENCE + idx
        if seq != expected_seq:
            failures.append(f"Record index {idx} has invalid sequence: {seq} (expected: {expected_seq})")
            record_valid = False
            
        payload = rec.get("payload")
        
        if not isinstance(payload, dict):
            failures.append(f"Record sequence {seq} payload must be a JSON object")
            previous_entry_hash = None
            continue

        payload_hash = rec.get("payload_hash")
        entry_hash = rec.get("entry_hash")
        prev_hash = rec.get("previous_hash")
        rec_ledger_id = rec.get("ledger_id")
        
        if rec_ledger_id != expected_ledger_id:
            failures.append(f"Record sequence {seq} ledger_id mismatch: {rec_ledger_id} (expected: {expected_ledger_id})")
            previous_entry_hash = None
            continue

        # Safe validations of SHA-256 identifier hashes
        if not is_sha256_identifier(payload_hash):
            failures.append(f"Record index {idx} payload_hash is not a valid SHA-256 identifier")
            previous_entry_hash = None
            continue
        if not is_sha256_identifier(entry_hash):
            failures.append(f"Record index {idx} entry_hash is not a valid SHA-256 identifier")
            previous_entry_hash = None
            continue
        if idx > 0 and not is_sha256_identifier(prev_hash):
            failures.append(f"Record index {idx} previous_hash is not a valid SHA-256 identifier")
            previous_entry_hash = None
            continue

        # Safe verification of payload keys
        try:
            validate_json_mapping_keys(payload)
        except (TypeError, ValueError) as exc:
            failures.append(f"Record index {idx} payload keys validation failed: {exc}")
            previous_entry_hash = None
            continue

        # Verify payload hash with crash-safe catch for non-canonical structures
        try:
            expected_p_hash = content_hash("zkaedi.ledger-payload", payload)
        except (TypeError, ValueError, OverflowError) as exc:
            failures.append(f"Record sequence {seq} payload is not canonical JSON: {exc}")
            previous_entry_hash = None
            continue

        if not hashes_equal(payload_hash, expected_p_hash):
            failures.append(f"Record sequence {seq} payload hash mismatch")
            record_valid = False
            
        # Verify header/entry hash
        recorded_at = rec.get("recorded_at_unix")
        if (
            isinstance(recorded_at, bool)
            or not isinstance(recorded_at, (int, float))
            or not math.isfinite(float(recorded_at))
        ):
            failures.append(f"Record sequence {seq} recorded_at_unix is invalid")
            previous_entry_hash = None
            continue
            
        header = {
            "schema_version": 1,
            "ledger_id": expected_ledger_id,
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
            previous_entry_hash = None
            continue

        if not hashes_equal(entry_hash, expected_entry_hash):
            failures.append(f"Record sequence {seq} entry hash mismatch")
            record_valid = False
            
        # Verify chain linking
        if idx == 0:
            if prev_hash is not None:
                failures.append("Genesis record previous_hash must be None")
                record_valid = False
        else:
            if previous_entry_hash is None:
                failures.append(f"Record sequence {seq} follows an invalid parent")
                record_valid = False
            elif not hashes_equal(prev_hash, previous_entry_hash):
                failures.append(f"Record sequence {seq} points to an invalid parent")
                record_valid = False

        previous_entry_hash = (
            entry_hash
            if record_valid
            else None
        )
                
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
            decoded = json.loads(line_str)
            if not isinstance(decoded, dict):
                raise LedgerParseException(
                    f"ledger record {len(records)} must be a JSON object",
                    len(records)
                )
            records.append(decoded)
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

def verify_ledger(path: str, expected_ledger_id: str) -> LedgerVerification:
    try:
        validate_ledger_id(expected_ledger_id)
    except ValueError as exc:
        return LedgerVerification(valid=False, initialized=False, records_verified=0, head_hash=None, failures=(f"invalid expected_ledger_id: {exc}",))

    abspath = os.path.abspath(path)
    if not os.path.exists(abspath):
        return LedgerVerification(valid=True, initialized=False, records_verified=0, head_hash=None, failures=())

    with open(abspath, "rb") as fh:
        lock_file_sh(fh)
        try:
            content_bytes = fh.read()
        finally:
            unlock_file(fh)

    try:
        records, valid_end = read_complete_jsonl_prefix(content_bytes)
    except LedgerParseException as err:
        return LedgerVerification(valid=False, initialized=False, records_verified=err.records_verified, head_hash=None, failures=(str(err),))

    failures = []
    if valid_end < len(content_bytes):
        failures.append("ledger contains an incomplete trailing write")

    valid_seq, seq_failures = verify_records_sequence(records, expected_ledger_id)
    failures.extend(seq_failures)
    
    valid = (len(failures) == 0) and valid_seq
    head_hash = records[-1].get("entry_hash") if (valid and records) else None
    initialized = len(records) > 0
    
    return LedgerVerification(
        valid=valid,
        initialized=initialized,
        records_verified=len(records),
        head_hash=head_hash,
        failures=tuple(failures)
    )

def append_ledger_payload(path: str, ledger_id: str, payload: dict[str, Any]) -> LedgerEnvelope:
    # 1. Transactional Prevalidation
    validate_ledger_id(ledger_id)
    if not isinstance(payload, Mapping):
        raise TypeError("Ledger payload must be a mapping")
        
    payload_snapshot = copy.deepcopy(payload)
    validate_json_mapping_keys(payload_snapshot)
    # Dry-run payload hash content sanity
    content_hash("zkaedi.ledger-payload", payload_snapshot)

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
            valid, failures = verify_records_sequence(records, ledger_id)
            if not valid:
                raise ValueError(f"Ledger history is invalid. Failures: {failures}")

            sequence = len(records)
            previous_hash = records[-1].get("entry_hash") if sequence > 0 else None
            
            envelope = build_ledger_envelope(
                ledger_id=ledger_id,
                sequence=sequence,
                previous_hash=previous_hash,
                payload=payload_snapshot,
                recorded_at_unix=time.time()
            )
            
            # Write and sync the envelope canonical representation
            serialized = serialize_envelope(envelope) + b"\n"
            fh.seek(0, os.SEEK_END)
            truncated_bytes = content_bytes[:valid_end]
            if truncated_bytes and not truncated_bytes.endswith(b"\n"):
                fh.write(b"\n")
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
        "schema_version": ANCHOR_SCHEMA_VERSION,
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
    if not isinstance(verification, LedgerVerification):
        raise TypeError("verification must be a LedgerVerification")
    if not verification.valid or not verification.initialized or verification.head_hash is None or verification.records_verified <= 0:
        raise ValueError("cannot anchor an invalid, uninitialized or empty ledger")
        
    validate_ledger_id(ledger_id)
    if not isinstance(signer_key_id, str) or not signer_key_id:
        raise ValueError("signer_key_id must be non-empty")

    issued_at = (
        time.time()
        if anchored_at_unix is None
        else anchored_at_unix
    )
    if (
        isinstance(issued_at, bool)
        or not isinstance(issued_at, (int, float))
        or not math.isfinite(float(issued_at))
    ):
        raise ValueError("anchored_at_unix must be finite")

    unsigned = LedgerAnchor(
        ledger_id=ledger_id,
        sequence=verification.records_verified - 1,
        head_hash=verification.head_hash,
        anchored_at_unix=issued_at,
        signer_key_id=signer_key_id,
        signature=""
    )
    
    sig_bytes = private_key.sign(anchor_signing_bytes(unsigned))
    if not isinstance(sig_bytes, bytes) or len(sig_bytes) != 64:
        raise ValueError("private key returned an invalid Ed25519 signature")

    return LedgerAnchor(
        ledger_id=unsigned.ledger_id,
        sequence=unsigned.sequence,
        head_hash=unsigned.head_hash,
        anchored_at_unix=unsigned.anchored_at_unix,
        signer_key_id=unsigned.signer_key_id,
        signature=sig_bytes.hex()
    )

def _valid_timestamp(value: Any, *, allow_none: bool = False) -> bool:
    if value is None:
        return allow_none
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
    )

def evaluate_anchor_policy(
    anchor: LedgerAnchor,
    *,
    max_age: float = MAX_ANCHOR_AGE,
    now: Optional[float] = None,
) -> bool:
    if not isinstance(anchor, LedgerAnchor):
        return False

    if (
        isinstance(max_age, bool)
        or not isinstance(max_age, (int, float))
        or not math.isfinite(float(max_age))
        or max_age < 0
    ):
        return False

    timestamp = anchor.anchored_at_unix
    if (
        isinstance(timestamp, bool)
        or not isinstance(timestamp, (int, float))
        or not math.isfinite(float(timestamp))
    ):
        return False

    current = time.time() if now is None else now
    if (
        isinstance(current, bool)
        or not isinstance(current, (int, float))
        or not math.isfinite(float(current))
    ):
        return False

    return (
        timestamp <= current + MAX_CLOCK_SKEW
        and current - timestamp <= max_age
    )

def verify_ledger_anchor(
    anchor: LedgerAnchor,
    *,
    expected_ledger_id: str,
    verification: LedgerVerification,
    trusted_keys: Mapping[str, TrustedSigner],
) -> bool:
    # Fail-closed inputs validation guards
    if not isinstance(anchor, LedgerAnchor):
        return False
    if not isinstance(verification, LedgerVerification):
        return False
    if not isinstance(trusted_keys, Mapping):
        return False

    try:
        validate_ledger_id(expected_ledger_id)
    except ValueError:
        return False

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
    if not verification.initialized:
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
    try:
        signer_entry = trusted_keys.get(anchor.signer_key_id)
    except Exception:
        return False

    if signer_entry is None:
        return False

    if not isinstance(signer_entry, TrustedSigner):
        return False

    public_key = signer_entry.public_key
    valid_from = signer_entry.valid_from
    valid_until = signer_entry.valid_until
    revoked_at = signer_entry.revoked_at
    allowed_ledgers = signer_entry.allowed_ledgers

    if not isinstance(public_key, bytes) or len(public_key) != 32:
        return False

    # Runtime schema validations on TrustedSigner properties
    if not _valid_timestamp(valid_from):
        return False
    if not _valid_timestamp(valid_until, allow_none=True):
        return False
    if not _valid_timestamp(revoked_at, allow_none=True):
        return False
    if valid_until is not None and valid_until < valid_from:
        return False
    if revoked_at is not None and revoked_at < valid_from:
        return False
    if allowed_ledgers is not None:
        if not isinstance(allowed_ledgers, frozenset):
            return False
        if not all(isinstance(item, str) and item for item in allowed_ledgers):
            return False

    # Validate key lifecycle policy at anchor.anchored_at_unix time
    t = anchor.anchored_at_unix
    if t < valid_from:
        return False
    if valid_until is not None and t >= valid_until:
        return False
    if revoked_at is not None and t >= revoked_at:
        return False
    if allowed_ledgers is not None and expected_ledger_id not in allowed_ledgers:
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

def accept_ledger_anchor(
    anchor: LedgerAnchor,
    *,
    expected_ledger_id: str,
    verification: LedgerVerification,
    trusted_keys: Mapping[str, TrustedSigner],
    max_age: float = MAX_ANCHOR_AGE,
    now: Optional[float] = None,
) -> bool:
    try:
        validate_ledger_id(expected_ledger_id)
    except ValueError:
        return False

    if (
        isinstance(max_age, bool)
        or not isinstance(max_age, (int, float))
        or not math.isfinite(float(max_age))
        or max_age < 0
    ):
        return False
    return (
        verify_ledger_anchor(
            anchor,
            expected_ledger_id=expected_ledger_id,
            verification=verification,
            trusted_keys=trusted_keys,
        )
        and evaluate_anchor_policy(
            anchor,
            max_age=max_age,
            now=now,
        )
    )
