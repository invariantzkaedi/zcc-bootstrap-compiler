import json
import os
import time
import hashlib
import fcntl
from typing import Any
from dataclasses import dataclass, asdict

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
    
    # Environment version metadata
    harness_version: str
    evaluator_version: str
    policy_checkpoint: str
    sandbox_version: str

@dataclass(frozen=True)
class ReplayLoadResult:
    records: list[dict]
    skipped_torn_tail: bool
    skipped_duplicate_count: int

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

def compute_dedup_hash(
    prompt: str,
    completion: str,
    harness_version: str,
    evaluator_version: str,
    policy_checkpoint: str,
    sandbox_version: str,
) -> str:
    payload = json.dumps(
        {
            "prompt": prompt,
            "completion": completion,
            "harness_version": harness_version,
            "evaluator_version": evaluator_version,
            "policy_checkpoint": policy_checkpoint,
            "sandbox_version": sandbox_version,
        },
        sort_keys=True,
        separators=(",", ":"),
    )
    return "sha256:" + hashlib.sha256(payload.encode()).hexdigest()

def append_jsonl_durable(path: str, record: dict[str, Any]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a", encoding="utf-8") as fh:
        fcntl.flock(fh.fileno(), fcntl.LOCK_EX)
        try:
            fh.write(json.dumps(record, sort_keys=True) + "\n")
            fh.flush()
            os.fsync(fh.fileno())
        finally:
            fcntl.flock(fh.fileno(), fcntl.LOCK_UN)

    if hasattr(os, "O_DIRECTORY"):
        dir_fd = os.open(os.path.dirname(path), os.O_DIRECTORY)
        try:
            os.fsync(dir_fd)
        finally:
            os.close(dir_fd)

def record_online_outcome(path: str, outcome: OnlineOutcome) -> None:
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
    if not os.path.exists(path):
        return ReplayLoadResult(records=[], skipped_torn_tail=False, skipped_duplicate_count=0)

    with open(path, "r", encoding="utf-8") as fh:
        fcntl.flock(fh.fileno(), fcntl.LOCK_SH)
        try:
            lines = fh.readlines()
        finally:
            fcntl.flock(fh.fileno(), fcntl.LOCK_UN)

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
            if index == len(lines) - 1:
                skipped_torn = True
                continue
            raise

        dedup_hash = record.get("dedup_hash")
        if dedup_hash:
            if dedup_hash in unique:
                skipped_dup += 1
            else:
                unique[dedup_hash] = record

    return ReplayLoadResult(
        records=list(unique.values()),
        skipped_torn_tail=skipped_torn,
        skipped_duplicate_count=skipped_dup
    )
