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

        if self.failure_class is not None:
            if not isinstance(self.failure_class, str) or not self.failure_class:
                raise ValueError("failure_class must be a non-empty string or None")
                
        if self.verdict == "pass" and self.failure_class is not None:
            raise ValueError("verdict 'pass' cannot have a failure_class")

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
    schema_version: int = 1,
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
    unsigned = {
        key: value
        for key, value in record.items()
        if key != "record_hash"
    }
    return "sha256:" + hashlib.sha256(
        canonical_json_bytes({
            "domain": "zkaedi.online-outcome",
            "schema_version": 1,
            "value": unsigned,
        })
    ).hexdigest()

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
        raise TypeError("replay record must be a dictionary")
        
    abspath = os.path.abspath(path)
    directory = os.path.dirname(abspath)
    if directory:
        os.makedirs(directory, exist_ok=True)
        
    record_snapshot = copy.deepcopy(record)
    serialized = canonical_json_bytes(record_snapshot) + b"\n"
    
    with open(abspath, "a+b") as fh:
        lock_file_ex(fh)
        try:
            fh.seek(0)
            existing = fh.read()
            _, valid_end = read_replay_prefix(existing)
            
            if valid_end < len(existing):
                fh.seek(valid_end)
                fh.truncate()
                fh.flush()
                os.fsync(fh.fileno())
                
            # If the existing file did not end with a newline, append one first
            fh.seek(0, os.SEEK_END)
            truncated_bytes = existing[:valid_end]
            if truncated_bytes and not truncated_bytes.endswith(b"\n"):
                fh.write(b"\n")
                
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
    payload["schema_version"] = 1
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
        schema_version=1
    )
    payload["record_hash"] = compute_record_hash(payload)
    append_jsonl_durable(path, payload)

def load_unique_records(path: str) -> ReplayLoadResult:
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
        # Legacy/migration check
        if "schema_version" not in record or record.get("schema_version") == 0:
            if "dedup_hash" in record or "record_hash" in record:
                raise ValueError(f"record {index} has a hash but has invalid/missing schema_version")
            unique[f"legacy:{index}"] = record
            continue

        if "dedup_hash" not in record:
            raise ValueError(f"record {index} is missing dedup_hash")

        dedup_hash = record["dedup_hash"]
        if not isinstance(dedup_hash, str):
            raise ValueError(f"record {index} dedup_hash must be a string")

        if not dedup_hash:
            raise ValueError(f"record {index} dedup_hash must not be empty")

        if not is_sha256_identifier(dedup_hash):
            raise ValueError(f"record {index} dedup_hash is malformed")

        schema_version = record["schema_version"]
        if isinstance(schema_version, bool) or not isinstance(schema_version, int):
            raise ValueError(f"record {index} schema_version must be an integer")

        if schema_version != 1:
            raise ValueError(f"record {index} has unsupported schema_version: {schema_version}")

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

        if dedup_hash in unique:
            skipped_dup += 1
        else:
            unique[dedup_hash] = record

    return ReplayLoadResult(
        records=list(unique.values()),
        tail_record_skipped=skipped_torn,
        skipped_duplicate_count=skipped_dup,
        tail_missing_newline=unterminated
    )
