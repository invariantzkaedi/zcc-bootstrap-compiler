#!/usr/bin/env python3
"""
tests/test_zcc_quantum_unified_cov.py — Python Coverage Suite for Fused Quantum Core
Tests:
  1. Spectral Form Factor and Level Spacing Ratio on CFG eigenvalues.
  2. Quantum Walk Mutation Tournament Selection & Phase Cancellation.
  3. Transverse-Field Ising Spin-Glass Register Allocation.
"""

import unittest
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
sys.path.insert(0, str(REPO_ROOT))

from tools.quantum_unified_bridge import (
    analyze_cfg_quantum,
    select_mutations_quantum,
    solve_regalloc_quantum,
)

class TestZccQuantumUnifiedCoverage(unittest.TestCase):

    def test_module_1_spectral_form_factor(self):
        """Module 1: Spectral Form Factor analysis on synthetic CFG eigenvalues."""
        evals = [float(i) * 1.2 for i in range(20)]
        res = analyze_cfg_quantum(evals)
        self.assertIn("spectral_dim", res)
        self.assertIn("r_spacing_ratio", res)
        self.assertGreater(res["spectral_dim"], 0.0)
        self.assertGreaterEqual(res["r_spacing_ratio"], 0.0)

    def test_module_2_mutation_tournament(self):
        """Module 2: Quantum Walk Mutation Superposition Tournament."""
        conflict_mat = [0.0] * 256
        # Mut 0 and Mut 1 conflict
        conflict_mat[0 * 16 + 1] = 1.0
        conflict_mat[1 * 16 + 0] = 1.0

        fitness_deltas = [0.0] * 16
        fitness_deltas[0] = 20.0  # High positive reward
        fitness_deltas[1] = 2.0   # Low reward

        winners = select_mutations_quantum(
            candidates=["mut0", "mut1", "mut2", "mut3"],
            conflict_mat=conflict_mat,
            fitness_deltas=fitness_deltas,
            max_select=2
        )
        self.assertIsInstance(winners, list)
        self.assertLessEqual(len(winners), 2)
        # Verify Mut 0 and Mut 1 are not both chosen
        self.assertFalse(0 in winners and 1 in winners)

    def test_module_3_ising_register_allocator(self):
        """Module 3: Transverse-Field Ising Quantum Register Allocation."""
        interference = [0.0] * 256
        # Graph with 3 variables: v0-v1 and v1-v2 interfere
        interference[0 * 3 + 1] = 1.0; interference[1 * 3 + 0] = 1.0
        interference[1 * 3 + 2] = 1.0; interference[2 * 3 + 1] = 1.0

        colors, spills = solve_regalloc_quantum(interference, num_vars=3, num_colors=2)
        self.assertEqual(len(colors), 3)
        self.assertEqual(spills, 0)
        self.assertNotEqual(colors[0], colors[1])
        self.assertNotEqual(colors[1], colors[2])


if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestZccQuantumUnifiedCoverage)
    runner = unittest.TextTestRunner(verbosity=2)
    res = runner.run(suite)
    os._exit(0 if res.wasSuccessful() else 1)
