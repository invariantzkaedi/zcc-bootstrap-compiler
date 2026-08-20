"""
SVG Execution Plane Package
Strategic Monkey-Patch: Candidate Provenance Tagging & Metadata Utility
"""
from .generator import CandidateGenerator, Candidate

def _apply_agents_monkey_patches():
    if getattr(Candidate, "_patched_by_agents", False):
        return

    def add_tag(self, tag: str):
        if not hasattr(self, "_tags"):
            self._tags = []
        if tag not in self._tags:
            self._tags.append(tag)

    def get_tags(self) -> list:
        return getattr(self, "_tags", [])

    def get_provenance_header(self) -> dict:
        return {
            "candidate_id": self.candidate_id,
            "generator_role": self.generator_role,
            "node_id": self.node_id,
            "tags": self.get_tags()
        }

    Candidate.add_tag = add_tag
    Candidate.get_tags = get_tags
    Candidate.get_provenance_header = get_provenance_header
    Candidate._patched_by_agents = True

_apply_agents_monkey_patches()

__all__ = ["CandidateGenerator", "Candidate"]
