import os
import json
from typing import Dict, Any, List, Optional

class PolicyViolationError(Exception):
    pass

class RecursionPolicyEnforcer:
    def __init__(self, policy_path: str = "svg_agentic_ide_handoff/policies/recursion-policy.json"):
        self.policy_path = policy_path
        self.policy = self._load_policy()
        
        self.max_depth = self.policy.get("max_depth", 12)
        self.max_breadth = self.policy.get("max_breadth", 32)
        self.max_total_nodes = self.policy.get("max_total_nodes_per_run", 50000)
        self.max_candidates = self.policy.get("max_candidates_per_node", 8)
        self.stop_conditions = self.policy.get("stop_on", [])
        
        self.current_nodes_count = 0
        self.failure_signatures: Dict[str, int] = {}

    def _load_policy(self) -> Dict[str, Any]:
        if os.path.exists(self.policy_path):
            with open(self.policy_path, "r", encoding="utf-8") as f:
                return json.load(f)
        return {
            "max_depth": 12,
            "max_breadth": 32,
            "max_total_nodes_per_run": 50000,
            "max_candidates_per_node": 8,
            "stop_on": ["budget_exhausted", "same_failure_signature_3x", "policy_violation", "blast_radius_widening"]
        }

    def validate_expansion_request(self, depth: int, breadth: int, candidate_count: int = 1) -> bool:
        if depth > self.max_depth:
            raise PolicyViolationError(f"Requested depth {depth} exceeds max depth {self.max_depth}")
        if breadth > self.max_breadth:
            raise PolicyViolationError(f"Requested breadth {breadth} exceeds max breadth {self.max_breadth}")
        if candidate_count > self.max_candidates:
            raise PolicyViolationError(f"Candidate count {candidate_count} exceeds limit {self.max_candidates}")
        if self.current_nodes_count + breadth > self.max_total_nodes:
            raise PolicyViolationError(f"Total node count would exceed run cap {self.max_total_nodes}")
        return True

    def register_failure(self, failure_signature: str):
        self.failure_signatures[failure_signature] = self.failure_signatures.get(failure_signature, 0) + 1
        if self.failure_signatures[failure_signature] >= 3:
            raise PolicyViolationError(f"Same failure signature occurred {self.failure_signatures[failure_signature]} times: {failure_signature}")

    def track_nodes_added(self, count: int):
        self.current_nodes_count += count
