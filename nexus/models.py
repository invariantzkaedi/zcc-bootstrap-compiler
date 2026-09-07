"""
nexus.models - Canonical Data Models, Schemas & Frame Specifications
=====================================================================
High-assurance immutable structures for symbolic drift telemetry, DAG
lineage, counterfactual replay branches, and supervisor governance.
"""

from __future__ import annotations

import json
import hashlib
import time
from dataclasses import dataclass, asdict
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple
from uuid import UUID, uuid4


class DriftClass(str, Enum):
    """Four-state runtime classification derived from composite drift metrics."""
    STABILIZE = "stabilize"
    TRANSITION = "transition"
    DRIFT = "drift"
    CRITICAL = "critical"


class SupervisorDecision(str, Enum):
    """Bounded governance decisions emitted by the Nexus Supervisor loop."""
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
    """Canonical immutable telemetry frame tracking DAG node provenance."""
    node_uuid: UUID
    parent_uuid: Optional[UUID]
    checkpoint_uuid: UUID

    timestamp_ns: int
    subsystem: str
    operation: str

    entropy: float
    drift_vector: Tuple[float, ...]
    drift_score: float
    drift_class: str
    confidence: float

    latency_ns: int
    memory_delta_bytes: int
    fitness_delta: float

    state_hash: str
    config_hash: str
    lineage_hash: str

    def to_dict(self) -> Dict[str, Any]:
        """Serialize frame to a JSON-safe dictionary."""
        return {
            "node_uuid": str(self.node_uuid),
            "parent_uuid": str(self.parent_uuid) if self.parent_uuid else None,
            "checkpoint_uuid": str(self.checkpoint_uuid),
            "timestamp_ns": self.timestamp_ns,
            "subsystem": self.subsystem,
            "operation": self.operation,
            "entropy": float(self.entropy),
            "drift_vector": list(self.drift_vector),
            "drift_score": float(self.drift_score),
            "drift_class": self.drift_class,
            "confidence": float(self.confidence),
            "latency_ns": self.latency_ns,
            "memory_delta_bytes": self.memory_delta_bytes,
            "fitness_delta": float(self.fitness_delta),
            "state_hash": self.state_hash,
            "config_hash": self.config_hash,
            "lineage_hash": self.lineage_hash,
        }

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> NexusFrame:
        """Deserialize frame from a dictionary."""
        return cls(
            node_uuid=UUID(d["node_uuid"]),
            parent_uuid=UUID(d["parent_uuid"]) if d.get("parent_uuid") else None,
            checkpoint_uuid=UUID(d["checkpoint_uuid"]),
            timestamp_ns=int(d["timestamp_ns"]),
            subsystem=str(d["subsystem"]),
            operation=str(d["operation"]),
            entropy=float(d["entropy"]),
            drift_vector=tuple(float(x) for x in d["drift_vector"]),
            drift_score=float(d["drift_score"]),
            drift_class=str(d["drift_class"]),
            confidence=float(d["confidence"]),
            latency_ns=int(d["latency_ns"]),
            memory_delta_bytes=int(d["memory_delta_bytes"]),
            fitness_delta=float(d["fitness_delta"]),
            state_hash=str(d["state_hash"]),
            config_hash=str(d["config_hash"]),
            lineage_hash=str(d["lineage_hash"]),
        )


@dataclass
class CheckpointPayload:
    """Full snapshot payload associated with a checkpoint_uuid."""
    checkpoint_uuid: UUID
    timestamp_ns: int
    subsystem: str
    state_data: Any
    config_data: Dict[str, Any]
    metadata: Dict[str, Any]
    parent_checkpoint_uuid: Optional[UUID] = None

    def serialize(self) -> str:
        payload = {
            "checkpoint_uuid": str(self.checkpoint_uuid),
            "timestamp_ns": self.timestamp_ns,
            "subsystem": self.subsystem,
            "state_data": self.state_data,
            "config_data": self.config_data,
            "metadata": self.metadata,
            "parent_checkpoint_uuid": str(self.parent_checkpoint_uuid) if self.parent_checkpoint_uuid else None,
        }
        return json.dumps(payload, sort_keys=True)

    @classmethod
    def deserialize(cls, json_str: str) -> CheckpointPayload:
        d = json.loads(json_str)
        return cls(
            checkpoint_uuid=UUID(d["checkpoint_uuid"]),
            timestamp_ns=int(d["timestamp_ns"]),
            subsystem=str(d["subsystem"]),
            state_data=d["state_data"],
            config_data=d["config_data"],
            metadata=d["metadata"],
            parent_checkpoint_uuid=UUID(d["parent_checkpoint_uuid"]) if d.get("parent_checkpoint_uuid") else None,
        )


@dataclass
class ReplayResult:
    """Structured report returned by counterfactual branch replay."""
    fork_node_uuid: UUID
    origin_checkpoint_uuid: UUID
    causal_delta: float
    counterfactual_fitness: float
    observed_fitness: float
    invariant_violations: List[str]
    resource_delta_bytes: int
    latency_delta_ns: int
    confidence: float
    replay_frame: NexusFrame
    verdict: str  # e.g. "SUPERIOR", "INFERIOR", "INVARIANT_BREACH", "PARITY"
