import json
import os
import time
import hashlib
import secrets
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

@dataclass(frozen=True)
class ReplayLoadResult:
    records: list[dict]
    skipped_torn_tail: bool
    skipped_duplicate_count: int
    unterminated_tail: bool

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

def compute_dedup_hash(
    prompt: str,
    completion: str,
    harness_version: str,
    evaluator_version: str,
    policy_checkpoint: str,
    sandbox_version: str,
) -> str:
    payload = {
        "prompt": prompt,
        "completion": completion,
        "harness_version": harness_version,
        "evaluator_version": evaluator_version,
        "policy_checkpoint": policy_checkpoint,
        "sandbox_version": sandbox_version,
    }
    return "sha256:" + hashlib.sha256(canonical_json_bytes(payload)).hexdigest()

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

def append_jsonl_durable(path: str, record: dict[str, Any]) -> None:
    abspath = os.path.abspath(path)
    directory = os.path.dirname(abspath)
    if directory:
        os.makedirs(directory, exist_ok=True)
        
    serialized = canonical_json_bytes(record) + b"\n"
    with open(abspath, "ab") as fh:
        lock_file_ex(fh)
        try:
            fh.write(serialized)
            fh.flush()
            os.fsync(fh.fileno())
        finally:
            unlock_file(fh)

    if hasattr(os, "O_DIRECTORY"):
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
    payload["desirable"] = derive_desirability(outcome)
    payload["trainable"] = should_train_on(outcome)
    payload["recorded_at_unix"] = time.time()
    payload["dedup_hash"] = compute_dedup_hash(
        outcome.prompt,
        outcome.completion,
        outcome.harness_version,
        outcome.evaluator_version,
        outcome.policy_checkpoint,
        outcome.sandbox_version
    )
    append_jsonl_durable(path, payload)

def load_unique_records(path: str) -> ReplayLoadResult:
    abspath = os.path.abspath(path)
    if not os.path.exists(abspath):
        return ReplayLoadResult(records=[], skipped_torn_tail=False, skipped_duplicate_count=0, unterminated_tail=False)

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

    lines = content_bytes.decode("utf-8").splitlines()
    unique: dict[str, dict] = {}
    skipped_torn = False
    skipped_dup = 0
    
    for index, line in enumerate(lines):
        line_str = line.strip()
        if not line_str:
            continue

        try:
            record = json.loads(line_str)
        except json.JSONDecodeError:
            is_final = (index == len(lines) - 1)
            if is_final and unterminated:
                skipped_torn = True
                continue
            raise

        dedup_hash = record.get("dedup_hash")
        if not dedup_hash:
            # Preserve legacy record
            synthetic_key = f"legacy:{index}"
            unique[synthetic_key] = record
            continue

        # Re-verify the dedup hash to protect against manipulation
        try:
            expected = compute_dedup_hash(
                record["prompt"],
                record["completion"],
                record["harness_version"],
                record["evaluator_version"],
                record["policy_checkpoint"],
                record["sandbox_version"],
            )
            if not secrets.compare_digest(dedup_hash, expected):
                raise ValueError(f"record {index} has an invalid dedup hash")
        except KeyError:
            raise ValueError(f"record {index} lacks required fields for dedup validation")

        if dedup_hash in unique:
            skipped_dup += 1
        else:
            unique[dedup_hash] = record

    return ReplayLoadResult(
        records=list(unique.values()),
        skipped_torn_tail=skipped_torn,
        skipped_duplicate_count=skipped_dup,
        unterminated_tail=unterminated
    )
