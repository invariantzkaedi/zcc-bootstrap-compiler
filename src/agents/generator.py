import hashlib
import json
from typing import Dict, Any, List

class Candidate:
    def __init__(
        self,
        candidate_id: str,
        node_id: str,
        generator_role: str,
        content: Any,
        scores: Dict[str, float] = None
    ):
        self.candidate_id = candidate_id
        self.node_id = node_id
        self.generator_role = generator_role
        self.content = content
        self.scores = scores or {
            "tests": 0.0,
            "static": 0.0,
            "security": 0.0,
            "performance": 0.0,
            "maintainability": 0.0,
            "cost": 0.0
        }

    def compute_weighted_score(self) -> float:
        weights = {
            "tests": 0.30,
            "static": 0.20,
            "security": 0.20,
            "performance": 0.15,
            "maintainability": 0.10,
            "cost": 0.05
        }
        return sum(self.scores.get(k, 0.0) * w for k, w in weights.items())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "candidate_id": self.candidate_id,
            "node_id": self.node_id,
            "generator_role": self.generator_role,
            "content": self.content,
            "scores": self.scores,
            "weighted_score": round(self.compute_weighted_score(), 4)
        }

class CandidateGenerator:
    def __init__(self, role: str = "A2-Coder"):
        self.role = role

    def generate_candidates(self, node_id: str, seed_data: Dict[str, Any], count: int = 3) -> List[Candidate]:
        candidates = []
        for i in range(count):
            cid = f"cand-{node_id}-{i+1}"
            content = f"Generated candidate variant {i+1} for node {node_id}"
            
            # Deterministic initial scoring for baseline evaluation
            h = hashlib.md5(f"{node_id}-{i}".encode("utf-8")).hexdigest()
            base_val = int(h[:4], 16) / 65535.0
            
            scores = {
                "tests": round(0.70 + (base_val * 0.30), 2),
                "static": round(0.80 + (base_val * 0.20), 2),
                "security": round(0.90 + (base_val * 0.10), 2),
                "performance": round(0.85 + (base_val * 0.15), 2),
                "maintainability": round(0.75 + (base_val * 0.25), 2),
                "cost": round(0.95, 2)
            }
            
            cand = Candidate(
                candidate_id=cid,
                node_id=node_id,
                generator_role=self.role,
                content=content,
                scores=scores
            )
            candidates.append(cand)
            
        return candidates
