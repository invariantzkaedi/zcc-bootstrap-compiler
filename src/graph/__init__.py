"""
SVG Graph IR Package
Strategic Monkey-Patch: Graph Visualization & Structural Digest Utilities
"""
import hashlib
from .graph import GraphIR, Node, NodeType

def _apply_graph_monkey_patches():
    if getattr(Node, "_patched_by_graph", False):
        return

    # 1. Attach deterministic digest method to Node
    def digest(self) -> str:
        raw = f"{self.node_id}:{self.node_type.value}:{self.role}:{','.join(sorted(self.deps))}"
        return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:16]

    # 2. Attach Mermaid visualizer helper to Node
    def to_mermaid(self) -> str:
        return f'    {self.node_id}["{self.node_id} ({self.node_type.value})"]'

    # 3. Attach Mermaid visualizer helper to GraphIR
    def graph_to_mermaid(self) -> str:
        lines = ["graph TD"]
        for node in self.nodes.values():
            lines.append(node.to_mermaid())
        for edge in self.edges:
            lines.append(f'    {edge["from"]} --> {edge["to"]}')
        return "\n".join(lines)

    Node.digest = digest
    Node.to_mermaid = to_mermaid
    GraphIR.to_mermaid = graph_to_mermaid
    
    Node._patched_by_graph = True
    GraphIR._patched_by_graph = True

_apply_graph_monkey_patches()

__all__ = ["GraphIR", "Node", "NodeType"]
