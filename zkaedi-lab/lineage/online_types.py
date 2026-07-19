import json
import os
import time
import hashlib
import secrets
import copy
import math
from typing import Any
from dataclasses import dataclass, asdict

# Windows-compatible cross-platform lock fallback
try:
    import fcntl
    HAS_FCNTL = True
    HAS_MSVCRT = False
except ImportError:
    HAS_FCNTL = False
    try:
        import msvcrt
        HAS_MSVCRT = True
    except ImportError:
        HAS_MSVCRT = False

ONLINE_OUTCOME_SCHEMA_VERSION = 1

@dataclass(frozen=True)
class OnlineOutcome:
    config_id: str
    candidate_id: str
    prompt: str
    completion: str

    sandbox_passed: bool
    safety_passed: bool
    verification_score: float
    runtime_ms: float

    runner_exit: int
    verdict: str
    failure_class: str | None

    # Version metadata
    harness_version: str
    evaluator_version: str
    policy_checkpoint: str
    sandbox_version: str

    def __post_init__(self) -> None:
        if not isinstance(self.config_id, str) or not self.config_id:
            raise ValueError("config_id must be a non-empty string")
        if not isinstance(self.candidate_id, str) or not self.candidate_id:
            raise ValueError("candidate_id must be a non-empty string")
        if not isinstance(self.prompt, str) or not self.prompt:
            raise ValueError("prompt must be a non-empty string")
        if not isinstance(self.completion, str) or not self.completion:
            raise ValueError("completion must be a non-empty string")
        if not isinstance(self.verdict, str) or self.verdict not in {"pass", "fail", "error"}:
            raise ValueError("verdict must be 'pass', 'fail', or 'error'")
        
        if isinstance(self.runner_exit, bool) or not isinstance(self.runner_exit, int):
            raise TypeError("runner_exit must be an integer (non-boolean)")

        if isinstance(self.runtime_ms, bool) or not isinstance(self.runtime_ms, (int, float)) or not math.isfinite(self.runtime_ms) or self.runtime_ms < 0:
            raise ValueError("runtime_ms must be a finite non-negative number")

        if isinstance(self.verification_score, bool) or not isinstance(self.verification_score, (int, float)) or not math.isfinite(self.verification_score):
            raise ValueError("verification_score must be a finite number")

        for name in ("sandbox_passed", "safety_passed"):
            value = getattr(self, name)
            if not isinstance(value, bool):
                raise TypeError(f"{name} must be a boolean")

        for name in (
            "harness_version",
            "evaluator_version",
            "policy_checkpoint",
            "sandbox_version",
        ):
            value = getattr(self, name)
            if not isinstance(value, str) or not value:
                raise ValueError(f"{name} must be a non-empty string")

        if self.failure_class is not None:
            if not isinstance(self.failure_class, str) or not self.failure_class:
                raise ValueError("failure_class must be a non-empty string or None")
                
        if self.verdict == "pass":
            if self.failure_class is not None:
                raise ValueError("verdict 'pass' cannot have a failure_class")
            if not self.sandbox_passed or not self.safety_passed:
                raise ValueError("pass verdict requires sandbox and safety success")
            if self.runner_exit != 0:
                raise ValueError("pass verdict requires runner_exit == 0")

        if self.verdict in {"fail", "error"} and self.failure_class is None:
            raise ValueError("failed outcomes require a failure_class")

@dataclass(frozen=True)
class ReplayLoadResult:
    records: list[dict]
    # tail_record_skipped: True if one or more trailing corrupted tail records were skipped
    tail_record_skipped: bool
    skipped_duplicate_count: int
    # tail_missing_newline: True if the file did not end with a trailing newline
    tail_missing_newline: bool

def derive_desirability(outcome: OnlineOutcome) -> bool:
    return (
        outcome.sandbox_passed
        and outcome.safety_passed
        and outcome.verdict == "pass"
        and outcome.verification_score >= 1.0
    )

TRAINABLE_FAILURES = {
    "policy_violation",
    "schema_violation",
    "verification_failure",
}

def should_train_on(outcome: OnlineOutcome) -> bool:
    if derive_desirability(outcome):
        return True
    return outcome.failure_class in TRAINABLE_FAILURES

def canonical_json_bytes(value: Any) -> bytes:
    return json.dumps(
        value,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")

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

def compute_dedup_hash(
    prompt: str,
    completion: str,
    harness_version: str,
    evaluator_version: str,
    policy_checkpoint: str,
    sandbox_version: str,
    schema_version: int = ONLINE_OUTCOME_SCHEMA_VERSION,
) -> str:
    payload = {
        "prompt": prompt,
        "completion": completion,
        "harness_version": harness_version,
        "evaluator_version": evaluator_version,
        "policy_checkpoint": policy_checkpoint,
        "sandbox_version": sandbox_version,
        "schema_version": schema_version,
    }
    return "sha256:" + hashlib.sha256(canonical_json_bytes(payload)).hexdigest()

def compute_record_hash(record: dict[str, Any]) -> str:
    if not isinstance(record, dict):
        raise TypeError("record must be a dictionary")
    # Verify that all keys are strings
    for k in record.keys():
        if not isinstance(k, str):
            raise TypeError("record keys must be strings")
    unsigned = {
        key: value
        for key, value in record.items()
        if key != "record_hash"
    }
    # Validate canonical JSON serializability
    try:
        canonical_json_bytes(unsigned)
    except Exception as exc:
        raise ValueError(f"record cannot be serialized to canonical JSON: {exc}") from exc
        
    return "sha256:" + hashlib.sha256(
        canonical_json_bytes({
            "domain": "zkaedi.online-outcome",
            "schema_version": ONLINE_OUTCOME_SCHEMA_VERSION,
            "value": unsigned,
        })
    ).hexdigest()

def compute_replay_root_hash(record_hashes: list[str]) -> str:
    payload = {
        "record_count": len(record_hashes),
        "ordered_record_hashes": record_hashes,
    }
    canonical = json.dumps(
        {
            "domain": "zkaedi.replay-snapshot",
            "value": payload,
        },
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")
    return "sha256:" + hashlib.sha256(canonical).hexdigest()

def lock_file_ex(fh) -> None:
    if HAS_FCNTL:
        fcntl.flock(fh.fileno(), fcntl.LOCK_EX)
    elif HAS_MSVCRT:
        # Reposition to lock byte 0 consistently across readers/writers on Windows
        fh.seek(0)
        msvcrt.locking(fh.fileno(), msvcrt.LK_LOCK, 1)

def lock_file_sh(fh) -> None:
    """Acquires a shared lock on POSIX, but emulates using exclusive locks on Windows.
    Windows LK_LOCK is blocking and exclusive, ensuring safety across Windows runtimes.
    """
    if HAS_FCNTL:
        fcntl.flock(fh.fileno(), fcntl.LOCK_SH)
    elif HAS_MSVCRT:
        fh.seek(0)
        msvcrt.locking(fh.fileno(), msvcrt.LK_LOCK, 1)

def unlock_file(fh) -> None:
    if HAS_FCNTL:
        fcntl.flock(fh.fileno(), fcntl.LOCK_UN)
    elif HAS_MSVCRT:
        fh.seek(0)
        msvcrt.locking(fh.fileno(), msvcrt.LK_UNLCK, 1)

def read_replay_prefix(content: bytes) -> tuple[list[dict[str, Any]], int]:
    records: list[dict[str, Any]] = []
    valid_end = 0
    cursor = 0

    for raw_line in content.splitlines(keepends=True):
        cursor += len(raw_line)

        if not raw_line.strip():
            valid_end = cursor
            continue

        is_final = (cursor == len(content))
        unterminated = not raw_line.endswith(b"\n")

        try:
            decoded = json.loads(raw_line.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            if is_final and unterminated:
                return records, valid_end
            raise ValueError(
                f"replay contains durable corruption near record {len(records)}"
            ) from exc

        if not isinstance(decoded, dict):
            raise ValueError(
                f"replay record {len(records)} must be a JSON object"
            )

        records.append(decoded)
        valid_end = cursor

    return records, valid_end

def append_jsonl_durable(path: str, record: dict[str, Any]) -> None:
    if not isinstance(record, dict):
        raise TypeError("record must be a dictionary")
        
    abspath = os.path.abspath(path)
    directory = os.path.dirname(abspath)
    if directory:
        os.makedirs(directory, exist_ok=True)
        
    record_snapshot = copy.deepcopy(record)
    serialized = canonical_json_bytes(record_snapshot) + b"\n"
    
    with open(abspath, "a+b") as fh:
        lock_file_ex(fh)
        try:
            fh.seek(0, os.SEEK_END)
            size = fh.tell()
            
            # Read up to the last 8192 bytes
            read_size = min(size, 8192)
            if read_size > 0:
                fh.seek(-read_size, os.SEEK_END)
                chunk = fh.read(read_size)
                
                # Find last line
                lines = chunk.splitlines(keepends=True)
                if lines:
                    last_line = lines[-1]
                    # Check if last_line is terminated
                    if not last_line.endswith(b"\n") and last_line.strip():
                        # It is unterminated. Let's see if it parses.
                        try:
                            json.loads(last_line.decode("utf-8"))
                            # It is a valid record but just missing a newline. We don't truncate.
                        except (UnicodeDecodeError, json.JSONDecodeError):
                            # It is corrupted / torn. We must truncate it.
                            trunc_offset = size - len(last_line)
                            fh.seek(trunc_offset)
                            fh.truncate()
                            fh.flush()
                            os.fsync(fh.fileno())
            
            # Check spacing: if the file size after truncation is > 0 and does not end with \n, append \n
            fh.seek(0, os.SEEK_END)
            final_size = fh.tell()
            if final_size > 0:
                fh.seek(-1, os.SEEK_END)
                if fh.read(1) != b"\n":
                    fh.seek(0, os.SEEK_END)
                    fh.write(b"\n")
                    
            fh.seek(0, os.SEEK_END)
            fh.write(serialized)
            fh.flush()
            os.fsync(fh.fileno())
        finally:
            unlock_file(fh)

    if hasattr(os, "O_DIRECTORY") and directory:
        dir_fd = os.open(directory, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)

def record_online_outcome(path: str, outcome: OnlineOutcome) -> None:
    """Appends outcomes to the replay database.
    Note: deduplication checks are performed as a read-time projection during load_unique_records.
    """
    payload = asdict(outcome)
    payload["schema_version"] = ONLINE_OUTCOME_SCHEMA_VERSION
    payload["desirable"] = derive_desirability(outcome)
    payload["trainable"] = should_train_on(outcome)
    payload["recorded_at_unix"] = time.time()
    payload["dedup_hash"] = compute_dedup_hash(
        outcome.prompt,
        outcome.completion,
        outcome.harness_version,
        outcome.evaluator_version,
        outcome.policy_checkpoint,
        outcome.sandbox_version,
        schema_version=ONLINE_OUTCOME_SCHEMA_VERSION
    )
    payload["record_hash"] = compute_record_hash(payload)
    append_jsonl_durable(path, payload)

def load_unique_records(path: str, allow_legacy: bool = False) -> ReplayLoadResult:
    abspath = os.path.abspath(path)
    if not os.path.exists(abspath):
        return ReplayLoadResult(records=[], tail_record_skipped=False, skipped_duplicate_count=0, tail_missing_newline=False)

    with open(abspath, "rb") as fh:
        lock_file_sh(fh)
        try:
            fh.seek(0, os.SEEK_END)
            size = fh.tell()
            unterminated = False
            if size > 0:
                fh.seek(-1, os.SEEK_END)
                unterminated = fh.read(1) != b"\n"
            
            fh.seek(0)
            content_bytes = fh.read()
        finally:
            unlock_file(fh)

    try:
        records, valid_end = read_replay_prefix(content_bytes)
        skipped_torn = (valid_end < len(content_bytes))
    except ValueError as exc:
        raise ValueError(f"Failed to parse replay JSONL: {exc}") from exc

    unique: dict[str, dict] = {}
    skipped_dup = 0
    
    for index, record in enumerate(records):
        # Legacy migration boundary checks
        if "schema_version" not in record:
            if allow_legacy:
                unique[f"legacy:{index}"] = record
                continue
            else:
                raise ValueError(f"record {index} is unversioned; run the legacy migration tool")

        schema_version = record["schema_version"]
        if isinstance(schema_version, bool) or not isinstance(schema_version, int):
            raise ValueError(f"record {index} schema_version must be an integer")

        if schema_version == 0:
            if allow_legacy:
                if "dedup_hash" in record or "record_hash" in record:
                    raise ValueError(f"record {index} has a hash but has legacy schema_version")
                unique[f"legacy:{index}"] = record
                continue
            else:
                raise ValueError(f"record {index} is legacy; run the legacy migration tool")

        if schema_version != ONLINE_OUTCOME_SCHEMA_VERSION:
            raise ValueError(f"record {index} has unsupported schema_version: {schema_version}")

        if "dedup_hash" not in record:
            raise ValueError(f"record {index} is missing dedup_hash")

        dedup_hash = record["dedup_hash"]
        if not isinstance(dedup_hash, str):
            raise ValueError(f"record {index} dedup_hash must be a string")

        if not dedup_hash:
            raise ValueError(f"record {index} dedup_hash must not be empty")

        if not is_sha256_identifier(dedup_hash):
            raise ValueError(f"record {index} dedup_hash is malformed")

        # Integrity check on full record
        supplied_record_hash = record.get("record_hash")
        if not is_sha256_identifier(supplied_record_hash):
            raise ValueError(f"record {index} has malformed record_hash")
        
        expected_record_hash = compute_record_hash(record)
        if not secrets.compare_digest(supplied_record_hash, expected_record_hash):
            raise ValueError(f"record {index} failed integrity verification")

        # Re-verify the dedup hash to protect against manipulation
        try:
            expected_dedup_hash = compute_dedup_hash(
                record["prompt"],
                record["completion"],
                record["harness_version"],
                record["evaluator_version"],
                record["policy_checkpoint"],
                record["sandbox_version"],
                schema_version=schema_version
            )
            if not secrets.compare_digest(dedup_hash, expected_dedup_hash):
                raise ValueError(f"record {index} has an invalid dedup hash")
        except KeyError:
            raise ValueError(f"record {index} lacks required fields for dedup validation")

        # Conflict resolution check
        existing = unique.get(dedup_hash)
        if existing is None:
            unique[dedup_hash] = record
        elif secrets.compare_digest(existing["record_hash"], record["record_hash"]):
            skipped_dup += 1
        else:
            raise ValueError(f"record {index} conflicts with an existing dedup identity")

    return ReplayLoadResult(
        records=list(unique.values()),
        tail_record_skipped=skipped_torn,
        skipped_duplicate_count=skipped_dup,
        tail_missing_newline=unterminated
    )

def migrate_legacy_replay(
    source_path: str,
    destination_path: str,
    migration_version: str = "legacy-v0-to-v1",
) -> None:
    source_abspath = os.path.abspath(source_path)
    if not os.path.exists(source_abspath):
        return

    with open(source_abspath, "rb") as sf:
        lock_file_sh(sf)
        try:
            content = sf.read()
        finally:
            unlock_file(sf)

    records, _ = read_replay_prefix(content)
    
    for index, record in enumerate(records):
        # Determine if it needs migration
        if "schema_version" not in record or record.get("schema_version") == 0:
            source_record_hash = "sha256:" + hashlib.sha256(canonical_json_bytes(record)).hexdigest()
            
            migrated = copy.deepcopy(record)
            migrated["schema_version"] = ONLINE_OUTCOME_SCHEMA_VERSION
            migrated["migration"] = {
                "source_schema": 0,
                "migration_version": migration_version,
                "source_record_hash": source_record_hash,
            }
            
            prompt = migrated.get("prompt", "")
            completion = migrated.get("completion", "")
            harness_version = migrated.get("harness_version", "legacy")
            evaluator_version = migrated.get("evaluator_version", "legacy")
            policy_checkpoint = migrated.get("policy_checkpoint", "legacy")
            sandbox_version = migrated.get("sandbox_version", "legacy")
            
            migrated["prompt"] = prompt
            migrated["completion"] = completion
            migrated["harness_version"] = harness_version
            migrated["evaluator_version"] = evaluator_version
            migrated["policy_checkpoint"] = policy_checkpoint
            migrated["sandbox_version"] = sandbox_version
            
            migrated["dedup_hash"] = compute_dedup_hash(
                prompt, completion, harness_version, evaluator_version, policy_checkpoint, sandbox_version, schema_version=ONLINE_OUTCOME_SCHEMA_VERSION
            )
            migrated["record_hash"] = compute_record_hash(migrated)
            append_jsonl_durable(destination_path, migrated)
        else:
            append_jsonl_durable(destination_path, record)
