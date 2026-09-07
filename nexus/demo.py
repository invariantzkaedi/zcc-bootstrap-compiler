"""
nexus.demo - Interactive Demonstration of Nexus Sovereign Runtime
=================================================================
Simulates an end-to-end telemetry pipeline across:
1. Streaming symbolic drift telemetry and DAG checkpoint emission.
2. Autonomous Nexus Supervisor governance with real-time throttle & rollback.
3. Retrospective counterfactual replay computing exact causal deltas.
"""

from __future__ import annotations

import time
from uuid import uuid4
from nexus import (
    NexusDAGLedger,
    symbolic_drift_checkpoint_engine,
    counterfactual_dag_replay_engine,
    nexus_supervisor_control_loop,
    NexusSupervisorState,
    SupervisorPolicy,
    SupervisorDecision,
)


def run_nexus_live_demonstration():
    print("""
╔════════════════════════════════════════════════════════════════════════╗
║ 🔱 NEXUS SOVEREIGN RUNTIME // LEGENDARY EDITION                       ║
║ Symbolic Drift Telemetry • Counterfactual Replay • Supervisor Governance║
╚════════════════════════════════════════════════════════════════════════╝
    """)

    ledger = NexusDAGLedger()
    supervisor_state = NexusSupervisorState()
    policy = SupervisorPolicy(max_consecutive_drifts=2, fitness_drop_tolerance=-0.50)

    # Simulated training loss sequence with an intentional sudden divergence at Step 4
    steps_data = [
        {"step": 0, "loss": 7.8289, "emb": [0.10, 0.10], "dist": [0.50, 0.50], "note": "Genesis initialization"},
        {"step": 1, "loss": 7.7264, "emb": [0.12, 0.11], "dist": [0.51, 0.49], "note": "Warmup step"},
        {"step": 2, "loss": 7.4102, "emb": [0.18, 0.15], "dist": [0.55, 0.45], "note": "Steady descent"},
        {"step": 3, "loss": 7.1500, "emb": [0.45, 0.40], "dist": [0.72, 0.28], "note": "Moderate parameter velocity"},
        {"step": 4, "loss": 7.0200, "emb": [0.70, 0.65], "dist": [0.82, 0.18], "note": "Drift accumulation"},
        {"step": 5, "loss": 9.4500, "emb": [3.80, 3.50], "dist": [0.99, 0.01], "note": "Simulated gradient explosion / divergence"},
    ]

    parent_f = None
    parent_p = None

    print(f"{'Step':<5} | {'Subsystem':<15} | {'Loss':<8} | {'Drift':<8} | {'Class':<12} | {'Supervisor Action':<20}")
    print("-" * 80)

    checkpoint_history = []

    for item in steps_data:
        curr_loss = item["loss"]
        curr_fitness = -curr_loss

        frame, payload = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation=f"step_{item['step']}",
            current_state={"step": item["step"], "loss": curr_loss},
            current_config={"lr": 4e-4, "optimizer": "adamw"},
            current_embedding=item["emb"],
            current_distribution=item["dist"],
            current_fitness=curr_fitness,
            parent_frame=parent_f,
            parent_payload=parent_p,
            ledger=ledger,
        )

        checkpoint_history.append(payload.checkpoint_uuid)

        decision, sup_frame, directives = nexus_supervisor_control_loop(
            incoming_frame=frame,
            incoming_payload=payload,
            ledger=ledger,
            supervisor_state=supervisor_state,
            policy=policy,
        )

        print(f"[{item['step']}]   | {frame.subsystem:<15} | {curr_loss:<8.4f} | {frame.drift_score:<8.4f} | {frame.drift_class:<12} | {decision.value.upper():<20}")

        if decision == SupervisorDecision.ROLLBACK:
            target_cp = directives.get("target_checkpoint_uuid")
            print(f"       ↳ 🛡️ [SUPERVISOR INTERVENTION] Emergency rollback commanded! Reverting state to stabilized checkpoint: {target_cp[:8]}...")
            break

        parent_f = frame
        parent_p = payload

    print("\n" + "=" * 80)
    print(" 🔮 ENGAGING COUNTERFACTUAL DAG REPLAY ENGINE")
    print("=" * 80)

    # Pick a stabilized checkpoint (e.g. step 2) and fork under ZKAEDI Prime
    origin_cp = checkpoint_history[2]
    print(f"• Origin Checkpoint : {origin_cp}")
    print(f"• Observed Fitness   : -7.4102 (Loss: 7.4102 under AdamW)")
    print(f"• Mutation Override  : {{'optimizer': 'zkaedi_prime', 'coupling': 'soft_tanh'}}")

    def zkaedi_counterfactual_evaluator(state, config):
        # Simulates ZKAEDI Prime achieving lower loss (deeper descent)
        new_loss = 6.9540
        cf_fitness = -new_loss
        cf_emb = [0.16, 0.14]
        cf_dist = [0.52, 0.48]
        violations = []
        metrics = {"memory_delta_bytes": -2048, "throughput_tok_sec": 38400}
        return {"step": state["step"] + 1, "loss": new_loss}, cf_fitness, cf_emb, cf_dist, violations, metrics

    replay_res = counterfactual_dag_replay_engine(
        origin_checkpoint_uuid=origin_cp,
        counterfactual_config_overrides={"optimizer": "zkaedi_prime", "coupling": "soft_tanh"},
        counterfactual_evaluator=zkaedi_counterfactual_evaluator,
        ledger=ledger,
    )

    print(f"• Counterfactual Loss: 6.9540")
    print(f"• Causal Delta Δ     : {replay_res.causal_delta:+.4f} (Higher fitness is superior)")
    print(f"• Replay Verdict     : {replay_res.verdict}")
    print(f"• Confidence Score   : {replay_res.confidence:.2%}")
    print(f"• Fork Node UUID     : {replay_res.fork_node_uuid}")
    print(f"• Lineage Hash       : {replay_res.replay_frame.lineage_hash[:32]}...")

    print(f"\n[✔] Total Immutable DAG Frames Registered in Ledger: {len(ledger.frames)}")
    print(f"[✔] Active Leaf Nodes in DAG: {len(ledger.active_leaves)}")
    print("\n🔱 Nexus Sovereign Runtime Demonstration Complete & Verified.\n")


if __name__ == "__main__":
    run_nexus_live_demonstration()
