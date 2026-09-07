"""
nexus.checkpoint_engine - Universal Telemetry & Symbolic Drift Engine
=====================================================================
Computes multi-dimensional symbolic drift tensors across state embeddings,
probability distributions, entropy, and fitness. Classifies stability regimes
and writes immutable, cryptographically attested checkpoints to the DAG.
"""

from __future__ import annotations

import json
import math
import hashlib
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple, Union
from uuid import UUID, uuid4

from .models import DriftClass, NexusFrame, CheckpointPayload


def _canonical_hash(obj: Any) -> str:
    """Computes deterministic SHA-256 hash of any JSON-serializable structure or bytes."""
    if isinstance(obj, bytes):
        return hashlib.sha256(obj).hexdigest()
    if isinstance(obj, str):
        return hashlib.sha256(obj.encode("utf-8")).hexdigest()
    dumped = json.dumps(obj, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(dumped.encode("utf-8")).hexdigest()


def _l2_displacement(z1: Sequence[float], z2: Sequence[float]) -> float:
    """Computes Euclidean distance ||z_t - z_{t-1}||_2."""
    if not z1 or not z2:
        return 0.0
    dim = min(len(z1), len(z2))
    s = sum((z1[i] - z2[i]) ** 2 for i in range(dim))
    return math.sqrt(s)


def _normalize_probs(probs: Sequence[float], eps: float = 1e-9) -> List[float]:
    """Ensures a probability vector is non-negative and sums to 1."""
    if not probs:
        return [1.0]
    p = [max(float(x), eps) for x in probs]
    total = sum(p)
    return [x / total for x in p]


def _kl_divergence(p: Sequence[float], q: Sequence[float], eps: float = 1e-9) -> float:
    """
    Computes D_KL(P || Q) = sum_i P_i * ln(P_i / Q_i).
    Applies Laplace smoothing to prevent numerical divergence.
    """
    p_norm = _normalize_probs(p, eps)
    q_norm = _normalize_probs(q, eps)
    dim = min(len(p_norm), len(q_norm))
    kl = 0.0
    for i in range(dim):
        pi = p_norm[i]
        qi = q_norm[i]
        kl += pi * math.log(pi / qi)
    return max(0.0, float(kl))


def _shannon_entropy(probs: Sequence[float], eps: float = 1e-9) -> float:
    """Computes Shannon entropy H(P) = -sum_i P_i * ln(P_i)."""
    p_norm = _normalize_probs(probs, eps)
    ent = -sum(pi * math.log(pi) for pi in p_norm)
    return max(0.0, float(ent))


class NexusDAGLedger:
    """
    In-memory DAG representation backed by an append-only JSONL journal.
    Guarantees monotonic parent->child relationships and immutable state recovery.
    """

    def __init__(self, ledger_dir: Optional[Union[str, Path]] = None):
        self.ledger_dir = Path(ledger_dir) if ledger_dir else None
        if self.ledger_dir:
            self.ledger_dir.mkdir(parents=True, exist_ok=True)
            self.journal_file = self.ledger_dir / "nexus_frames.jsonl"
            self.checkpoints_file = self.ledger_dir / "nexus_checkpoints.jsonl"
        else:
            self.journal_file = None
            self.checkpoints_file = None

        self.frames: Dict[UUID, NexusFrame] = {}
        self.checkpoints: Dict[UUID, CheckpointPayload] = {}
        self.children_map: Dict[UUID, List[UUID]] = {}
        self.active_leaves: List[UUID] = []

    def append_frame(self, frame: NexusFrame, payload: Optional[CheckpointPayload] = None) -> None:
        """Stores frame and optional snapshot payload, linking into the DAG."""
        self.frames[frame.node_uuid] = frame

        if frame.parent_uuid:
            self.children_map.setdefault(frame.parent_uuid, []).append(frame.node_uuid)
            if frame.parent_uuid in self.active_leaves:
                self.active_leaves.remove(frame.parent_uuid)
        self.active_leaves.append(frame.node_uuid)

        if payload:
            self.checkpoints[payload.checkpoint_uuid] = payload

        # Persistent append-only logging if configured
        if self.journal_file:
            with open(self.journal_file, "a", encoding="utf-8") as f:
                f.write(json.dumps(frame.to_dict()) + "\n")
        if self.checkpoints_file and payload:
            with open(self.checkpoints_file, "a", encoding="utf-8") as f:
                f.write(payload.serialize() + "\n")

    def get_lineage(self, node_uuid: UUID) -> List[NexusFrame]:
        """Traces lineage backwards from node_uuid to root."""
        lineage = []
        curr = node_uuid
        while curr and curr in self.frames:
            f = self.frames[curr]
            lineage.append(f)
            curr = f.parent_uuid
        lineage.reverse()
        return lineage


def symbolic_drift_checkpoint_engine(
    subsystem: str,
    operation: str,
    current_state: Any,
    current_config: Dict[str, Any],
    current_embedding: Sequence[float],
    current_distribution: Sequence[float],
    current_fitness: float,
    parent_frame: Optional[NexusFrame] = None,
    parent_payload: Optional[CheckpointPayload] = None,
    ledger: Optional[NexusDAGLedger] = None,
    alpha: float = 0.35,
    beta: float = 0.30,
    gamma: float = 0.20,
    delta: float = 0.15,
    tau_s: float = 0.05,
    tau_d: float = 0.20,
    tau_c: float = 0.50,
    latency_ns: int = 0,
    memory_delta_bytes: int = 0,
    metadata: Optional[Dict[str, Any]] = None,
) -> Tuple[NexusFrame, CheckpointPayload]:
    """
    Universal telemetry and checkpoint primitive for the Nexus Sovereign Runtime.

    Evaluates:
      D_t(v) = alpha * ||z_t - z_{t-1}||_2
             + beta  * D_KL(P_t || P_{t-1})
             + gamma * |Delta H_t|
             + delta * |Delta F_t|

    Classifies:
      stabilize:  D_t < tau_s
      transition: tau_s <= D_t < tau_d
      drift:      tau_d <= D_t < tau_c
      critical:   D_t >= tau_c

    Returns:
      (immutable_nexus_frame, checkpoint_payload)
    """
    timestamp_ns = time.time_ns()
    node_uuid = uuid4()
    checkpoint_uuid = uuid4()

    # Calculate individual drift terms
    if parent_frame is not None and parent_payload is not None:
        prev_embedding = parent_payload.metadata.get("embedding", [])
        prev_dist = parent_payload.metadata.get("distribution", [])
        prev_fitness = parent_payload.metadata.get("fitness", current_fitness)
        prev_entropy = parent_frame.entropy

        disp = _l2_displacement(current_embedding, prev_embedding)
        kl = _kl_divergence(current_distribution, prev_dist)
        curr_ent = _shannon_entropy(current_distribution)
        delta_h = abs(curr_ent - prev_entropy)
        delta_f = abs(current_fitness - prev_fitness)
        parent_uuid = parent_frame.node_uuid
        parent_lineage = parent_frame.lineage_hash
    else:
        disp = 0.0
        kl = 0.0
        curr_ent = _shannon_entropy(current_distribution)
        delta_h = 0.0
        delta_f = 0.0
        parent_uuid = None
        parent_lineage = "GENESIS_ROOT"

    # Drift vector components: (disp, kl, delta_h, delta_f)
    drift_vector = (disp, kl, delta_h, delta_f)

    # Composite drift score
    drift_score = (alpha * disp) + (beta * kl) + (gamma * delta_h) + (delta * delta_f)

    # Four-state runtime classification
    if drift_score < tau_s:
        drift_class = DriftClass.STABILIZE.value
        confidence = 1.0 - (drift_score / max(tau_s, 1e-6)) * 0.1  # High confidence
    elif drift_score < tau_d:
        drift_class = DriftClass.TRANSITION.value
        confidence = 0.85
    elif drift_score < tau_c:
        drift_class = DriftClass.DRIFT.value
        confidence = 0.60
    else:
        drift_class = DriftClass.CRITICAL.value
        confidence = 0.20

    # Cryptographic hashes
    state_hash = _canonical_hash(current_state)
    config_hash = _canonical_hash(current_config)
    lineage_str = f"{parent_lineage}:{node_uuid}:{state_hash}"
    lineage_hash = hashlib.sha256(lineage_str.encode("utf-8")).hexdigest()

    # Fitness delta from parent
    fitness_delta = (current_fitness - parent_payload.metadata.get("fitness", current_fitness)) if parent_payload else 0.0

    frame = NexusFrame(
        node_uuid=node_uuid,
        parent_uuid=parent_uuid,
        checkpoint_uuid=checkpoint_uuid,
        timestamp_ns=timestamp_ns,
        subsystem=subsystem,
        operation=operation,
        entropy=curr_ent,
        drift_vector=drift_vector,
        drift_score=drift_score,
        drift_class=drift_class,
        confidence=confidence,
        latency_ns=latency_ns,
        memory_delta_bytes=memory_delta_bytes,
        fitness_delta=fitness_delta,
        state_hash=state_hash,
        config_hash=config_hash,
        lineage_hash=lineage_hash,
    )

    payload_metadata = {
        "embedding": list(current_embedding),
        "distribution": list(current_distribution),
        "fitness": current_fitness,
        "drift_score": drift_score,
        "drift_class": drift_class,
    }
    if metadata:
        payload_metadata.update(metadata)

    payload = CheckpointPayload(
        checkpoint_uuid=checkpoint_uuid,
        timestamp_ns=timestamp_ns,
        subsystem=subsystem,
        state_data=current_state,
        config_data=current_config,
        metadata=payload_metadata,
        parent_checkpoint_uuid=parent_frame.checkpoint_uuid if parent_frame else None,
    )

    if ledger is not None:
        ledger.append_frame(frame, payload)

    return frame, payload
