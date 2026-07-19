"""Append-only hash-chained receipt ledger (Milestone A evidence plane).

Ledger format: one canonical-JSON line per entry:
    {"prev_sha256": <hex of previous raw line, genesis = 64 zeros>,
     "seq": <int>, "receipt": {...}}

Tamper model in this repo: DETECTION via chain verification. PREVENTION of
same-UID writes is the container runtime's job (podman :ro mount / separate
UID); do not claim it from this module.

The candidate never calls this module: the runner writes receipts after the
candidate process has exited. Per-run receipt files are chmod 0444.
"""
from __future__ import annotations

import json
import os
import sys
from typing import Iterator, Optional

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from lineage.canonicalize import canonical_bytes, sha256_hex

GENESIS = "0" * 64


class LedgerError(Exception):
    pass


class Ledger:
    def __init__(self, path: str):
        self.path = path
        os.makedirs(os.path.dirname(path), exist_ok=True)

    # -- read side -----------------------------------------------------------
    def _raw_lines(self) -> list[bytes]:
        if not os.path.exists(self.path):
            return []
        with open(self.path, "rb") as fh:
            return [ln for ln in fh.read().split(b"\n") if ln.strip()]

    def entries(self) -> Iterator[dict]:
        for ln in self._raw_lines():
            yield json.loads(ln)

    def tip(self) -> tuple[str, int]:
        """(sha256 of last raw line, next seq). Genesis if empty."""
        lines = self._raw_lines()
        if not lines:
            return GENESIS, 0
        last = json.loads(lines[-1])
        return sha256_hex(lines[-1]), last["seq"] + 1

    # -- write side ----------------------------------------------------------
    def append(self, receipt: dict) -> str:
        """Append one receipt; returns sha256 of the written line."""
        prev, seq = self.tip()
        entry = {"prev_sha256": prev, "seq": seq, "receipt": receipt}
        line = canonical_bytes(entry)
        if b"\n" in line:
            raise LedgerError("newline in canonical entry")
        fd = os.open(self.path, os.O_APPEND | os.O_CREAT | os.O_WRONLY, 0o644)
        try:
            os.write(fd, line + b"\n")
            os.fsync(fd)
        finally:
            os.close(fd)
        return sha256_hex(line)

    # -- verification --------------------------------------------------------
    def verify(self) -> tuple[bool, int, Optional[int]]:
        """Recompute the whole chain. Returns (ok, entry_count, first_bad_seq)."""
        prev = GENESIS
        count = 0
        for i, ln in enumerate(self._raw_lines()):
            try:
                entry = json.loads(ln)
            except json.JSONDecodeError:
                return False, count, i
            if entry.get("seq") != i or entry.get("prev_sha256") != prev:
                return False, count, i
            if canonical_bytes(entry) != ln:  # non-canonical line = tampered
                return False, count, i
            prev = sha256_hex(ln)
            count += 1
        return True, count, None

    def find_run(self, run_id: str) -> Optional[dict]:
        for entry in self.entries():
            if entry["receipt"].get("run_id") == run_id:
                return entry["receipt"]
        return None

    def runs_for_candidate(self, candidate_id: str) -> list[dict]:
        return [e["receipt"] for e in self.entries()
                if e["receipt"].get("candidate_id") == candidate_id]


if __name__ == "__main__":
    import sys
    ok, n, bad = Ledger(sys.argv[1]).verify()
    print(json.dumps({"ok": ok, "entries": n, "first_bad_seq": bad}))
    sys.exit(0 if ok else 2)
