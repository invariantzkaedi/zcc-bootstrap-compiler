"""
SVG Assurance Plane Package
Strategic Monkey-Patch: Seamless Candidate Verification Delegation
"""
from .verifier import IndependentVerifier, VerificationResult, SelfApprovalError
from ..agents.generator import Candidate

def _apply_assurance_monkey_patches():
    if getattr(Candidate, "_patched_by_assurance", False):
        return

    def verify_with(self, verifier_role: str = "A6-Verifier", min_score_threshold: float = 0.80) -> VerificationResult:
        verifier = IndependentVerifier(verifier_role=verifier_role)
        return verifier.verify_candidate(self, min_score_threshold=min_score_threshold)

    Candidate.verify_with = verify_with
    Candidate._patched_by_assurance = True

_apply_assurance_monkey_patches()

__all__ = ["IndependentVerifier", "VerificationResult", "SelfApprovalError"]
