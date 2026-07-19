"""Tier policy loading and in-process enforcement primitives (Milestone C).

Enforcement layers, honestly labeled:
  ENFORCED HERE:  rlimits (cpu/mem/fsize/nofile), sanitized env, tmpfs cwd,
                  hard wall timeout + process-group kill, network namespace
                  isolation via `unshare -n` when available.
  RUNTIME'S JOB:  host-mount denial, UID separation, pid-limit hard walls
                  (podman --network=none --pids-limit ... :ro mounts).
The receipt records which layers were actually applied — no silent gaps.
"""
from __future__ import annotations

import json
import os
import resource
import shutil

REQUIRED = ("tier", "network", "filesystem", "limits", "evaluator_argv")


class PolicyError(Exception):
    pass


def load_policy(path: str) -> dict:
    with open(path, "rb") as fh:
        pol = json.load(fh)
    missing = [k for k in REQUIRED if k not in pol]
    if missing:
        raise PolicyError(f"policy {path} missing keys: {missing}")
    lim = pol["limits"]
    for k in ("cpu_seconds", "memory_bytes", "wall_seconds", "fsize_bytes", "nofile"):
        if not isinstance(lim.get(k), int) or lim[k] <= 0:
            raise PolicyError(f"policy limit {k} must be a positive int")
    if pol["network"] not in (True, False):
        raise PolicyError("policy.network must be boolean")
    return pol


def sanitized_env(workdir: str) -> dict:
    """Nothing inherited. No secrets, no host paths, no venv, no git/ssh."""
    return {
        "PATH": "/usr/bin:/bin",
        "HOME": workdir,
        "TMPDIR": workdir,
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
        "PYTHONDONTWRITEBYTECODE": "1",
        "PYTHONHASHSEED": "0",
    }


def make_preexec(limits: dict):
    """Applied in the child between fork and exec. Failure here aborts the
    child loudly (subprocess raises) — limits are never silently skipped."""
    def _preexec():
        os.setsid()  # own process group -> killable as a group on timeout
        cpu = limits["cpu_seconds"]
        resource.setrlimit(resource.RLIMIT_CPU, (cpu, cpu + 1))
        mem = limits["memory_bytes"]
        resource.setrlimit(resource.RLIMIT_AS, (mem, mem))
        fsz = limits["fsize_bytes"]
        resource.setrlimit(resource.RLIMIT_FSIZE, (fsz, fsz))
        nof = limits["nofile"]
        resource.setrlimit(resource.RLIMIT_NOFILE, (nof, nof))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    return _preexec


def netns_available() -> bool:
    """True if `unshare -n` or `unshare -rn` can actually create a network namespace here."""
    if shutil.which("unshare") is None:
        return False
    return os.system("unshare -n true >/dev/null 2>&1") == 0 or os.system("unshare -rn true >/dev/null 2>&1") == 0


def wrap_network_isolation(argv: list[str], policy: dict) -> tuple[list[str], str]:
    """Return (argv, network_enforcement_label). Fails closed: if the policy
    demands no network and no enforcement mechanism exists, raise."""
    if policy["network"]:
        return argv, "network-allowed-by-policy"
    if shutil.which("unshare") is not None:
        if os.system("unshare -n true >/dev/null 2>&1") == 0:
            return ["unshare", "-n", "--"] + argv, "netns-unshare"
        if os.system("unshare -rn true >/dev/null 2>&1") == 0:
            return ["unshare", "-rn", "--"] + argv, "netns-unshare"
    raise PolicyError(
        "policy requires network=false but no isolation mechanism available; "
        "refusing to run un-isolated (run under podman --network=none instead)"
    )
