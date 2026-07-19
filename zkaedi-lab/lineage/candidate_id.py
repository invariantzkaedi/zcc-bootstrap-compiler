"""Deterministic candidate identity (Milestone B).

candidate_id = sha256(domain || canonical({7 identity fields}))
Any field change, including parent or mutation description, yields a new id.
"""
from __future__ import annotations

import sys

from lineage.canonicalize import DOMAIN_CANDIDATE, domain_hash

IDENTITY_FIELDS = (
    "canonical_prompt",
    "tool_policy",
    "scaffold",
    "eval_config",
    "parent_id",
    "mutation_description",
    "harness_version",
)

GENESIS_PARENT = "sha256:" + "0" * 64


def compute_candidate_id(candidate: dict) -> str:
    missing = [f for f in IDENTITY_FIELDS if f not in candidate]
    if missing:
        raise ValueError(f"candidate missing identity fields: {missing}")
    identity = {f: candidate[f] for f in IDENTITY_FIELDS}
    pid = identity["parent_id"]
    if not (isinstance(pid, str) and pid.startswith("sha256:") and len(pid) == 71):
        raise ValueError(f"malformed parent_id: {pid!r}")
    return domain_hash(DOMAIN_CANDIDATE, identity)


def verify_candidate(candidate: dict) -> str:
    """Recompute id; if the candidate carries one, it must match. Returns the id."""
    cid = compute_candidate_id(candidate)
    claimed = candidate.get("candidate_id")
    if claimed is not None and claimed != cid:
        raise ValueError(f"candidate_id mismatch: claimed {claimed} computed {cid}")
    return cid


if __name__ == "__main__":
    import json
    with open(sys.argv[1], "rb") as fh:
        print(verify_candidate(json.load(fh)))
