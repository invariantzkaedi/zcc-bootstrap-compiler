import unittest
import os
import json
from src.graph import GraphIR, Node, NodeType
from src.control import RecursionPolicyEnforcer, AuditLogger, PolicyViolationError
from src.agents import CandidateGenerator, Candidate
from src.assurance import IndependentVerifier, SelfApprovalError

class TestCode001Runtime(unittest.TestCase):
    def setUp(self):
        self.test_events = "events/test_events.jsonl"
        self.test_prov = "provenance/test_provenance.jsonl"

    def tearDown(self):
        if os.path.exists(self.test_events):
            os.remove(self.test_events)
        if os.path.exists(self.test_prov):
            os.remove(self.test_prov)

    def test_graph_ingestion_and_monkey_patches(self):
        graph = GraphIR()
        node = Node("SPEC-001", NodeType.SPEC, "A1-Planner", deps=[], risk="medium")
        graph.add_node(node)
        
        self.assertIn("SPEC-001", graph.nodes)
        # Test monkey-patched methods from src.graph
        self.assertTrue(hasattr(node, "to_mermaid"))
        self.assertTrue(hasattr(graph, "to_mermaid"))
        self.assertIn("SPEC-001", graph.to_mermaid())

    def test_control_monkey_patch(self):
        graph = GraphIR()
        enforcer = RecursionPolicyEnforcer()
        logger = AuditLogger(events_path=self.test_events, provenance_path=self.test_prov)
        
        node = Node("CODE-001", NodeType.CODE, "A2-Coder", deps=["SPEC-001"])
        # Test monkey-patched add_node_with_policy from src.control
        graph.add_node_with_policy(node, enforcer=enforcer, logger=logger)
        self.assertIn("CODE-001", graph.nodes)

    def test_agents_and_assurance_monkey_patches(self):
        gen = CandidateGenerator(role="A2-Coder")
        cands = gen.generate_candidates("CODE-001", {}, count=1)
        cand = cands[0]
        
        # Test monkey-patched methods from src.agents
        cand.add_tag("verified-candidate")
        self.assertIn("verified-candidate", cand.get_tags())
        self.assertEqual(cand.get_provenance_header()["generator_role"], "A2-Coder")

        # Test monkey-patched method from src.assurance
        res = cand.verify_with(verifier_role="A6-Verifier")
        self.assertEqual(res.verifier_role, "A6-Verifier")
        self.assertTrue(res.verified)

        # Confirm non-self-approval rule remains intact under monkey patch
        with self.assertRaises(SelfApprovalError):
            cand.verify_with(verifier_role="A2-Coder")

if __name__ == "__main__":
    unittest.main()
