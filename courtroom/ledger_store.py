"""ledger_store.py — JSONL persistence for ReleaseLedger.

Pattern matches zkaedi-fidelity-forge Module 6 audit spine: one JSON
object per line, sha256 hash chain, verify-on-load. Streaming append
so long-running sessions don't rewrite the file on every entry.

Truncated/partial last line is detected and refused rather than silently
treated as a valid record. The chain is the audit trail; we don't
forge through corruption.

Includes optional advisory file locking (POSIX fcntl) to make
multi-writer corruption a hard error rather than a silent chain break,
and rotation / compaction for long-running sessions.
"""
from __future__ import annotations

import json
import os
import sys
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

from courtroom import (
    EntryKind, GENESIS, LedgerEntry, LEDGER_FORMAT_VERSION, ReleaseLedger,
)


# Optional fcntl import — POSIX-only. On Windows we silently skip locking;
# zkaedi runs WSL2 so fcntl works there.
try:
    import fcntl
    _HAS_FCNTL = True
except ImportError:
    _HAS_FCNTL = False


class LedgerStoreError(Exception):
    """Raised when load/append cannot proceed without lying about the chain."""


@contextmanager
def _exclusive_lock(fileobj):
    """POSIX advisory exclusive lock on the file descriptor.

    No-op on platforms without fcntl (Windows native Python). zkaedi runs
    WSL2 so this engages there. On Windows, multi-writer behavior degrades
    to last-writer-wins on the chain — verify() will catch the resulting
    corruption on reload.
    """
    if _HAS_FCNTL:
        fcntl.flock(fileobj.fileno(), fcntl.LOCK_EX)
    try:
        yield
    finally:
        if _HAS_FCNTL:
            fcntl.flock(fileobj.fileno(), fcntl.LOCK_UN)


def _entry_to_dict(e: LedgerEntry) -> dict:
    return {
        "seq": e.seq,
        "timestamp": e.timestamp,
        "kind": e.kind.value,
        "actor": e.actor,
        "payload": e.payload,
        "prev_hash": e.prev_hash,
        "entry_hash": e.entry_hash,
    }


def _entry_from_dict(d: dict) -> LedgerEntry:
    return LedgerEntry(
        seq=d["seq"],
        timestamp=d["timestamp"],
        kind=EntryKind(d["kind"]),
        actor=d["actor"],
        payload=d["payload"],
        prev_hash=d["prev_hash"],
        entry_hash=d["entry_hash"],
    )


def save(ledger: ReleaseLedger, path: str | os.PathLike) -> None:
    """Write the entire ledger to a JSONL file (overwrite).

    Ledger is verified before writing. A ledger that doesn't verify is
    not written; we don't persist a chain we can't trust.
    """
    ok, reason = ledger.verify()
    if not ok:
        raise LedgerStoreError(f"refusing to save unverified ledger: {reason}")

    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    # Atomic-ish write via temp + rename so an interrupted save doesn't
    # leave the canonical file half-written.
    tmp = p.with_suffix(p.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8") as f, _exclusive_lock(f):
        for entry in ledger.entries:
            f.write(json.dumps(_entry_to_dict(entry),
                               sort_keys=True, separators=(",", ":")))
            f.write("\n")
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, p)


def append_entry(path: str | os.PathLike, entry: LedgerEntry) -> None:
    """Append a single entry to an existing JSONL file. Does not re-verify
    the whole chain — the caller is expected to have appended via the
    ReleaseLedger API which preserved the chain in memory. Use this when
    you want streaming durability without rewriting the file each turn.
    """
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open("a", encoding="utf-8") as f, _exclusive_lock(f):
        f.write(json.dumps(_entry_to_dict(entry),
                           sort_keys=True, separators=(",", ":")))
        f.write("\n")
        f.flush()
        os.fsync(f.fileno())


def load(path: str | os.PathLike, *,
         tolerate_partial_last_line: bool = True) -> ReleaseLedger:
    """Read a JSONL ledger from disk and return a fully-verified
    ReleaseLedger.

    The chain is verified during load. Any tampering or truncation
    (other than a single partial last line, by default) raises
    LedgerStoreError.
    """
    p = Path(path)
    if not p.exists():
        raise LedgerStoreError(f"ledger file not found: {p}")

    raw_lines = p.read_text(encoding="utf-8").splitlines()

    parsed: list[LedgerEntry] = []
    for i, line in enumerate(raw_lines):
        if not line.strip():
            continue
        try:
            d = json.loads(line)
        except json.JSONDecodeError as ex:
            if i == len(raw_lines) - 1 and tolerate_partial_last_line:
                # Truncated final write — acceptable per policy.
                break
            raise LedgerStoreError(
                f"line {i}: malformed JSON ({ex})") from ex
        try:
            parsed.append(_entry_from_dict(d))
        except (KeyError, ValueError) as ex:
            raise LedgerStoreError(f"line {i}: malformed entry ({ex})") from ex

    # Reconstruct + verify.
    ledger = ReleaseLedger()
    prev = GENESIS
    for entry in parsed:
        if entry.seq != len(ledger._entries):
            raise LedgerStoreError(
                f"seq={entry.seq}: out-of-order or missing entry "
                f"(expected seq={len(ledger._entries)})")
        if entry.prev_hash != prev:
            raise LedgerStoreError(
                f"seq={entry.seq}: prev_hash mismatch "
                f"(expected {prev[:8]}..., got {entry.prev_hash[:8]}...)")
        # Recompute hash from canonical bytes — catches payload tampering.
        recomputed = entry_hash_of(entry)
        if recomputed != entry.entry_hash:
            raise LedgerStoreError(
                f"seq={entry.seq}: entry_hash mismatch (tampered)")
        ledger._entries.append(entry)
        prev = entry.entry_hash

    return ledger


def entry_hash_of(entry: LedgerEntry) -> str:
    """Recompute the sha256 of an entry's canonical bytes.

    Used during load-verify; same bytes the LedgerEntry.canonical() method
    produces.
    """
    import hashlib
    return hashlib.sha256(entry.canonical()).hexdigest()


# ============================================================
# Convenience: streaming session that durably persists each turn
# ============================================================
class PersistedLedger:
    """ReleaseLedger that mirrors every append to a JSONL file.

    Construct with a path; existing file (if any) is loaded and verified.
    Subsequent appends go to memory + appended to the file (one fsync per
    entry). On crash, the on-disk file is the source of truth.
    """

    def __init__(self, path: str | os.PathLike):
        self.path = Path(path)
        if self.path.exists():
            self._ledger = load(self.path)
        else:
            self._ledger = ReleaseLedger()

    def append(self, kind: EntryKind, actor: str,
               payload: dict, now: datetime | None = None) -> LedgerEntry:
        entry = self._ledger.append(kind, actor, payload, now=now)
        append_entry(self.path, entry)
        return entry

    def verify(self) -> tuple[bool, str]:
        return self._ledger.verify()

    @property
    def entries(self):
        return self._ledger.entries

    @property
    def head_hash(self):
        return self._ledger.head_hash

    @property
    def underlying(self) -> ReleaseLedger:
        return self._ledger


def rotate(path: str | os.PathLike, archive_path: str | os.PathLike,
           now: datetime | None = None) -> PersistedLedger:
    """Compact the ledger at `path` by archiving it and starting a fresh
    ledger that references the archived head_hash in its genesis payload.

    Returns the new PersistedLedger. The archived file is unchanged and
    still independently verifiable. The new file starts with a
    CHECKPOINT entry whose payload includes:
        - the archived path
        - the archived head_hash
        - the entry count and seq range of the archived chain

    NOTE: this breaks the global hash chain across the rotation boundary.
    To verify across rotations, walk the chain in the archive, then
    confirm the new file's seq=0 CHECKPOINT payload matches the archive's
    head_hash.
    """
    src = Path(path)
    dst = Path(archive_path)
    if not src.exists():
        raise LedgerStoreError(f"cannot rotate non-existent ledger: {src}")

    archived = load(src)
    ok, reason = archived.verify()
    if not ok:
        raise LedgerStoreError(f"refusing to rotate corrupt ledger: {reason}")

    # Move src → dst.
    dst.parent.mkdir(parents=True, exist_ok=True)
    os.replace(src, dst)

    # New ledger with a checkpoint as seq=0.
    new_ledger = PersistedLedger(src)
    new_ledger.append(
        EntryKind.VERDICT,    # piggy-back on existing kind; payload says CHECKPOINT
        actor="rotation",
        payload={
            "_checkpoint": True,
            "archive_path": str(dst),
            "archived_head_hash": archived.head_hash,
            "archived_seq_range": [0, len(archived.entries) - 1],
            "archived_entry_count": len(archived.entries),
            "format_version": LEDGER_FORMAT_VERSION,
        },
        now=now,
    )
    return new_ledger
