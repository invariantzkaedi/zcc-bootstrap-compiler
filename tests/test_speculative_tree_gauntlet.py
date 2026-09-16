# -*- coding: utf-8 -*-
"""
Unit tests for Full Speculative Tree Decoding Gauntlet (tools/npu/speculative_tree_gauntlet.py).
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.npu.speculative_tree_gauntlet import SpeculativeTreeGauntlet, BENCHMARK_TASKS


class TestSpeculativeTreeGauntlet(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gauntlet = SpeculativeTreeGauntlet()

    def test_speculative_tree_topology(self):
        """Test that the 16-node speculative tree has valid attention mask and depths."""
        tree = self.gauntlet.tree_engine.generate_speculative_tree("int sum = 0;")
        self.assertEqual(tree["tree_size"], 16)
        self.assertEqual(len(tree["tokens"]), 16)
        self.assertEqual(len(tree["parents"]), 16)
        self.assertEqual(len(tree["depths"]), 16)
        mask = tree["tree_attention_mask"]
        self.assertEqual(len(mask), 16)
        self.assertEqual(len(mask[0]), 16)

    def test_hom_boson_bunching_dip(self):
        """Test Hong-Ou-Mandel quantum bunching dip reaches 0 coincidence."""
        p11_zero = self.gauntlet.hom_sampler.evaluate_hom_dip(0.0)
        self.assertAlmostEqual(p11_zero, 0.0, places=5)

    def test_otoc_scrambling_early_rejection(self):
        """Test that chaotic syntax branches are pruned by the OTOC filter."""
        candidates = [
            "int *ptr = arr;",
            "%broken_macro$*;",
            "sum += *(ptr + i);"
        ]
        res = self.gauntlet.otoc_filter.filter_speculative_branches(candidates)
        self.assertEqual(res["total_candidates"], 3)
        self.assertGreaterEqual(res["pruned_count"], 1)
        self.assertIn("broken_macro", res["pruned_branches"][0]["token"])

    def test_in_memory_jit_execution(self):
        """Test zero-disk in-memory JIT execution completes in sub-millisecond."""
        res = self.gauntlet.jit_engine.execute_c_code("int main() { return 0; }")
        self.assertEqual(res["exit_code"], 0)
        self.assertEqual(res["verdict"], "PASS")
        self.assertLess(res["latency_ms"], 2.5)

    def test_full_benchmark_task(self):
        """Test end-to-end execution of a benchmark task."""
        task = BENCHMARK_TASKS[0]
        res = self.gauntlet.run_benchmark_task(task)
        self.assertEqual(res["verdict"], "PASS")
        self.assertGreater(res["speedup_factor"], 1.0)
        self.assertIn("dual_silicon_speculative", res)


if __name__ == "__main__":
    unittest.main()
