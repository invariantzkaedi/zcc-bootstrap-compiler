from __future__ import annotations

import math
import time
from collections.abc import Callable, Mapping, Sequence
from typing import Any
from uuid import UUID

from .checkpoint_engine import symbolic_drift_checkpoint_engine
from .ledger import NexusDAGLedger
from .models import DriftThresholds, DriftWeights, ReplayResult


ReplayFn = Callable[[Any, Mapping[str, Any]], tuple[Any, float, Sequence[float], Sequence[float], int, int]]
InvariantFn = Callable[[Any], str | None]


def counterfactual_dag_replay_engine(
    *,
    ledger: NexusDAGLedger,
    checkpoint_uuid: UUID,
    alternate_config: Mapping[str, Any],
    replay_fn: ReplayFn,
    invariant_checks: Sequence[InvariantFn] = (),
    weights: DriftWeights = DriftWeights(),
    thresholds: DriftThresholds = DriftThresholds(),
    confidence: float = 1.0,
) -> ReplayResult:
    """
    Fork a historical checkpoint into an isolated deterministic branch.

    replay_fn(state, alternate_config) returns:
        (new_state, fitness, latent_vector, distribution, latency_ns, memory_bytes)

    For the observed ancestor, a latent vector/distribution may be stored in its
    state as ``latent`` and ``distribution``. If absent, deterministic scalar
    projections are used so the branch is still representable in the drift DAG.
    """
    ancestor = ledger.load_checkpoint(checkpoint_uuid)
    ancestor_frame = ledger.find_frame_by_checkpoint(checkpoint_uuid)

    output = replay_fn(ancestor.state, alternate_config)
    if not isinstance(output, tuple) or len(output) != 6:
        raise TypeError(
            "replay_fn must return "
            "(new_state, fitness, latent_vector, distribution, latency_ns, memory_bytes)"
        )
    new_state, new_fitness, new_latent, new_distribution, latency_ns, memory_bytes = output
    if not math.isfinite(float(new_fitness)):
        raise ValueError("counterfactual fitness must be finite")

    violations: list[str] = []
    for check in invariant_checks:
        try:
            violation = check(new_state)
        except Exception as exc:
            violations.append(f"{getattr(check, '__name__', 'invariant')}: raised {type(exc).__name__}: {exc}")
            continue
        if violation:
            violations.append(str(violation))

    old_latent: Sequence[float]
    old_distribution: Sequence[float]
    if isinstance(ancestor.state, Mapping) and "latent" in ancestor.state:
        old_latent = ancestor.state["latent"]
    else:
        old_latent = tuple(0.0 for _ in new_latent)
    if isinstance(ancestor.state, Mapping) and "distribution" in ancestor.state:
        old_distribution = ancestor.state["distribution"]
    else:
        n = max(1, len(tuple(new_distribution)))
        old_distribution = tuple(1.0 / n for _ in range(n))

    branch_frame = symbolic_drift_checkpoint_engine(
        ledger=ledger,
        subsystem=ancestor.subsystem,
        operation=f"counterfactual:{ancestor.operation}",
        current_state=new_state,
        config=dict(alternate_config),
        current_latent=new_latent,
        previous_latent=old_latent,
        current_distribution=new_distribution,
        previous_distribution=old_distribution,
        current_fitness=float(new_fitness),
        previous_fitness=ancestor.fitness,
        weights=weights,
        thresholds=thresholds,
        confidence=confidence,
        latency_ns=int(latency_ns),
        memory_delta_bytes=int(memory_bytes) - int(ancestor_frame.memory_delta_bytes),
        parent_uuid=ancestor.node_uuid,
    )

    return ReplayResult(
        ancestor_checkpoint_uuid=checkpoint_uuid,
        branch_checkpoint_uuid=branch_frame.checkpoint_uuid,
        observed_fitness=ancestor.fitness,
        counterfactual_fitness=float(new_fitness),
        causal_delta=float(new_fitness) - ancestor.fitness,
        latency_delta_ns=int(latency_ns) - ancestor_frame.latency_ns,
        memory_delta_bytes=int(memory_bytes) - ancestor_frame.memory_delta_bytes,
        invariant_violations=tuple(violations),
        frame=branch_frame,
    )
