"""
nexus.replay_engine - Counterfactual DAG Branch Replay & Causal Inference
==========================================================================
Enables retrospective branch execution from any historical checkpoint in the DAG,
applying counterfactual mutations to measure true causal lift vs observed reality:

    Delta_causal = F(counterfactual) - F(observed)
"""

from __future__ import annotations

import copy
import time
from typing import Any, Callable, Dict, List, Optional, Tuple
from uuid import UUID, uuid4

from .models import DriftClass, NexusFrame, CheckpointPayload, ReplayResult
from .checkpoint_engine import NexusDAGLedger, symbolic_drift_checkpoint_engine


def counterfactual_dag_replay_engine(
    origin_checkpoint_uuid: UUID,
    counterfactual_config_overrides: Dict[str, Any],
    counterfactual_evaluator: Callable[[Any, Dict[str, Any]], Tuple[Any, float, List[float], List[float], List[str], Dict[str, Any]]],
    ledger: NexusDAGLedger,
    operation_label: str = "counterfactual_branch_replay",
) -> ReplayResult:
    """
    Forks execution from a historical checkpoint, evaluates counterfactual parameters,
    computes exact causal deltas Delta_causal = F(counterfactual) - F(observed),
    audits invariants, and integrates the branch frame into the DAG.

    Args:
        origin_checkpoint_uuid: Target checkpoint to fork from.
        counterfactual_config_overrides: Key-value parameter mutations (e.g. optimizer, passes, precision).
        counterfactual_evaluator: Callable (state, config) -> (new_state, fitness, embedding, dist, invariant_violations, metrics).
        ledger: The active NexusDAGLedger containing historical nodes.
        operation_label: Subsystem audit tag.

    Returns:
        ReplayResult with causal delta, invariant report, and new NexusFrame.
    """
    if origin_checkpoint_uuid not in ledger.checkpoints:
        raise KeyError(f"Checkpoint UUID {origin_checkpoint_uuid} not found in active ledger.")

    origin_payload = ledger.checkpoints[origin_checkpoint_uuid]
    
    # Locate origin frame
    origin_frame = None
    for f in ledger.frames.values():
        if f.checkpoint_uuid == origin_checkpoint_uuid:
            origin_frame = f
            break
    if origin_frame is None:
        raise ValueError(f"Origin frame matching checkpoint {origin_checkpoint_uuid} missing in ledger.")

    observed_fitness = origin_payload.metadata.get("fitness", origin_frame.fitness_delta)

    # Clone state and fuse config overrides
    forked_state = copy.deepcopy(origin_payload.state_data)
    forked_config = copy.deepcopy(origin_payload.config_data)
    forked_config.update(counterfactual_config_overrides)

    start_ns = time.time_ns()
    # Execute counterfactual branch in sandbox
    (
        cf_state,
        cf_fitness,
        cf_embedding,
        cf_distribution,
        violations,
        metrics,
    ) = counterfactual_evaluator(forked_state, forked_config)
    elapsed_ns = time.time_ns() - start_ns

    # Compute Causal Delta: F(counterfactual) - F(observed)
    causal_delta = cf_fitness - observed_fitness
    resource_delta = metrics.get("memory_delta_bytes", 0)
    latency_delta = elapsed_ns - origin_frame.latency_ns

    # Derive verdict
    if violations:
        verdict = "INVARIANT_BREACH"
        confidence = 0.0
    elif causal_delta > 1e-5:
        verdict = "SUPERIOR"
        confidence = min(1.0, 0.8 + abs(causal_delta) * 0.1)
    elif causal_delta < -1e-5:
        verdict = "INFERIOR"
        confidence = min(1.0, 0.8 + abs(causal_delta) * 0.1)
    else:
        verdict = "PARITY"
        confidence = 0.95

    # Emit new frame parented to origin frame
    replay_frame, replay_payload = symbolic_drift_checkpoint_engine(
        subsystem=origin_frame.subsystem,
        operation=operation_label,
        current_state=cf_state,
        current_config=forked_config,
        current_embedding=cf_embedding,
        current_distribution=cf_distribution,
        current_fitness=cf_fitness,
        parent_frame=origin_frame,
        parent_payload=origin_payload,
        ledger=ledger,
        latency_ns=elapsed_ns,
        memory_delta_bytes=resource_delta,
        metadata={
            "counterfactual": True,
            "origin_checkpoint_uuid": str(origin_checkpoint_uuid),
            "causal_delta": causal_delta,
            "verdict": verdict,
            "invariant_violations": violations,
            "config_overrides": counterfactual_config_overrides,
        },
    )

    return ReplayResult(
        fork_node_uuid=replay_frame.node_uuid,
        origin_checkpoint_uuid=origin_checkpoint_uuid,
        causal_delta=causal_delta,
        counterfactual_fitness=cf_fitness,
        observed_fitness=observed_fitness,
        invariant_violations=violations,
        resource_delta_bytes=resource_delta,
        latency_delta_ns=latency_delta,
        confidence=confidence,
        replay_frame=replay_frame,
        verdict=verdict,
    )
