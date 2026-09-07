from __future__ import annotations

import dataclasses
import hashlib
import json
import os
import threading
from enum import Enum
from pathlib import Path
from typing import Any, Iterable, Mapping
from uuid import UUID

from .models import CheckpointState, NexusFrame


def _jsonable(value: Any) -> Any:
    if dataclasses.is_dataclass(value):
        return {f.name: _jsonable(getattr(value, f.name)) for f in dataclasses.fields(value)}
    if isinstance(value, Enum):
        return value.value
    if isinstance(value, UUID):
        return str(value)
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, bytes):
        return {"__bytes_hex__": value.hex()}
    if isinstance(value, Mapping):
        return {str(k): _jsonable(v) for k, v in sorted(value.items(), key=lambda x: str(x[0]))}
    if isinstance(value, (tuple, list)):
        return [_jsonable(v) for v in value]
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    raise TypeError(f"unsupported value for canonical serialization: {type(value).__name__}")


def canonical_json_bytes(value: Any) -> bytes:
    """Return deterministic UTF-8 JSON suitable for hashing and persistence."""
    return json.dumps(
        _jsonable(value),
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def sha256_hex(value: Any) -> str:
    return hashlib.sha256(canonical_json_bytes(value)).hexdigest()


def lineage_hash(parent_lineage_hash: str | None, node_uuid: UUID) -> str:
    """H(parent_lineage_hash || node_uuid.bytes), with an empty genesis parent."""
    h = hashlib.sha256()
    if parent_lineage_hash:
        try:
            h.update(bytes.fromhex(parent_lineage_hash))
        except ValueError as exc:
            raise ValueError("parent_lineage_hash must be a SHA-256 hex digest") from exc
    h.update(node_uuid.bytes)
    return h.hexdigest()


class NexusDAGLedger:
    """
    Append-only JSONL DAG ledger plus atomic checkpoint payload store.

    Each ledger envelope contains a hash of the previous envelope and a hash of
    its payload. `verify()` recomputes both chains and verifies persisted
    checkpoint state/config hashes. Writes are serialized within the process and
    use O_APPEND + fsync. Checkpoint files are written to a temporary file,
    fsynced, and atomically replaced.
    """

    def __init__(self, root: str | Path) -> None:
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self.checkpoint_dir = self.root / "checkpoints"
        self.checkpoint_dir.mkdir(parents=True, exist_ok=True)
        self.ledger_path = self.root / "ledger.jsonl"
        self._lock = threading.RLock()

    @staticmethod
    def _frame_payload(frame: NexusFrame) -> dict[str, Any]:
        return _jsonable(frame)

    def _last_record_hash(self) -> str:
        if not self.ledger_path.exists() or self.ledger_path.stat().st_size == 0:
            return "0" * 64
        last = ""
        with self.ledger_path.open("r", encoding="utf-8") as fh:
            for line in fh:
                if line.strip():
                    last = line
        if not last:
            return "0" * 64
        try:
            return str(json.loads(last)["record_hash"])
        except (json.JSONDecodeError, KeyError) as exc:
            raise ValueError("ledger tail is corrupt; refusing to append") from exc

    def append_frame(self, frame: NexusFrame) -> str:
        payload = self._frame_payload(frame)
        with self._lock:
            previous_record_hash = self._last_record_hash()
            payload_hash = hashlib.sha256(canonical_json_bytes(payload)).hexdigest()
            envelope_without_hash = {
                "kind": "frame",
                "previous_record_hash": previous_record_hash,
                "payload_hash": payload_hash,
                "payload": payload,
            }
            record_hash = hashlib.sha256(canonical_json_bytes(envelope_without_hash)).hexdigest()
            envelope = dict(envelope_without_hash)
            envelope["record_hash"] = record_hash
            blob = canonical_json_bytes(envelope) + b"\n"
            fd = os.open(self.ledger_path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
            try:
                view = memoryview(blob)
                while view:
                    written = os.write(fd, view)
                    if written <= 0:
                        raise OSError("short append while writing ledger")
                    view = view[written:]
                os.fsync(fd)
            finally:
                os.close(fd)
            return record_hash

    def save_checkpoint(self, checkpoint: CheckpointState) -> Path:
        data = _jsonable(checkpoint)
        path = self.checkpoint_dir / f"{checkpoint.checkpoint_uuid}.json"
        temp = path.with_suffix(f".json.tmp-{os.getpid()}-{threading.get_ident()}")
        blob = canonical_json_bytes(data)
        fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        try:
            view = memoryview(blob)
            while view:
                n = os.write(fd, view)
                if n <= 0:
                    raise OSError("short write while saving checkpoint")
                view = view[n:]
            os.fsync(fd)
        finally:
            os.close(fd)
        os.replace(temp, path)
        # Best-effort directory durability on POSIX.
        if hasattr(os, "O_DIRECTORY"):
            try:
                dfd = os.open(self.checkpoint_dir, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(dfd)
                finally:
                    os.close(dfd)
            except OSError:
                pass
        return path

    def load_checkpoint(self, checkpoint_uuid: UUID) -> CheckpointState:
        path = self.checkpoint_dir / f"{checkpoint_uuid}.json"
        try:
            data = json.loads(path.read_text("utf-8"))
        except FileNotFoundError as exc:
            raise KeyError(f"checkpoint not found: {checkpoint_uuid}") from exc
        return CheckpointState(
            checkpoint_uuid=UUID(data["checkpoint_uuid"]),
            node_uuid=UUID(data["node_uuid"]),
            parent_uuid=UUID(data["parent_uuid"]) if data["parent_uuid"] else None,
            timestamp_ns=int(data["timestamp_ns"]),
            subsystem=str(data["subsystem"]),
            operation=str(data["operation"]),
            state=data["state"],
            config=data["config"],
            entropy=float(data["entropy"]),
            fitness=float(data["fitness"]),
            state_hash=str(data["state_hash"]),
            config_hash=str(data["config_hash"]),
            lineage_hash=str(data["lineage_hash"]),
        )

    def iter_frames(self) -> Iterable[NexusFrame]:
        if not self.ledger_path.exists():
            return
        with self.ledger_path.open("r", encoding="utf-8") as fh:
            for line_no, line in enumerate(fh, 1):
                if not line.strip():
                    continue
                try:
                    env = json.loads(line)
                    p = env["payload"]
                    yield NexusFrame(
                        node_uuid=UUID(p["node_uuid"]),
                        parent_uuid=UUID(p["parent_uuid"]) if p["parent_uuid"] else None,
                        checkpoint_uuid=UUID(p["checkpoint_uuid"]),
                        timestamp_ns=int(p["timestamp_ns"]),
                        subsystem=str(p["subsystem"]),
                        operation=str(p["operation"]),
                        entropy=float(p["entropy"]),
                        drift_vector=tuple(float(x) for x in p["drift_vector"]),
                        drift_score=float(p["drift_score"]),
                        drift_class=str(p["drift_class"]),
                        confidence=float(p["confidence"]),
                        latency_ns=int(p["latency_ns"]),
                        memory_delta_bytes=int(p["memory_delta_bytes"]),
                        fitness_delta=float(p["fitness_delta"]),
                        state_hash=str(p["state_hash"]),
                        config_hash=str(p["config_hash"]),
                        lineage_hash=str(p["lineage_hash"]),
                    )
                except (KeyError, ValueError, TypeError, json.JSONDecodeError) as exc:
                    raise ValueError(f"invalid ledger record at line {line_no}") from exc

    def find_frame_by_checkpoint(self, checkpoint_uuid: UUID) -> NexusFrame:
        for frame in self.iter_frames():
            if frame.checkpoint_uuid == checkpoint_uuid:
                return frame
        raise KeyError(f"frame not found for checkpoint: {checkpoint_uuid}")

    def find_frame_by_node(self, node_uuid: UUID) -> NexusFrame:
        for frame in self.iter_frames():
            if frame.node_uuid == node_uuid:
                return frame
        raise KeyError(f"frame not found for node: {node_uuid}")

    def verify(self) -> tuple[bool, tuple[str, ...]]:
        errors: list[str] = []
        previous = "0" * 64
        frames: list[NexusFrame] = []
        if self.ledger_path.exists():
            with self.ledger_path.open("r", encoding="utf-8") as fh:
                for line_no, line in enumerate(fh, 1):
                    if not line.strip():
                        continue
                    try:
                        env = json.loads(line)
                        supplied_record_hash = env["record_hash"]
                        without = dict(env)
                        without.pop("record_hash")
                        actual_record_hash = hashlib.sha256(canonical_json_bytes(without)).hexdigest()
                        if env.get("previous_record_hash") != previous:
                            errors.append(f"line {line_no}: previous_record_hash mismatch")
                        if supplied_record_hash != actual_record_hash:
                            errors.append(f"line {line_no}: record_hash mismatch")
                        payload_hash = hashlib.sha256(canonical_json_bytes(env["payload"])).hexdigest()
                        if env.get("payload_hash") != payload_hash:
                            errors.append(f"line {line_no}: payload_hash mismatch")
                        previous = supplied_record_hash
                    except Exception as exc:
                        errors.append(f"line {line_no}: invalid record: {exc}")
                        break
        try:
            frames = list(self.iter_frames())
        except Exception as exc:
            errors.append(f"frame parse failed: {exc}")
            frames = []

        by_node = {f.node_uuid: f for f in frames}
        for frame in frames:
            if frame.parent_uuid is None:
                expected_lineage = lineage_hash(None, frame.node_uuid)
            else:
                parent = by_node.get(frame.parent_uuid)
                if parent is None:
                    errors.append(f"node {frame.node_uuid}: missing parent {frame.parent_uuid}")
                    continue
                expected_lineage = lineage_hash(parent.lineage_hash, frame.node_uuid)
            if expected_lineage != frame.lineage_hash:
                errors.append(f"node {frame.node_uuid}: lineage_hash mismatch")

            try:
                cp = self.load_checkpoint(frame.checkpoint_uuid)
            except KeyError:
                # Decision/audit frames intentionally need not carry a persisted state file.
                continue
            if sha256_hex(cp.state) != cp.state_hash or cp.state_hash != frame.state_hash:
                errors.append(f"checkpoint {cp.checkpoint_uuid}: state_hash mismatch")
            if sha256_hex(cp.config) != cp.config_hash or cp.config_hash != frame.config_hash:
                errors.append(f"checkpoint {cp.checkpoint_uuid}: config_hash mismatch")
            if cp.lineage_hash != frame.lineage_hash:
                errors.append(f"checkpoint {cp.checkpoint_uuid}: lineage_hash mismatch")

        return (not errors, tuple(errors))
