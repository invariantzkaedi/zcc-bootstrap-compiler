# -*- coding: utf-8 -*-
"""
Unit tests for DirectML Two-Particle Hong-Ou-Mandel Boson Sampler.
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.npu.directml_hom_boson_sampler import DirectMlHomBosonSampler


class TestDirectMlHomBosonSampler(unittest.TestCase):

    def test_bosonic_hom_dip_zero(self):
        sampler = DirectMlHomBosonSampler(num_sites=8, statistics="boson")
        p0 = sampler.evaluate_hom_dip(0.0)
        self.assertAlmostEqual(p0, 0.0, places=6, msg="Boson coincidence probability at zero delay must be 0.0")

    def test_fermionic_anti_bunching(self):
        sampler = DirectMlHomBosonSampler(num_sites=8, statistics="fermion")
        p0 = sampler.evaluate_hom_dip(0.0)
        self.assertAlmostEqual(p0, 1.0, places=6, msg="Fermion coincidence probability at zero delay must be 1.0")

    def test_distinguishable_limit(self):
        sampler = DirectMlHomBosonSampler(num_sites=8, statistics="boson")
        # At large delay, particles become distinguishable: coincidence approaches 0.5
        p_far = sampler.evaluate_hom_dip(10.0)
        self.assertAlmostEqual(p_far, 0.5, places=3, msg="Distinguishable limit must approach 0.5")

    def test_branch_consensus_selection(self):
        sampler = DirectMlHomBosonSampler(num_sites=8, statistics="boson")
        candidates = ["*(p + i)", "*p + i", "p[i]", "arr[i]"]
        logits = [5.0, 4.8, 2.0, 1.0]
        consensus = sampler.sample_branch_consensus(candidates, logits)

        self.assertEqual(consensus["selected_token"], "*(p + i)")
        self.assertLess(consensus["p_coincidence"], 0.1)
        self.assertGreater(consensus["bunching_dip"], 0.8)
        self.assertLess(consensus["latency_ms"], 10.0)


if __name__ == "__main__":
    unittest.main()
