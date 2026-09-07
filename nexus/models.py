from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Any, Mapping
from uuid import UUID


class DriftClass(str, Enum):
    STABILIZE = "stabilize"
    TRANSITION = "transition"
    DRIFT = "drift"
    CRITICAL = "critical"


class SupervisorDecision(str, Enum):
    CONTINUE = "continue"
    THROTTLE = "throttle"
    REROUTE = "reroute"
    CHECKPOINT = "checkpoint"
    ROLLBACK = "rollback"
    SPAWN = "spawn"
    MERGE = "merge"
    QUARANTINE = "quarantine"
    ESCALATE = "escalate"


@dataclass(frozen=True, slots=True)
class NexusFrame:
    node_uuid: UUID
    parent_uuid: UUID | None
    checkpoint_uuid: UUID
    timestamp_ns: int
    subsystem: str
    operation: str
    entropy: float
    drift_vector: tuple[float, ...]
    drift_score: float
    drift_class: str
    confidence: float
    latency_ns: int
    memory_delta_bytes: int
    fitness_delta: float
    state_hash: str
    config_hash: str
    lineage_hash: str


@dataclass(frozen=True, slots=True)
class CheckpointState:
    checkpoint_uuid: UUID
    node_uuid: UUID
    parent_uuid: UUID | None
    timestamp_ns: int
    subsystem: str
    operation: str
    state: Any
    config: Mapping[str, Any]
    entropy: float
    fitness: float
    state_hash: str
    config_hash: str
    lineage_hash: str


@dataclass(frozen=True, slots=True)
class DriftWeights:
    alpha: float = 1.0
    beta: float = 1.0
    gamma: float = 1.0
    delta: float = 1.0


@dataclass(frozen=True, slots=True)
class DriftThresholds:
    tau_s: float = 0.10
    tau_d: float = 0.50
    tau_c: float = 1.00

    def validate(self) -> None:
        if not (0.0 <= self.tau_s <= self.tau_d <= self.tau_c):
            raise ValueError("thresholds must satisfy 0 <= tau_s <= tau_d <= tau_c")


@dataclass(frozen=True, slots=True)
class ReplayResult:
    ancestor_checkpoint_uuid: UUID
    branch_checkpoint_uuid: UUID
    observed_fitness: float
    counterfactual_fitness: float
    causal_delta: float
    latency_delta_ns: int
    memory_delta_bytes: int
    invariant_violations: tuple[str, ...]
    frame: NexusFrame


@dataclass(frozen=True, slots=True)
class SupervisorEvent:
    source_node_uuid: UUID
    decision: SupervisorDecision
    reason: str
    decision_frame: NexusFrame
    rollback_checkpoint_uuid: UUID | None = None
