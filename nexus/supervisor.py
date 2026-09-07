"""
nexus.supervisor - Nexus Supervisor Control Loop & Governance
==============================================================
Autonomous supervisory control loop that consumes telemetry streams, evaluates
drift regimes, enforces invariant bounds, and issues auditable governance actions:
CONTINUE, THROTTLE, REROUTE, CHECKPOINT, ROLLBACK, SPAWN, MERGE, QUARANTINE, ESCALATE.

Every supervisor decision is itself an immutable DAG frame.
"""

from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Tuple
from uuid import UUID, uuid4

from .models import DriftClass, SupervisorDecision, NexusFrame, CheckpointPayload
from .checkpoint_engine import NexusDAGLedger, symbolic_drift_checkpoint_engine


class SupervisorPolicy:
    """Configurable threshold and invariant policy for the supervisor."""

    def __init__(
        self,
        max_consecutive_drifts: int = 3,
        critical_drift_threshold: float = 0.50,
        fitness_drop_tolerance: float = -0.15,
        checkpoint_interval_steps: int = 10,
        enable_automatic_rollback: bool = True,
    ):
        self.max_consecutive_drifts = max_consecutive_drifts
        self.critical_drift_threshold = critical_drift_threshold
        self.fitness_drop_tolerance = fitness_drop_tolerance
        self.checkpoint_interval_steps = checkpoint_interval_steps
        self.enable_automatic_rollback = enable_automatic_rollback


class NexusSupervisorState:
    """Stateful tracking across frame evaluations."""

    def __init__(self):
        self.consecutive_drifts: int = 0
        self.step_count: int = 0
        self.last_stabilized_checkpoint: Optional[UUID] = None
        self.active_throttle_factor: float = 1.0
        self.quarantined_nodes: set[UUID] = set()


def nexus_supervisor_control_loop(
    incoming_frame: NexusFrame,
    incoming_payload: Optional[CheckpointPayload],
    ledger: NexusDAGLedger,
    supervisor_state: NexusSupervisorState,
    policy: Optional[SupervisorPolicy] = None,
) -> Tuple[SupervisorDecision, NexusFrame, Dict[str, Any]]:
    """
    Evaluates an incoming frame against supervisor policy and emits an auditable
    governance action frame linked directly into the DAG.

    Returns:
        (decision, supervisor_frame, action_directives)
    """
    if policy is None:
        policy = SupervisorPolicy()

    supervisor_state.step_count += 1
    directives: Dict[str, Any] = {}

    # Track stabilized checkpoints
    if incoming_frame.drift_class == DriftClass.STABILIZE.value:
        supervisor_state.consecutive_drifts = 0
        supervisor_state.active_throttle_factor = 1.0
        supervisor_state.last_stabilized_checkpoint = incoming_frame.checkpoint_uuid
        
        # Periodic checkpoint reinforcement
        if supervisor_state.step_count % policy.checkpoint_interval_steps == 0:
            decision = SupervisorDecision.CHECKPOINT
            directives["action"] = "persist_gold_master"
        else:
            decision = SupervisorDecision.CONTINUE
            directives["action"] = "nominal_execution"

    elif incoming_frame.drift_class == DriftClass.TRANSITION.value:
        supervisor_state.consecutive_drifts = 0
        decision = SupervisorDecision.CONTINUE
        directives["action"] = "monitor_sampling_rate_increase"

    elif incoming_frame.drift_class == DriftClass.DRIFT.value:
        supervisor_state.consecutive_drifts += 1
        if supervisor_state.consecutive_drifts >= policy.max_consecutive_drifts:
            decision = SupervisorDecision.THROTTLE
            supervisor_state.active_throttle_factor = max(0.2, 1.0 - incoming_frame.drift_score)
            directives["action"] = "dampen_mutation_or_learning_rate"
            directives["throttle_factor"] = supervisor_state.active_throttle_factor
        else:
            decision = SupervisorDecision.CONTINUE
            directives["action"] = "warning_drift_detected"

    elif incoming_frame.drift_class == DriftClass.CRITICAL.value:
        supervisor_state.consecutive_drifts += 1
        if policy.enable_automatic_rollback and supervisor_state.last_stabilized_checkpoint:
            decision = SupervisorDecision.ROLLBACK
            directives["action"] = "revert_to_checkpoint"
            directives["target_checkpoint_uuid"] = str(supervisor_state.last_stabilized_checkpoint)
        else:
            decision = SupervisorDecision.QUARANTINE
            supervisor_state.quarantined_nodes.add(incoming_frame.node_uuid)
            directives["action"] = "isolate_corrupted_branch"

    else:
        decision = SupervisorDecision.ESCALATE
        directives["action"] = "unclassified_regime_escalation"

    # Invariant safety check: Severe negative fitness delta
    if incoming_frame.fitness_delta < policy.fitness_drop_tolerance:
        decision = SupervisorDecision.ROLLBACK if supervisor_state.last_stabilized_checkpoint else SupervisorDecision.QUARANTINE
        directives["action"] = "invariant_violation_severe_fitness_collapse"
        directives["observed_fitness_delta"] = incoming_frame.fitness_delta
        if supervisor_state.last_stabilized_checkpoint:
            directives["target_checkpoint_uuid"] = str(supervisor_state.last_stabilized_checkpoint)

    # Emit supervisor decision as an immutable DAG frame parented to the incoming frame
    sup_frame, sup_payload = symbolic_drift_checkpoint_engine(
        subsystem="nexus_supervisor",
        operation=f"governance_{decision.value}",
        current_state={
            "target_node_uuid": str(incoming_frame.node_uuid),
            "decision": decision.value,
            "directives": directives,
            "supervisor_step": supervisor_state.step_count,
        },
        current_config={
            "policy": {
                "max_consecutive_drifts": policy.max_consecutive_drifts,
                "critical_drift_threshold": policy.critical_drift_threshold,
                "fitness_drop_tolerance": policy.fitness_drop_tolerance,
            }
        },
        current_embedding=[float(incoming_frame.drift_score), float(supervisor_state.active_throttle_factor)],
        current_distribution=[0.5, 0.5],
        current_fitness=incoming_frame.fitness_delta,
        parent_frame=incoming_frame,
        parent_payload=incoming_payload,
        ledger=ledger,
        metadata={
            "supervisor_decision": decision.value,
            "supervised_node": str(incoming_frame.node_uuid),
            "directives": directives,
        },
    )

    return decision, sup_frame, directives
