"""zkaedi-lab runner (Milestone A). The only door.

Agents never execute themselves. This harness:
  1. verifies candidate identity (content-addressed),
  2. executes the evaluator on the candidate config inside Tier-0 containment,
  3. captures stdout/stderr to runner-private files and hashes them,
  4. determines the verdict itself (the candidate's words are never evidence),
  5. writes the receipt to the append-only ledger AFTER the candidate exits.

Runner exit codes (for shell gating):
  0  = candidate ran and PASSed (evaluator exit 0)
  10 = candidate ran and FAILed (nonzero exit, incl. crash signals)
  11 = candidate hit the wall timeout
  20+= runner-side error, no receipt written (never mistakable for a verdict)
"""
from __future__ import annotations

import argparse
import json
import os
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from lineage.candidate_id import verify_candidate
from lineage.canonicalize import sha256_hex
from runner.receipt_writer import Ledger
from runner.sandbox_policy import (
    load_policy, make_preexec, sanitized_env, wrap_network_isolation,
)

RUNNER_VERSION = "sha256:dev-0.1.0"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def utc() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def pick_tmpfs_root() -> tuple[str, str]:
    """Prefer real tmpfs so candidate writes never touch disk."""
    if os.path.isdir("/dev/shm") and os.access("/dev/shm", os.W_OK):
        return "/dev/shm", "tmpfs"
    return tempfile.gettempdir(), "tempdir-NOT-tmpfs"


def run(candidate_path: str, policy_path: str, task_path: str | None,
        ledger_path: str) -> int:
    # ---- load & verify inputs (runner-side, before anything executes) ------
    with open(candidate_path, "rb") as fh:
        candidate = json.load(fh)
    cid = verify_candidate(candidate)
    policy = load_policy(policy_path)
    run_id = str(uuid.uuid4())

    tmp_root, fs_label = pick_tmpfs_root()
    workdir = tempfile.mkdtemp(prefix=f"zk-{run_id[:8]}-", dir=tmp_root)
    run_dir = os.path.join(REPO, "runs", run_id)  # runner-private evidence dir
    os.makedirs(run_dir)

    try:
        # candidate sees exactly: task input, its own config, tool schema
        with open(os.path.join(workdir, "candidate.json"), "wb") as fh:
            fh.write(json.dumps(candidate, sort_keys=True).encode())
        if task_path:
            shutil.copy(task_path, os.path.join(workdir, "task.json"))

        argv = list(policy["evaluator_argv"])
        argv = [arg.replace("/home/claude/zkaedi-lab", REPO) for arg in argv]
        argv = argv + [os.path.join(workdir, "candidate.json")]
        argv, net_label = wrap_network_isolation(argv, policy)

        out_path = os.path.join(run_dir, "stdout")
        err_path = os.path.join(run_dir, "stderr")
        limits = policy["limits"]
        usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
        started = utc()
        t0 = time.monotonic()

        with open(out_path, "wb") as out, open(err_path, "wb") as err:
            proc = subprocess.Popen(
                argv, cwd=workdir, env=sanitized_env(workdir),
                stdout=out, stderr=err, stdin=subprocess.DEVNULL,
                preexec_fn=make_preexec(limits),
            )
            timed_out = False
            try:
                proc.wait(timeout=limits["wall_seconds"])
            except subprocess.TimeoutExpired:
                timed_out = True
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()

        wall = time.monotonic() - t0
        ub, ua = usage_before, resource.getrusage(resource.RUSAGE_CHILDREN)
        exit_code = proc.returncode

        # ---- verdict: decided by the runner, from evidence only ------------
        if timed_out:
            verdict, runner_exit = "timeout", 11
        elif exit_code == 0:
            verdict, runner_exit = "pass", 0
        else:
            verdict, runner_exit = "fail", 10

        with open(out_path, "rb") as fh:
            stdout_sha = sha256_hex(fh.read())
        with open(err_path, "rb") as fh:
            stderr_sha = sha256_hex(fh.read())

        receipt = {
            "run_id": run_id,
            "candidate_id": cid,
            "parent_id": candidate["parent_id"],
            "runner_version": RUNNER_VERSION,
            "started_at": started,
            "finished_at": utc(),
            "exit_code": exit_code,
            "stdout_sha256": stdout_sha,
            "stderr_sha256": stderr_sha,
            "artifacts": {"stdout": out_path, "stderr": err_path},
            "resource_usage": {
                "wall_seconds": round(wall, 3),
                "cpu_seconds": round((ua.ru_utime - ub.ru_utime)
                                     + (ua.ru_stime - ub.ru_stime), 3),
                "max_rss_bytes": ua.ru_maxrss * 1024,
            },
            "sandbox_policy": {
                "tier": policy["tier"],
                "network": policy["network"],
                "network_enforcement": net_label,
                "filesystem": fs_label,
                "limits_applied": limits,
            },
            "verdict": verdict,
        }

        # receipt written by the runner only, after execution; then frozen
        receipt_path = os.path.join(run_dir, "receipt.json")
        with open(receipt_path, "wb") as fh:
            fh.write(json.dumps(receipt, sort_keys=True, indent=1).encode())
        os.chmod(receipt_path, 0o444)
        entry_sha = Ledger(ledger_path).append(receipt)

        print(json.dumps({"run_id": run_id, "candidate_id": cid,
                          "verdict": verdict, "exit_code": exit_code,
                          "ledger_entry_sha256": entry_sha,
                          "receipt": receipt_path}))
        return runner_exit
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("candidate")
    ap.add_argument("--policy", default=os.path.join(REPO, "policies", "tier0.json"))
    ap.add_argument("--task", default=None)
    ap.add_argument("--ledger",
                    default=os.path.join(REPO, "receipts", "append-only", "ledger.jsonl"))
    a = ap.parse_args()
    try:
        return run(a.candidate, a.policy, a.task, a.ledger)
    except Exception as exc:  # runner-side error: loud, receipt-less, distinct
        print(f"RUNNER-ERROR: {type(exc).__name__}: {exc}", file=sys.stderr)
        return 20


if __name__ == "__main__":
    sys.exit(main())
