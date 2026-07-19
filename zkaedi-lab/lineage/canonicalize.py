"""Canonical serialization for zkaedi-lab.

Every hash in the system (candidate ids, ledger chain, signatures) is computed
over these bytes. Rules: sorted keys, minimal separators, UTF-8, no NaN/Inf.
"""
from __future__ import annotations

import hashlib
import json
from typing import Any

DOMAIN_CANDIDATE = b"zkaedi-lab/candidate/v1\x00"
DOMAIN_LEDGER = b"zkaedi-lab/ledger-entry/v1\x00"
DOMAIN_PROMOTION = b"zkaedi-lab/promotion/v1\x00"


def canonical_bytes(obj: Any) -> bytes:
    """Deterministic UTF-8 JSON bytes. Raises ValueError on NaN/Inf."""
    return json.dumps(
        obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def domain_hash(domain: bytes, obj: Any) -> str:
    """sha256:<hex> with domain separation so cross-type collisions are impossible."""
    return "sha256:" + sha256_hex(domain + canonical_bytes(obj))
