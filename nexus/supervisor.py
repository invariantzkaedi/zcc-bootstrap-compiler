from __future__ import annotations

import time
from collections.abc import Iterable, Mapping, Sequence
from typing import Any
from uuid import UUID, uuid4

from .ledger import NexusDAGLedger, lineage_hash, sha256_hex
from .models import CheckpointState, DriftClass, NexusFrame, SupervisorDecision, SupervisorEvent


def _select_decision(
    frame: NexusFrame,
    *,
    violations: Sequence[str],
    stabilized_checkpoint: UUID | None,
    improvement_confidence: float,
) -> tuple[SupervisorDecision, str, UUID | None]:
    if violations:
        return (
            SupervisorDecision.QUARANTINE,
            "invariant violation: " + "; ".join(violations),
            None,
        )
    if frame.drift_class == DriftClass.CRITICAL.value:
        if stabilized_checkpoint is not None:
            return (
                SupervisorDecision.ROLLBACK,
                "critical drift; rollback to last stabilized checkpoint",
                stabilized_checkpoint,
            )
        return (
            SupervisorDecision.QUARANTINE,
            "critical drift with no stabilized rollback checkpoint",
            None,
        )
    if frame.drift_class == DriftClass.DRIFT.value:
        return SupervisorDecision.THROTTLE, "sub-critical drift detected", None
    if (
        frame.drift_class == DriftClass.STABILIZE.value
        and frame.fitness_delta > 0.0
        and frame.confidence >= improvement_confidence
    ):
        return SupervisorDecision.CHECKPOINT, "high-confidence stabilized improvement", None
    return SupervisorDecision.CONTINUE, "trajectory within bounded operating policy", None


def _emit_decision_frame(
    *,
    ledger: NexusDAGLedger,
    source: NexusFrame,
    decision: SupervisorDecision,
    reason: str,
    rollback_checkpoint_uuid: UUID | None,
    policy: Mapping[str, Any],
) -> NexusFrame:
    node_uuid = uuid4()
    checkpoint_uuid = uuid4()
    timestamp_ns = time.time_ns()
    state = {
        "decision": decision.value,
        "reason": reason,
        "source_node_uuid": str(source.node_uuid),
        "source_checkpoint_uuid": str(source.checkpoint_uuid),
        "rollback_checkpoint_uuid": str(rollback_checkpoint_uuid) if rollback_checkpoint_uuid else None,
    }
    config = dict(policy)
    lin_hash = lineage_hash(source.lineage_hash, node_uuid)
    state_hash = sha256_hex(state)
    config_hash = sha256_hex(config)
    frame = NexusFrame(
        node_uuid=node_uuid,
        parent_uuid=source.node_uuid,
        checkpoint_uuid=checkpoint_uuid,
        timestamp_ns=timestamp_ns,
        subsystem="nexus.supervisor",
        operation=f"decision:{decision.value}",
        entropy=source.entropy,
        drift_vector=source.drift_vector,
        drift_score=source.drift_score,
        drift_class=source.drift_class,
        confidence=source.confidence,
        latency_ns=0,
        memory_delta_bytes=0,
        fitness_delta=0.0,
        state_hash=state_hash,
        config_hash=config_hash,
        lineage_hash=lin_hash,
    )
    checkpoint = CheckpointState(
        checkpoint_uuid=checkpoint_uuid,
        node_uuid=node_uuid,
        parent_uuid=source.node_uuid,
        timestamp_ns=timestamp_ns,
        subsystem="nexus.supervisor",
        operation=frame.operation,
        state=state,
        config=config,
        entropy=source.entropy,
        fitness=source.fitness_delta,
        state_hash=state_hash,
        config_hash=config_hash,
        lineage_hash=lin_hash,
    )
    ledger.save_checkpoint(checkpoint)
    ledger.append_frame(frame)
    return frame


def nexus_supervisor_control_loop(
    frames: Iterable[NexusFrame],
    *,
    ledger: NexusDAGLedger,
    invariant_violations: Mapping[UUID, Sequence[str]] | None = None,
    improvement_confidence: float = 0.90,
) -> tuple[SupervisorEvent, ...]:
    """
    Apply a deterministic bounded policy and record every decision in the DAG.

    Policy:
      * invariant violation -> QUARANTINE
      * CRITICAL -> ROLLBACK to last stabilized checkpoint, else QUARANTINE
      * DRIFT -> THROTTLE
      * stabilized + positive fitness + high confidence -> CHECKPOINT
      * otherwise -> CONTINUE
    """
    if not 0.0 <= improvement_confidence <= 1.0:
        raise ValueError("improvement_confidence must be within [0, 1]")
    invariant_violations = invariant_violations or {}
    last_stabilized: UUID | None = None
    events: list[SupervisorEvent] = []
    policy = {
        "improvement_confidence": improvement_confidence,
        "policy_version": 1,
    }

    for frame in frames:
        # Only source telemetry can advance the stable rollback anchor. Supervisor
        # decision frames should not be recursively supervised by this call.
        if frame.subsystem == "nexus.supervisor":
            continue

        violations = tuple(str(v) for v in invariant_violations.get(frame.node_uuid, ()))
        decision, reason, rollback_uuid = _select_decision(
            frame,
            violations=violations,
            stabilized_checkpoint=last_stabilized,
            improvement_confidence=improvement_confidence,
        )
        decision_frame = _emit_decision_frame(
            ledger=ledger,
            source=frame,
            decision=decision,
            reason=reason,
            rollback_checkpoint_uuid=rollback_uuid,
            policy=policy,
        )
        events.append(
            SupervisorEvent(
                source_node_uuid=frame.node_uuid,
                decision=decision,
                reason=reason,
                decision_frame=decision_frame,
                rollback_checkpoint_uuid=rollback_uuid,
            )
        )
        if frame.drift_class == DriftClass.STABILIZE.value and not violations:
            last_stabilized = frame.checkpoint_uuid

    return tuple(events)
