# -*- coding: utf-8 -*-
"""
Unit tests for OTOC Butterfly Scrambling Speculative Filter.
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.npu.otoc_speculative_filter import OtocSpeculativeFilter


class TestOtocSpeculativeFilter(unittest.TestCase):

    def setUp(self):
        self.filter = OtocSpeculativeFilter(lyapunov_threshold=0.50)

    def test_valid_syntax_low_lyapunov(self):
        # Valid C syntax should have lambda_L <= 0.15 and F >= 0.85
        pert = self.filter.compute_syntax_perturbation("int *p = ", "arr;")
        self.assertEqual(pert, 0.0)
        lam, f_final, _ = self.filter.evaluate_otoc_correlator(pert)
        self.assertLessEqual(lam, 0.15)
        self.assertGreaterEqual(f_final, 0.85)

    def test_corrupt_macro_high_lyapunov_burst(self):
        # Corrupt macro tokens should trigger a massive Lyapunov burst
        pert = self.filter.compute_syntax_perturbation("", "sum += %broken$*;")
        self.assertGreater(pert, 2.0)
        lam, f_final, _ = self.filter.evaluate_otoc_correlator(pert)
        self.assertGreater(lam, 1.0)
        self.assertLess(f_final, 0.4)

    def test_unbalanced_parentheses_pruned(self):
        pert = self.filter.compute_syntax_perturbation("", "int val = ((a + b);")
        self.assertGreater(pert, 1.0)
        lam, _, _ = self.filter.evaluate_otoc_correlator(pert)
        self.assertGreater(lam, 0.50)

    def test_batch_filtering_speed_and_accuracy(self):
        candidates = [
            {"id": 0, "token": "int arr[5] = {1, 2, 3, 4, 5};"},
            {"id": 1, "token": "int *ptr = arr;"},
            {"id": 2, "token": "sum += %invalid%;"},
            {"id": 3, "token": "val = (10 + 20);"}
        ]
        res = self.filter.filter_speculative_branches(candidates)
        self.assertEqual(res["total_candidates"], 4)
        self.assertEqual(res["accepted_count"], 3)
        self.assertEqual(res["pruned_count"], 1)
        self.assertEqual(res["pruned_branches"][0]["id"], 2)
        self.assertLess(res["filter_latency_ms"], 5.0)


if __name__ == "__main__":
    unittest.main()
