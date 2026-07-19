import json
import os
import time
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

def append_jsonl_durable(path: str, record: dict[str, Any]) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "a", encoding="utf-8") as fh:
        fh.write(json.dumps(record, sort_keys=True) + "\n")
        fh.flush()
        os.fsync(fh.fileno())

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
    
    # Hash for deduplication
    eval_hash_basis = f"{outcome.prompt}|{outcome.completion}|harness-0.1.0"
    payload["dedup_hash"] = hashlib_md5(eval_hash_basis)
    
    append_jsonl_durable(path, payload)

def hashlib_md5(val: str) -> str:
    import hashlib
    return hashlib.md5(val.encode()).hexdigest()
