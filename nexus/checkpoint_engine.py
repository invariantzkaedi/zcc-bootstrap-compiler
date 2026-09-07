from __future__ import annotations

import math
import time
from collections.abc import Mapping, Sequence
from typing import Any
from uuid import UUID, uuid4

from .ledger import NexusDAGLedger, lineage_hash, sha256_hex
from .models import CheckpointState, DriftClass, DriftThresholds, DriftWeights, NexusFrame


def _finite_vector(name: str, values: Sequence[float]) -> tuple[float, ...]:
    result = tuple(float(v) for v in values)
    if any(not math.isfinite(v) for v in result):
        raise ValueError(f"{name} must contain only finite values")
    return result


def l2_displacement(current: Sequence[float], previous: Sequence[float]) -> tuple[tuple[float, ...], float]:
    a = _finite_vector("current latent vector", current)
    b = _finite_vector("previous latent vector", previous)
    if len(a) != len(b):
        raise ValueError("current and previous latent vectors must have identical lengths")
    delta = tuple(x - y for x, y in zip(a, b))
    return delta, math.sqrt(sum(v * v for v in delta))


def _normalized_distribution(name: str, values: Sequence[float], epsilon: float) -> tuple[float, ...]:
    raw = _finite_vector(name, values)
    if not raw:
        raise ValueError(f"{name} must not be empty")
    if any(v < 0.0 for v in raw):
        raise ValueError(f"{name} cannot contain negative probabilities")
    total = sum(raw)
    if total <= 0.0:
        raise ValueError(f"{name} must have positive total mass")
    probs = tuple(v / total for v in raw)
    # Epsilon smoothing makes KL finite when the previous distribution has a zero bin.
    smooth = tuple(max(p, epsilon) for p in probs)
    renorm = sum(smooth)
    return tuple(p / renorm for p in smooth)


def kl_divergence(current: Sequence[float], previous: Sequence[float], epsilon: float = 1e-12) -> float:
    if epsilon <= 0.0:
        raise ValueError("epsilon must be positive")
    p = _normalized_distribution("current distribution", current, epsilon)
    q = _normalized_distribution("previous distribution", previous, epsilon)
    if len(p) != len(q):
        raise ValueError("current and previous distributions must have identical lengths")
    return sum(pi * math.log(pi / qi) for pi, qi in zip(p, q))


def shannon_entropy(distribution: Sequence[float], epsilon: float = 1e-12) -> float:
    p = _normalized_distribution("distribution", distribution, epsilon)
    return -sum(x * math.log(x) for x in p)


def classify_drift(score: float, thresholds: DriftThresholds) -> DriftClass:
    thresholds.validate()
    if not math.isfinite(score) or score < 0.0:
        raise ValueError("drift score must be finite and non-negative")
    if score < thresholds.tau_s:
        return DriftClass.STABILIZE
    if score < thresholds.tau_d:
        return DriftClass.TRANSITION
    if score < thresholds.tau_c:
        return DriftClass.DRIFT
    return DriftClass.CRITICAL


def symbolic_drift_checkpoint_engine(
    *,
    ledger: NexusDAGLedger,
    subsystem: str,
    operation: str,
    current_state: Any,
    config: Mapping[str, Any],
    current_latent: Sequence[float],
    previous_latent: Sequence[float],
    current_distribution: Sequence[float],
    previous_distribution: Sequence[float],
    current_fitness: float,
    previous_fitness: float,
    current_entropy: float | None = None,
    previous_entropy: float | None = None,
    weights: DriftWeights = DriftWeights(),
    thresholds: DriftThresholds = DriftThresholds(),
    confidence: float = 1.0,
    latency_ns: int = 0,
    memory_delta_bytes: int = 0,
    parent_uuid: UUID | None = None,
    parent_lineage_hash: str | None = None,
    node_uuid: UUID | None = None,
    checkpoint_uuid: UUID | None = None,
    timestamp_ns: int | None = None,
) -> NexusFrame:
    """
    Measure symbolic drift, persist a checkpoint, and append an immutable frame.

    D_t = alpha*||z_t-z_{t-1}||_2 + beta*KL(P_t||P_{t-1})
          + gamma*|H_t-H_{t-1}| + delta*|F_t-F_{t-1}|.
    """
    if not subsystem.strip() or not operation.strip():
        raise ValueError("subsystem and operation must be non-empty")
    thresholds.validate()
    if any(not math.isfinite(v) or v < 0.0 for v in (weights.alpha, weights.beta, weights.gamma, weights.delta)):
        raise ValueError("drift weights must be finite and non-negative")
    if not 0.0 <= confidence <= 1.0 or not math.isfinite(confidence):
        raise ValueError("confidence must be finite and within [0, 1]")
    if latency_ns < 0:
        raise ValueError("latency_ns cannot be negative")
    if not math.isfinite(float(current_fitness)) or not math.isfinite(float(previous_fitness)):
        raise ValueError("fitness values must be finite")

    node_uuid = node_uuid or uuid4()
    checkpoint_uuid = checkpoint_uuid or uuid4()
    timestamp_ns = timestamp_ns if timestamp_ns is not None else time.time_ns()

    if parent_uuid is not None:
        parent = ledger.find_frame_by_node(parent_uuid)
        if parent_lineage_hash is not None and parent_lineage_hash != parent.lineage_hash:
            raise ValueError("supplied parent_lineage_hash does not match ledger parent")
        parent_lineage_hash = parent.lineage_hash
    elif parent_lineage_hash is not None:
        raise ValueError("parent_lineage_hash cannot be supplied without parent_uuid")

    drift_vector, l2 = l2_displacement(current_latent, previous_latent)
    kl = kl_divergence(current_distribution, previous_distribution)
    h_current = shannon_entropy(current_distribution) if current_entropy is None else float(current_entropy)
    h_previous = shannon_entropy(previous_distribution) if previous_entropy is None else float(previous_entropy)
    if not math.isfinite(h_current) or not math.isfinite(h_previous):
        raise ValueError("entropy values must be finite")
    fitness_delta = float(current_fitness) - float(previous_fitness)

    score = (
        weights.alpha * l2
        + weights.beta * kl
        + weights.gamma * abs(h_current - h_previous)
        + weights.delta * abs(fitness_delta)
    )
    drift_class = classify_drift(score, thresholds)
    state_hash = sha256_hex(current_state)
    config_hash = sha256_hex(config)
    lin_hash = lineage_hash(parent_lineage_hash, node_uuid)

    frame = NexusFrame(
        node_uuid=node_uuid,
        parent_uuid=parent_uuid,
        checkpoint_uuid=checkpoint_uuid,
        timestamp_ns=timestamp_ns,
        subsystem=subsystem,
        operation=operation,
        entropy=h_current,
        drift_vector=drift_vector,
        drift_score=score,
        drift_class=drift_class.value,
        confidence=confidence,
        latency_ns=int(latency_ns),
        memory_delta_bytes=int(memory_delta_bytes),
        fitness_delta=fitness_delta,
        state_hash=state_hash,
        config_hash=config_hash,
        lineage_hash=lin_hash,
    )
    checkpoint = CheckpointState(
        checkpoint_uuid=checkpoint_uuid,
        node_uuid=node_uuid,
        parent_uuid=parent_uuid,
        timestamp_ns=timestamp_ns,
        subsystem=subsystem,
        operation=operation,
        state=current_state,
        config=dict(config),
        entropy=h_current,
        fitness=float(current_fitness),
        state_hash=state_hash,
        config_hash=config_hash,
        lineage_hash=lin_hash,
    )

    # Persist state before advertising it in the append-only ledger. A crash can
    # leave an unreferenced checkpoint file, but never a ledger entry that points
    # to a checkpoint which was not durably written first.
    ledger.save_checkpoint(checkpoint)
    ledger.append_frame(frame)
    return frame
