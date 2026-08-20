"""
SVG Control Plane Package
Strategic Monkey-Patch: Control Policy Hooking on GraphIR
"""
from .recursion import RecursionPolicyEnforcer, PolicyViolationError
from .audit import AuditLogger
from ..graph.graph import GraphIR

def _apply_control_monkey_patches():
    if getattr(GraphIR, "_patched_by_control", False):
        return

    # Attach policy-validated node insertion to GraphIR
    def add_node_with_policy(self, node, enforcer: RecursionPolicyEnforcer = None, logger: AuditLogger = None):
        if enforcer:
            enforcer.validate_expansion_request(depth=len(node.deps), breadth=1)
            enforcer.track_nodes_added(1)
            
        self.add_node(node)
        
        if logger:
            logger.log_event("NODE_ADDED", node.node_id, node.role, {"node_type": node.node_type.value})

    GraphIR.add_node_with_policy = add_node_with_policy
    GraphIR._patched_by_control = True

_apply_control_monkey_patches()

__all__ = ["RecursionPolicyEnforcer", "AuditLogger", "PolicyViolationError"]
