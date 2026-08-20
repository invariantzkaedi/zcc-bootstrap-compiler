import os
import json
import zipfile
import hashlib
from typing import Dict, List, Any, Optional
from enum import Enum

class NodeType(str, Enum):
    SPEC = "SPEC"
    CODE = "CODE"
    TEST = "TEST"
    SEC = "SEC"
    PERF = "PERF"
    REL = "REL"
    VER = "VER"

class Node:
    def __init__(
        self,
        node_id: str,
        node_type: NodeType,
        role: str,
        deps: List[str] = None,
        risk: str = "medium",
        output: Any = None,
        metadata: Dict[str, Any] = None,
        provenance_parent_refs: List[str] = None
    ):
        self.node_id = node_id
        self.node_type = NodeType(node_type)
        self.role = role
        self.deps = deps or []
        self.risk = risk
        self.output = output
        self.metadata = metadata or {}
        self.provenance_parent_refs = provenance_parent_refs or []

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.node_id,
            "type": self.node_type.value,
            "role": self.role,
            "deps": self.deps,
            "risk": self.risk,
            "output": self.output,
            "metadata": self.metadata,
            "provenance_parent_refs": self.provenance_parent_refs
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Node':
        return cls(
            node_id=data.get("id") or data.get("node_id"),
            node_type=data.get("type") or data.get("node_type", "SPEC"),
            role=data.get("role", "A1-Planner"),
            deps=data.get("deps", []),
            risk=data.get("risk", "medium"),
            output=data.get("output"),
            metadata=data.get("metadata", {}),
            provenance_parent_refs=data.get("provenance_parent_refs", [])
        )

class GraphIR:
    def __init__(self):
        self.nodes: Dict[str, Node] = {}
        self.edges: List[Dict[str, str]] = []
        self.bundles_indexed: List[str] = []

    def add_node(self, node: Node):
        self.nodes[node.node_id] = node
        for dep in node.deps:
            self.edges.append({"from": dep, "to": node.node_id})

    def ingest_bundle(self, bundle_path: str):
        if not os.path.exists(bundle_path):
            # Record missing/referenced path without crashing
            self.bundles_indexed.append(f"MISSING:{bundle_path}")
            return
        
        digest = hashlib.sha256()
        with open(bundle_path, 'rb') as f:
            while chunk := f.read(8192):
                digest.update(chunk)

        if zipfile.is_zipfile(bundle_path):
            with zipfile.ZipFile(bundle_path, 'r') as z:
                for name in z.namelist():
                    if name.endswith('.json') or name.endswith('.jsonl'):
                        content = z.read(name).decode('utf-8', errors='ignore')
                        self._parse_content(content, name)
        else:
            with open(bundle_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                self._parse_content(content, bundle_path)
                
        self.bundles_indexed.append(f"{bundle_path}:{digest.hexdigest()[:12]}")

    def _parse_content(self, content: str, source_name: str):
        for line in content.splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
                if isinstance(data, dict):
                    if "id" in data or "node_id" in data or "task_id" in data:
                        node_id = data.get("id") or data.get("node_id") or data.get("task_id")
                        node = Node(
                            node_id=node_id,
                            node_type=data.get("type", "SPEC"),
                            role=data.get("role", "A1-Planner"),
                            deps=data.get("deps", data.get("dependencies", [])),
                            risk=data.get("risk", "medium"),
                            output=data.get("output", data.get("expected_outputs")),
                            metadata={"source": source_name, "raw": data}
                        )
                        self.add_node(node)
            except Exception:
                continue

    def to_dict(self) -> Dict[str, Any]:
        return {
            "nodes": [node.to_dict() for node in self.nodes.values()],
            "edges": self.edges,
            "bundles_indexed": self.bundles_indexed
        }
