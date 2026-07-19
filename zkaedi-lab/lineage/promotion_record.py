"""Promotion records (Milestone B): authority separated from evidence.

A promotion is only issuable when every cited run_id resolves in the VERIFIED
ledger to a pass-verdict receipt for the promoted candidate. The record is
HMAC-SHA256 signed (key file outside candidate reach; upgrade path: ed25519 +
hardware key — the record format already carries `signed_by`).

Verification re-checks BOTH the signature and the evidence against the ledger,
so a stolen signature over vanished evidence still fails.
"""
from __future__ import annotations

import hmac
import hashlib
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lineage.canonicalize import DOMAIN_PROMOTION, canonical_bytes
from runner.receipt_writer import Ledger


class PromotionError(Exception):
    pass


def _sign(record_wo_sig: dict, key: bytes) -> str:
    return hmac.new(key, DOMAIN_PROMOTION + canonical_bytes(record_wo_sig),
                    hashlib.sha256).hexdigest()


def _check_evidence(candidate_id: str, run_ids: list[str], ledger: Ledger) -> None:
    ok, _, bad = ledger.verify()
    if not ok:
        raise PromotionError(f"ledger chain broken at seq {bad}; no evidence is valid")
    if not run_ids:
        raise PromotionError("promotion requires at least one evidence run")
    for rid in run_ids:
        receipt = ledger.find_run(rid)
        if receipt is None:
            raise PromotionError(f"evidence run {rid} not in ledger")
        if receipt.get("candidate_id") != candidate_id:
            raise PromotionError(f"evidence run {rid} is for a different candidate")
        if receipt.get("verdict") != "pass":
            raise PromotionError(f"evidence run {rid} verdict is "
                                 f"{receipt.get('verdict')!r}, not pass")


def create(candidate_id: str, from_tier: int, to_tier: int,
           evidence_run_ids: list[str], policy_version: str,
           signed_by: str, key: bytes, ledger: Ledger) -> dict:
    _check_evidence(candidate_id, evidence_run_ids, ledger)
    record = {
        "candidate_id": candidate_id,
        "from_tier": from_tier,
        "to_tier": to_tier,
        "decision": "promote",
        "evidence_run_ids": sorted(evidence_run_ids),
        "policy_version": policy_version,
        "signed_by": signed_by,
        "issued_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    record["signature"] = _sign(record, key)
    return record


def verify(record: dict, key: bytes, ledger: Ledger) -> bool:
    body = {k: v for k, v in record.items() if k != "signature"}
    if not hmac.compare_digest(_sign(body, key), record.get("signature", "")):
        raise PromotionError("signature invalid")
    _check_evidence(record["candidate_id"], record["evidence_run_ids"], ledger)
    return True


if __name__ == "__main__":
    # cli: create <candidate_id> <run_id> <keyfile> <ledger>  -> record json / exit 5 on refusal
    _, cid, rid, keyfile, ledger_path = sys.argv
    with open(keyfile, "rb") as fh:
        key = fh.read()
    try:
        rec = create(cid, 0, 1, [rid], "sha256:policy-dev", "zkaedi", key,
                     Ledger(ledger_path))
        print(json.dumps(rec, sort_keys=True))
    except PromotionError as exc:
        print(f"PROMOTION-REFUSED: {exc}", file=sys.stderr)
        sys.exit(5)
