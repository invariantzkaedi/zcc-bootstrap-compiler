# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // FAST PATHFINDER & TWO-REGIME ESCAPE ENGINE (FORM 5) 🔱
=======================================================================================================
 The Navigation Regime:
   - eta = 0.0 (Zero field drag / maximum navigation velocity)
   - Scars: H_base[u*] += kick (Piles an energy mountain over visited/trapped nodes)
   - Epsilon: Stochastic tie-breaker preventing stagnation in symmetrical saddle points
=======================================================================================================
"""

from typing import List, Tuple, Set, Optional
import numpy as np

class TwoRegimePathfinder:
    """
    Form 5 of Zkaedi Prime.
    Solves combinatorial mazes, AST traversals, and bug-escape paths
    using departure scars and epsilon tie-breakers with zero field viscosity (eta = 0).
    """
    def __init__(
        self,
        num_nodes: int,
        adjacency_matrix: np.ndarray,
        kick: float = 2.00,
        eps: float = 0.05,
        seed: int = 42
    ):
        self.num_nodes = num_nodes
        self.adj = adjacency_matrix.astype(np.float64).copy()
        self.kick = kick
        self.eps = eps
        self.rng = np.random.default_rng(seed)

        self.H_base = np.zeros(num_nodes, dtype=np.float64)
        self.visited_counts = np.zeros(num_nodes, dtype=np.int32)

    def find_path(
        self,
        start_node: int,
        target_nodes: Set[int],
        max_steps: int = 500
    ) -> Tuple[List[int], bool]:
        """
        Navigates from start_node to any node in target_nodes.
        Returns: (path_traversed, success)
        """
        current = start_node
        path = [current]
        self.visited_counts[current] += 1

        for _ in range(max_steps):
            if current in target_nodes:
                return path, True

            # Get neighbors (nodes where adj > 0)
            neighbors = np.where(self.adj[current] > 0)[0]
            if len(neighbors) == 0:
                break

            # Compute potential energy of neighbors + epsilon tie-breaker
            noise = self.eps * self.rng.normal(0.0, 1.0, size=len(neighbors))
            energies = self.H_base[neighbors] + noise

            # Move to minimal energy neighbor
            best_idx = np.argmin(energies)
            next_node = int(neighbors[best_idx])

            # DEPARTURE SCAR EVENT:
            # Pile an energy mountain over the node we are leaving!
            self.H_base[current] += self.kick

            current = next_node
            path.append(current)
            self.visited_counts[current] += 1

        return path, current in target_nodes
