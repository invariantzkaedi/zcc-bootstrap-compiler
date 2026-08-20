import hashlib
import json
from typing import Dict, Any, List, Optional
from ..agents.generator import Candidate

class SelfApprovalError(Exception):
    pass

class VerificationResult:
    def __init__(
        self,
        verified: bool,
        verifier_role: str,
        candidate_id: str,
        score: float,
        failure_signature: Optional[str] = None,
        details: Dict[str, Any] = None
    ):
        self.verified = verified
        self.verifier_role = verifier_role
        self.candidate_id = candidate_id
        self.score = score
        self.failure_signature = failure_signature
        self.details = details or {}

    def to_dict(self) -> Dict[str, Any]:
        return {
            "verified": self.verified,
            "verifier_role": self.verifier_role,
            "candidate_id": self.candidate_id,
            "score": self.score,
            "failure_signature": self.failure_signature,
            "details": self.details
        }

class IndependentVerifier:
    def __init__(self, verifier_role: str = "A6-Verifier"):
        self.verifier_role = verifier_role

    def verify_candidate(self, candidate: Candidate, min_score_threshold: float = 0.80) -> VerificationResult:
        # Strict rule: Generators cannot approve their own work
        if candidate.generator_role == self.verifier_role:
            raise SelfApprovalError(
                f"Assurance Violation: Generator '{candidate.generator_role}' cannot act as Verifier '{self.verifier_role}'"
            )

        score = candidate.compute_weighted_score()
        passed = score >= min_score_threshold
        
        failure_sig = None
        if not passed:
            # Emit failure signature per failure.schema.json
            sig_raw = f"LOW_SCORE:{candidate.node_id}:{candidate.candidate_id}:{score}"
            failure_sig = f"FAIL-SIG-{hashlib.sha256(sig_raw.encode('utf-8')).hexdigest()[:12]}"

        return VerificationResult(
            verified=passed,
            verifier_role=self.verifier_role,
            candidate_id=candidate.candidate_id,
            score=score,
            failure_signature=failure_sig,
            details={
                "scores_breakdown": candidate.scores,
                "generator_role": candidate.generator_role,
                "node_id": candidate.node_id
            }
        )
