"""
tests/test_nexus_legendary.py - Master Verification Gauntlet for Nexus Sovereign Runtime
========================================================================================
Validates mathematical drift calculations, threshold boundaries, Merkle lineage integrity,
counterfactual branch replay causal deltas, and autonomous supervisor governance.
"""

import math
import shutil
import tempfile
import unittest
from dataclasses import FrozenInstanceError
from pathlib import Path
from uuid import uuid4

from nexus import (
    DriftClass,
    SupervisorDecision,
    NexusFrame,
    CheckpointPayload,
    ReplayResult,
    NexusDAGLedger,
    symbolic_drift_checkpoint_engine,
    counterfactual_dag_replay_engine,
    SupervisorPolicy,
    NexusSupervisorState,
    nexus_supervisor_control_loop,
)
from nexus.checkpoint_engine import _l2_displacement, _kl_divergence, _shannon_entropy


class TestNexusLegendary(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(prefix="nexus_test_")
        self.ledger = NexusDAGLedger(ledger_dir=self.temp_dir)

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_01_drift_math_verification(self):
        """Validates exact numerical properties of L2 displacement, KL divergence, and entropy."""
        # L2 displacement
        v1 = [1.0, 2.0, 3.0]
        v2 = [4.0, 6.0, 8.0]
        # (3^2 + 4^2 + 5^2)^0.5 = (9 + 16 + 25)^0.5 = sqrt(50) ~= 7.0710678
        expected_l2 = math.sqrt(50.0)
        self.assertAlmostEqual(_l2_displacement(v1, v2), expected_l2, places=5)

        # KL divergence of identical distributions should be zero
        dist_p = [0.25, 0.25, 0.25, 0.25]
        self.assertAlmostEqual(_kl_divergence(dist_p, dist_p), 0.0, places=6)

        # Entropy of uniform 4-state distribution = ln(4) ~= 1.386294
        self.assertAlmostEqual(_shannon_entropy(dist_p), math.log(4.0), places=5)

        # KL divergence between distinct distributions is strictly positive
        dist_q = [0.70, 0.10, 0.10, 0.10]
        kl_val = _kl_divergence(dist_q, dist_p)
        self.assertGreater(kl_val, 0.0)

    def test_02_classification_threshold_boundaries(self):
        """Validates proper regime transitions across stabilize, transition, drift, and critical."""
        # Genesis node
        frame_0, payload_0 = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation="train_step_0",
            current_state={"loss": 7.82},
            current_config={"lr": 4e-4},
            current_embedding=[0.1, 0.1],
            current_distribution=[0.5, 0.5],
            current_fitness=-7.82,
            ledger=self.ledger,
        )
        self.assertEqual(frame_0.drift_class, DriftClass.STABILIZE.value)

        # Case A: Minimal movement -> STABILIZE (drift < 0.05)
        frame_stab, payload_stab = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation="train_step_1",
            current_state={"loss": 7.81},
            current_config={"lr": 4e-4},
            current_embedding=[0.101, 0.101],
            current_distribution=[0.501, 0.499],
            current_fitness=-7.81,
            parent_frame=frame_0,
            parent_payload=payload_0,
            ledger=self.ledger,
        )
        self.assertEqual(frame_stab.drift_class, DriftClass.STABILIZE.value)
        self.assertLess(frame_stab.drift_score, 0.05)

        # Case B: Moderate displacement -> TRANSITION (0.05 <= drift < 0.20)
        frame_trans, payload_trans = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation="train_step_2",
            current_state={"loss": 7.60},
            current_config={"lr": 4e-4},
            current_embedding=[0.35, 0.35],
            current_distribution=[0.65, 0.35],
            current_fitness=-7.60,
            parent_frame=frame_stab,
            parent_payload=payload_stab,
            ledger=self.ledger,
        )
        self.assertEqual(frame_trans.drift_class, DriftClass.TRANSITION.value)
        self.assertTrue(0.05 <= frame_trans.drift_score < 0.20)

        # Case C: High displacement -> DRIFT (0.20 <= drift < 0.50)
        frame_drift, payload_drift = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation="train_step_3",
            current_state={"loss": 7.00},
            current_config={"lr": 4e-4},
            current_embedding=[0.65, 0.65],
            current_distribution=[0.75, 0.25],
            current_fitness=-7.00,
            parent_frame=frame_trans,
            parent_payload=payload_trans,
            ledger=self.ledger,
        )
        self.assertEqual(frame_drift.drift_class, DriftClass.DRIFT.value)
        self.assertTrue(0.20 <= frame_drift.drift_score < 0.50)

        # Case D: Extreme jump -> CRITICAL (drift >= 0.50)
        frame_crit, payload_crit = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation="train_step_4",
            current_state={"loss": 20.0},
            current_config={"lr": 4e-4},
            current_embedding=[5.0, 5.0],
            current_distribution=[0.999, 0.001],
            current_fitness=-20.0,
            parent_frame=frame_drift,
            parent_payload=payload_drift,
            ledger=self.ledger,
        )
        self.assertEqual(frame_crit.drift_class, DriftClass.CRITICAL.value)
        self.assertGreaterEqual(frame_crit.drift_score, 0.50)

    def test_03_cryptographic_lineage_and_immutability(self):
        """Verifies immutability of NexusFrame and SHA-256 parent lineage chaining."""
        frame_root, payload_root = symbolic_drift_checkpoint_engine(
            subsystem="compiler_ir",
            operation="instcombine",
            current_state={"instructions": 142},
            current_config={"opt_level": 2},
            current_embedding=[1.0, 2.0],
            current_distribution=[0.5, 0.5],
            current_fitness=1.0,
            ledger=self.ledger,
        )

        # Immutability check
        with self.assertRaises(FrozenInstanceError):
            frame_root.drift_score = 999.0  # type: ignore

        frame_child, _ = symbolic_drift_checkpoint_engine(
            subsystem="compiler_ir",
            operation="dead_code_elim",
            current_state={"instructions": 118},
            current_config={"opt_level": 2},
            current_embedding=[1.05, 2.05],
            current_distribution=[0.5, 0.5],
            current_fitness=1.2,
            parent_frame=frame_root,
            parent_payload=payload_root,
            ledger=self.ledger,
        )

        # Parent pointer integrity
        self.assertEqual(frame_child.parent_uuid, frame_root.node_uuid)
        # Lineage hash must depend on parent's lineage hash
        self.assertNotEqual(frame_child.lineage_hash, frame_root.lineage_hash)
        self.assertEqual(len(frame_child.lineage_hash), 64)

    def test_04_counterfactual_dag_replay(self):
        """Validates counterfactual branch replay and causal delta extraction."""
        # Create baseline observation
        observed_frame, observed_payload = symbolic_drift_checkpoint_engine(
            subsystem="ai_optimizer",
            operation="baseline_adamw_step",
            current_state={"step": 50, "loss": 6.2674},
            current_config={"optimizer": "adamw", "lr": 4e-4},
            current_embedding=[0.5, 0.5],
            current_distribution=[0.5, 0.5],
            current_fitness=-6.2674,
            ledger=self.ledger,
        )

        # Scenario A: Counterfactual optimizer (ZKAEDI Prime) achieves superior loss 5.9342
        def zkaedi_prime_evaluator(state, config):
            self.assertEqual(config["optimizer"], "zkaedi_prime")
            new_state = {"step": 50, "loss": 5.9342}
            cf_fitness = -5.9342
            cf_embedding = [0.45, 0.45]
            cf_distribution = [0.5, 0.5]
            violations = []
            metrics = {"memory_delta_bytes": 0}
            return new_state, cf_fitness, cf_embedding, cf_distribution, violations, metrics

        res_superior = counterfactual_dag_replay_engine(
            origin_checkpoint_uuid=observed_payload.checkpoint_uuid,
            counterfactual_config_overrides={"optimizer": "zkaedi_prime"},
            counterfactual_evaluator=zkaedi_prime_evaluator,
            ledger=self.ledger,
        )

        # Causal delta: (-5.9342) - (-6.2674) = +0.3332
        self.assertAlmostEqual(res_superior.causal_delta, 0.3332, places=4)
        self.assertEqual(res_superior.verdict, "SUPERIOR")
        self.assertGreater(res_superior.confidence, 0.8)
        self.assertEqual(len(res_superior.invariant_violations), 0)

        # Scenario B: Counterfactual branch causes an invariant violation
        def buggy_evaluator(state, config):
            return state, -99.0, [0.0], [1.0], ["ABI_REGISTER_CLOBBER_RAX"], {}

        res_breach = counterfactual_dag_replay_engine(
            origin_checkpoint_uuid=observed_payload.checkpoint_uuid,
            counterfactual_config_overrides={"pass": "experimental_pass"},
            counterfactual_evaluator=buggy_evaluator,
            ledger=self.ledger,
        )
        self.assertEqual(res_breach.verdict, "INVARIANT_BREACH")
        self.assertEqual(res_breach.confidence, 0.0)
        self.assertIn("ABI_REGISTER_CLOBBER_RAX", res_breach.invariant_violations)

    def test_05_nexus_supervisor_governance(self):
        """Validates autonomous supervisor policy enforcement and auditable frame emission."""
        sup_state = NexusSupervisorState()
        policy = SupervisorPolicy(max_consecutive_drifts=2, fitness_drop_tolerance=-0.5)

        # Step 1: Nominal stabilized frame -> CONTINUE
        f1, p1 = symbolic_drift_checkpoint_engine(
            subsystem="quantum_sim",
            operation="qec_cycle_1",
            current_state={"fidelity": 0.999},
            current_config={"error_p": 0.001},
            current_embedding=[0.01, 0.01],
            current_distribution=[0.5, 0.5],
            current_fitness=0.999,
            ledger=self.ledger,
        )
        dec1, sup_f1, _ = nexus_supervisor_control_loop(f1, p1, self.ledger, sup_state, policy)
        self.assertEqual(dec1, SupervisorDecision.CONTINUE)
        self.assertEqual(sup_f1.parent_uuid, f1.node_uuid)

        # Step 2: First DRIFT -> CONTINUE with warning
        f2, p2 = symbolic_drift_checkpoint_engine(
            subsystem="quantum_sim",
            operation="qec_cycle_2",
            current_state={"fidelity": 0.970},
            current_config={"error_p": 0.001},
            current_embedding=[0.8, 0.8],
            current_distribution=[0.8, 0.2],
            current_fitness=0.970,
            parent_frame=f1,
            parent_payload=p1,
            ledger=self.ledger,
        )
        dec2, sup_f2, _ = nexus_supervisor_control_loop(f2, p2, self.ledger, sup_state, policy)
        self.assertEqual(dec2, SupervisorDecision.CONTINUE)

        # Step 3: Second consecutive DRIFT -> THROTTLE
        f3, p3 = symbolic_drift_checkpoint_engine(
            subsystem="quantum_sim",
            operation="qec_cycle_3",
            current_state={"fidelity": 0.950},
            current_config={"error_p": 0.001},
            current_embedding=[1.2, 1.2],
            current_distribution=[0.85, 0.15],
            current_fitness=0.950,
            parent_frame=f2,
            parent_payload=p2,
            ledger=self.ledger,
        )
        dec3, sup_f3, dir3 = nexus_supervisor_control_loop(f3, p3, self.ledger, sup_state, policy)
        self.assertEqual(dec3, SupervisorDecision.THROTTLE)
        self.assertIn("throttle_factor", dir3)

        # Step 4: Catastrophic drop -> ROLLBACK to last stabilized checkpoint (p1)
        f4, p4 = symbolic_drift_checkpoint_engine(
            subsystem="quantum_sim",
            operation="qec_cycle_4",
            current_state={"fidelity": 0.200},
            current_config={"error_p": 0.001},
            current_embedding=[5.0, 5.0],
            current_distribution=[0.99, 0.01],
            current_fitness=0.200,
            parent_frame=f3,
            parent_payload=p3,
            ledger=self.ledger,
        )
        dec4, sup_f4, dir4 = nexus_supervisor_control_loop(f4, p4, self.ledger, sup_state, policy)
        self.assertEqual(dec4, SupervisorDecision.ROLLBACK)
        self.assertEqual(dir4["target_checkpoint_uuid"], str(p1.checkpoint_uuid))

    def test_06_dag_ledger_persistence_and_lineage_trace(self):
        """Validates append-only disk persistence and backward lineage tracing."""
        # Create small 3-node linear chain
        f0, p0 = symbolic_drift_checkpoint_engine("sub", "op0", {"x": 0}, {}, [0], [1], 0.0, ledger=self.ledger)
        f1, p1 = symbolic_drift_checkpoint_engine("sub", "op1", {"x": 1}, {}, [0.01], [1], 0.1, parent_frame=f0, parent_payload=p0, ledger=self.ledger)
        f2, p2 = symbolic_drift_checkpoint_engine("sub", "op2", {"x": 2}, {}, [0.02], [1], 0.2, parent_frame=f1, parent_payload=p1, ledger=self.ledger)

        lineage = self.ledger.get_lineage(f2.node_uuid)
        self.assertEqual(len(lineage), 3)
        self.assertEqual([f.node_uuid for f in lineage], [f0.node_uuid, f1.node_uuid, f2.node_uuid])

        # Verify disk journal files exist and have 3 lines
        journal_path = Path(self.temp_dir) / "nexus_frames.jsonl"
        self.assertTrue(journal_path.exists())
        with open(journal_path, "r", encoding="utf-8") as f:
            lines = [line.strip() for line in f if line.strip()]
        self.assertEqual(len(lines), 3)


if __name__ == "__main__":
    unittest.main()
