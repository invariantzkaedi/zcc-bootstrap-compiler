from __future__ import annotations

import hashlib
import json
import math
import tempfile
import unittest
from pathlib import Path
from uuid import uuid4

from nexus import (
    DriftClass,
    DriftThresholds,
    DriftWeights,
    NexusDAGLedger,
    SupervisorDecision,
    classify_drift,
    counterfactual_dag_replay_engine,
    kl_divergence,
    l2_displacement,
    nexus_supervisor_control_loop,
    symbolic_drift_checkpoint_engine,
)
from nexus.ledger import canonical_json_bytes, lineage_hash, sha256_hex


class NexusLegendaryTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory(prefix="nexus-test-")
        self.ledger = NexusDAGLedger(Path(self.tmp.name))

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def emit(
        self,
        *,
        latent,
        previous_latent,
        distribution,
        previous_distribution,
        fitness,
        previous_fitness,
        thresholds=DriftThresholds(),
        weights=DriftWeights(),
        parent_uuid=None,
        confidence=1.0,
        operation="test.step",
    ):
        state = {"latent": list(latent), "distribution": list(distribution), "value": fitness}
        return symbolic_drift_checkpoint_engine(
            ledger=self.ledger,
            subsystem="test",
            operation=operation,
            current_state=state,
            config={"mode": "test"},
            current_latent=latent,
            previous_latent=previous_latent,
            current_distribution=distribution,
            previous_distribution=previous_distribution,
            current_fitness=fitness,
            previous_fitness=previous_fitness,
            thresholds=thresholds,
            weights=weights,
            parent_uuid=parent_uuid,
            confidence=confidence,
            timestamp_ns=1_700_000_000_000_000_000 + len(list(self.ledger.iter_frames())),
        )

    def test_telemetry_and_drift_math_exact_components(self) -> None:
        weights = DriftWeights(alpha=2.0, beta=3.0, gamma=5.0, delta=7.0)
        current = [3.0, 4.0]
        previous = [0.0, 0.0]
        p = [0.75, 0.25]
        q = [0.5, 0.5]
        h_cur = -sum(x * math.log(x) for x in p)
        h_prev = -sum(x * math.log(x) for x in q)
        kl = sum(pi * math.log(pi / qi) for pi, qi in zip(p, q))
        expected = 2.0 * 5.0 + 3.0 * kl + 5.0 * abs(h_cur - h_prev) + 7.0 * 0.2

        frame = symbolic_drift_checkpoint_engine(
            ledger=self.ledger,
            subsystem="math",
            operation="exact",
            current_state={"latent": current, "distribution": p},
            config={"a": 1},
            current_latent=current,
            previous_latent=previous,
            current_distribution=p,
            previous_distribution=q,
            current_fitness=1.2,
            previous_fitness=1.0,
            weights=weights,
            thresholds=DriftThresholds(1.0, 5.0, 20.0),
        )
        delta, l2 = l2_displacement(current, previous)
        self.assertEqual(delta, (3.0, 4.0))
        self.assertEqual(l2, 5.0)
        self.assertAlmostEqual(kl_divergence(p, q), kl, places=12)
        self.assertAlmostEqual(frame.drift_score, expected, places=12)

    def test_boundary_classification(self) -> None:
        t = DriftThresholds(tau_s=1.0, tau_d=2.0, tau_c=3.0)
        self.assertEqual(classify_drift(0.999999, t), DriftClass.STABILIZE)
        self.assertEqual(classify_drift(1.0, t), DriftClass.TRANSITION)
        self.assertEqual(classify_drift(1.999999, t), DriftClass.TRANSITION)
        self.assertEqual(classify_drift(2.0, t), DriftClass.DRIFT)
        self.assertEqual(classify_drift(2.999999, t), DriftClass.DRIFT)
        self.assertEqual(classify_drift(3.0, t), DriftClass.CRITICAL)

    def test_cryptographic_lineage_and_state_hash_verification(self) -> None:
        first = self.emit(
            latent=[0.0], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.0, previous_fitness=1.0,
        )
        second = self.emit(
            latent=[0.1], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.1, previous_fitness=1.0,
            parent_uuid=first.node_uuid,
        )
        expected_first = lineage_hash(None, first.node_uuid)
        expected_second = lineage_hash(first.lineage_hash, second.node_uuid)
        self.assertEqual(first.lineage_hash, expected_first)
        self.assertEqual(second.lineage_hash, expected_second)

        cp = self.ledger.load_checkpoint(second.checkpoint_uuid)
        self.assertEqual(cp.state_hash, sha256_hex(cp.state))
        ok, errors = self.ledger.verify()
        self.assertTrue(ok, errors)

        # Tamper with the persisted checkpoint payload without updating its hash.
        path = self.ledger.checkpoint_dir / f"{second.checkpoint_uuid}.json"
        payload = json.loads(path.read_text("utf-8"))
        payload["state"]["value"] = 999
        path.write_text(json.dumps(payload), "utf-8")
        ok, errors = self.ledger.verify()
        self.assertFalse(ok)
        self.assertTrue(any("state_hash mismatch" in e for e in errors))

    def test_counterfactual_branch_replay(self) -> None:
        ancestor = self.emit(
            latent=[0.0, 0.0], previous_latent=[0.0, 0.0],
            distribution=[0.5, 0.5], previous_distribution=[0.5, 0.5],
            fitness=10.0, previous_fitness=10.0,
        )

        def replay_fn(state, config):
            new_state = {
                "latent": [0.2, 0.1],
                "distribution": [0.6, 0.4],
                "candidate": config["candidate"],
            }
            return new_state, 12.5, new_state["latent"], new_state["distribution"], 200, 2048

        result = counterfactual_dag_replay_engine(
            ledger=self.ledger,
            checkpoint_uuid=ancestor.checkpoint_uuid,
            alternate_config={"candidate": "alternate"},
            replay_fn=replay_fn,
            invariant_checks=[lambda state: None if state["candidate"] == "alternate" else "bad candidate"],
        )
        self.assertEqual(result.observed_fitness, 10.0)
        self.assertEqual(result.counterfactual_fitness, 12.5)
        self.assertEqual(result.causal_delta, 2.5)
        self.assertEqual(result.frame.parent_uuid, ancestor.node_uuid)
        self.assertEqual(result.invariant_violations, ())
        self.assertEqual(
            self.ledger.load_checkpoint(result.branch_checkpoint_uuid).state["candidate"],
            "alternate",
        )
        ok, errors = self.ledger.verify()
        self.assertTrue(ok, errors)

    def test_supervisor_control_loop(self) -> None:
        thresholds = DriftThresholds(tau_s=0.1, tau_d=0.5, tau_c=1.0)
        # Stable anchor, but no positive improvement -> CONTINUE.
        stable = self.emit(
            latent=[0.0], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.0, previous_fitness=1.0,
            thresholds=thresholds,
        )
        # Score 0.7 -> DRIFT -> THROTTLE.
        drift = self.emit(
            latent=[0.7], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.0, previous_fitness=1.0,
            thresholds=thresholds,
            parent_uuid=stable.node_uuid,
        )
        # Score 1.2 -> CRITICAL -> ROLLBACK to stable.
        critical = self.emit(
            latent=[1.2], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.0, previous_fitness=1.0,
            thresholds=thresholds,
            parent_uuid=drift.node_uuid,
        )
        # Transition-range source, but injected invariant -> QUARANTINE.
        faulted = self.emit(
            latent=[0.2], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.0, previous_fitness=1.0,
            thresholds=thresholds,
            parent_uuid=critical.node_uuid,
        )

        events = nexus_supervisor_control_loop(
            [stable, drift, critical, faulted],
            ledger=self.ledger,
            invariant_violations={faulted.node_uuid: ("ABI invariant violated",)},
        )
        self.assertEqual(
            [e.decision for e in events],
            [
                SupervisorDecision.CONTINUE,
                SupervisorDecision.THROTTLE,
                SupervisorDecision.ROLLBACK,
                SupervisorDecision.QUARANTINE,
            ],
        )
        self.assertEqual(events[2].rollback_checkpoint_uuid, stable.checkpoint_uuid)
        self.assertTrue(all(e.decision_frame.parent_uuid == e.source_node_uuid for e in events))
        ok, errors = self.ledger.verify()
        self.assertTrue(ok, errors)

    def test_ledger_record_tamper_detection(self) -> None:
        self.emit(
            latent=[0.0], previous_latent=[0.0],
            distribution=[1.0], previous_distribution=[1.0],
            fitness=1.0, previous_fitness=1.0,
        )
        lines = self.ledger.ledger_path.read_text("utf-8").splitlines()
        env = json.loads(lines[0])
        env["payload"]["operation"] = "tampered"
        self.ledger.ledger_path.write_text(json.dumps(env) + "\n", "utf-8")
        ok, errors = self.ledger.verify()
        self.assertFalse(ok)
        self.assertTrue(any("record_hash mismatch" in e or "payload_hash mismatch" in e for e in errors))


if __name__ == "__main__":
    unittest.main()
