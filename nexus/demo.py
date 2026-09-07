from __future__ import annotations

import argparse
import tempfile
from pathlib import Path

from .checkpoint_engine import symbolic_drift_checkpoint_engine
from .ledger import NexusDAGLedger
from .models import DriftThresholds, DriftWeights
from .replay_engine import counterfactual_dag_replay_engine
from .supervisor import nexus_supervisor_control_loop


def main() -> int:
    parser = argparse.ArgumentParser(description="Nexus Legendary end-to-end DAG demo")
    parser.add_argument("--ledger", type=Path, help="persist demo ledger here")
    args = parser.parse_args()

    temp = None
    if args.ledger is None:
        temp = tempfile.TemporaryDirectory(prefix="nexus-demo-")
        ledger_root = Path(temp.name)
    else:
        ledger_root = args.ledger
    ledger = NexusDAGLedger(ledger_root)

    thresholds = DriftThresholds(tau_s=0.08, tau_d=0.25, tau_c=0.70)
    weights = DriftWeights(alpha=0.50, beta=0.25, gamma=0.10, delta=0.15)

    state0 = {"optimizer": "AdamW", "step": 0, "latent": [0.0, 0.0], "distribution": [0.5, 0.5]}
    f0 = symbolic_drift_checkpoint_engine(
        ledger=ledger,
        subsystem="ai.optimizer",
        operation="adamw.step",
        current_state=state0,
        config={"optimizer": "AdamW", "lr": 1e-3},
        current_latent=[0.0, 0.0],
        previous_latent=[0.0, 0.0],
        current_distribution=[0.5, 0.5],
        previous_distribution=[0.5, 0.5],
        current_fitness=0.50,
        previous_fitness=0.50,
        thresholds=thresholds,
        weights=weights,
        confidence=0.98,
    )

    state1 = {"optimizer": "AdamW", "step": 1, "latent": [0.35, -0.20], "distribution": [0.72, 0.28]}
    f1 = symbolic_drift_checkpoint_engine(
        ledger=ledger,
        subsystem="ai.optimizer",
        operation="adamw.step",
        current_state=state1,
        config={"optimizer": "AdamW", "lr": 1e-3},
        current_latent=state1["latent"],
        previous_latent=state0["latent"],
        current_distribution=state1["distribution"],
        previous_distribution=state0["distribution"],
        current_fitness=0.46,
        previous_fitness=0.50,
        thresholds=thresholds,
        weights=weights,
        confidence=0.95,
        parent_uuid=f0.node_uuid,
    )

    def zkaedi_replay(state, config):
        new_state = {
            "optimizer": "ZKAEDI-Prime",
            "step": int(state["step"]) + 1,
            "latent": [0.08, -0.03],
            "distribution": [0.54, 0.46],
        }
        return new_state, 0.58, new_state["latent"], new_state["distribution"], 120_000, 4096

    replay = counterfactual_dag_replay_engine(
        ledger=ledger,
        checkpoint_uuid=f0.checkpoint_uuid,
        alternate_config={"optimizer": "ZKAEDI-Prime", "lr": 1e-3},
        replay_fn=zkaedi_replay,
        thresholds=thresholds,
        weights=weights,
    )

    events = nexus_supervisor_control_loop([f0, f1, replay.frame], ledger=ledger)
    ok, errors = ledger.verify()

    print("Nexus Legendary Edition demo")
    print(f"ledger: {ledger_root}")
    print(f"AdamW drift: {f1.drift_score:.6f} [{f1.drift_class}]")
    print(f"ZKAEDI counterfactual causal delta: {replay.causal_delta:+.6f}")
    print("Supervisor:", ", ".join(event.decision.value for event in events))
    print(f"Ledger verification: {'PASS' if ok else 'FAIL'}")
    for error in errors:
        print("  -", error)

    if temp is not None:
        temp.cleanup()
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
