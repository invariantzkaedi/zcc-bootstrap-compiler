#!/usr/bin/env python3
import os
import json
import shutil
import tempfile
import unittest
import subprocess
from pathlib import Path
from unittest.mock import patch

from zcc_oneirogenesis import FitnessOracle, DreamEngine, DREAM_DIR, REPO_ROOT, PASSES
from zcc_cfg_extract import extract_cfg, cfg_spectral_dim
from zcc_criticality import topology_eta_search, universality_class


class TestOneirogenesisReproducibility(unittest.TestCase):

    def test_A_fitness_deterministic_mode_ignores_benchmark_time(self):
        """A. Proves two fitness measurements with different benchmark times yield same selection_score in deterministic mode."""
        dummy_asm = os.path.join(tempfile.gettempdir(), "dummy_test_repro.s")
        with open(dummy_asm, "w") as f:
            f.write(".text\nmain:\n  movl $1, %eax\n  ret\n")

        with tempfile.TemporaryDirectory() as tmpdir:
            dummy_c = os.path.join(tmpdir, "dummy_workload.c")
            with open(dummy_c, "w") as f:
                f.write("int main() { return 0; }\n")

            # 1ms timing mock generator
            t_val1 = [0]
            def mock_time1():
                t_val1[0] += 1_000_000
                return t_val1[0]

            with patch("time.perf_counter_ns", side_effect=mock_time1):
                fit1_det = FitnessOracle.measure("/bin/true", dummy_c, dummy_asm, tmpdir, deterministic=True)
            with patch("time.perf_counter_ns", side_effect=mock_time1):
                fit1_leg = FitnessOracle.measure("/bin/true", dummy_c, dummy_asm, tmpdir, deterministic=False)

            # 500ms timing mock generator
            t_val2 = [0]
            def mock_time2():
                t_val2[0] += 500_000_000
                return t_val2[0]

            with patch("time.perf_counter_ns", side_effect=mock_time2):
                fit2_det = FitnessOracle.measure("/bin/true", dummy_c, dummy_asm, tmpdir, deterministic=True)
            with patch("time.perf_counter_ns", side_effect=mock_time2):
                fit2_leg = FitnessOracle.measure("/bin/true", dummy_c, dummy_asm, tmpdir, deterministic=False)

            self.assertEqual(fit1_det['structural_score'], fit2_det['structural_score'])
            self.assertEqual(fit1_det['selection_score'], fit2_det['selection_score'])
            self.assertNotEqual(fit1_leg['selection_score'], fit2_leg['selection_score'])
            self.assertEqual(fit1_leg['structural_score'], fit2_leg['structural_score'])

        if os.path.exists(dummy_asm):
            os.remove(dummy_asm)

    def test_B_fitness_legacy_mode_includes_benchmark_time(self):
        """B. Proves legacy mode includes benchmark timing in selection_score."""
        dummy_asm = os.path.join(tempfile.gettempdir(), "dummy_test_legacy.s")
        with open(dummy_asm, "w") as f:
            f.write(".text\nmain:\n  movl $1, %eax\n  ret\n")

        with tempfile.TemporaryDirectory() as tmpdir:
            dummy_c = os.path.join(tmpdir, "dummy_workload.c")
            with open(dummy_c, "w") as f:
                f.write("int main() { return 0; }\n")

            fit1 = FitnessOracle.measure("/bin/true", dummy_c, dummy_asm, tmpdir, deterministic=False)
            struct = fit1['structural_score']
            sel = fit1['selection_score']
            bench_ms = fit1['benchmark_time_ns'] / 1e6
            self.assertAlmostEqual(sel, struct + bench_ms, places=5)

        if os.path.exists(dummy_asm):
            os.remove(dummy_asm)

    def test_C_graph_sampling_stable_ordered_node_sequence(self):
        """C. Proves graph sampling receives a stable ordered node sequence."""
        asm_lines = [
            "__entry__:",
            "  jmp .L2",
            ".L1:",
            "  movl $1, %eax",
            "  ret",
            ".L2:",
            "  jmp .L1",
            ".L10:",
            "  jmp .L1",
        ]
        cfg = extract_cfg(asm_lines)
        nodes = list(cfg.keys())
        self.assertEqual(nodes, sorted(nodes))
        for k in cfg:
            self.assertEqual(cfg[k], sorted(cfg[k]))

        eta1 = topology_eta_search(cfg, seed=42)
        eta2 = topology_eta_search(cfg, seed=42)
        self.assertEqual(eta1, eta2)

        spec1 = cfg_spectral_dim(cfg, seed=42)
        spec2 = cfg_spectral_dim(cfg, seed=42)
        self.assertEqual(spec1, spec2)

    def test_D_replay_dry_run(self):
        """D. Dry-run replay test running two isolated 10-cycle sessions with seed 42 and asserting equality of stable outputs."""
        tmp_dream1 = tempfile.mkdtemp(prefix="dream_run1_")
        tmp_dream2 = tempfile.mkdtemp(prefix="dream_run2_")

        try:
            d1 = Path(tmp_dream1) / "dreams"
            d1.mkdir(parents=True, exist_ok=True)
            with patch("zcc_oneirogenesis.DREAM_DIR", d1):
                eng1 = DreamEngine(seed=42, deterministic=True, dry_run=True)
                eng1.run(num_cycles=10)
                st1 = eng1.state
                eta1 = eng1.eta_c

            d2 = Path(tmp_dream2) / "dreams"
            d2.mkdir(parents=True, exist_ok=True)
            with patch("zcc_oneirogenesis.DREAM_DIR", d2):
                eng2 = DreamEngine(seed=42, deterministic=True, dry_run=True)
                eng2.run(num_cycles=10)
                st2 = eng2.state
                eta2 = eng2.eta_c

            self.assertEqual(eta1, eta2)
            self.assertEqual(st1.generation, st2.generation)
            self.assertEqual(st1.parent_hash, st2.parent_hash)
            self.assertEqual(len(st1.lineage), len(st2.lineage))

        finally:
            shutil.rmtree(tmp_dream1, ignore_errors=True)
            shutil.rmtree(tmp_dream2, ignore_errors=True)

    def test_E_live_replay_integration(self):
        """E. Live bounded integration replay executing real build + self-host gate + scoring + lineage persistence."""
        tmp_dream1 = tempfile.mkdtemp(prefix="dream_live_run1_")
        tmp_dream2 = tempfile.mkdtemp(prefix="dream_live_run2_")

        try:
            d1 = Path(tmp_dream1) / "dreams"
            d1.mkdir(parents=True, exist_ok=True)
            with patch("zcc_oneirogenesis.DREAM_DIR", d1):
                eng1 = DreamEngine(seed=42, force_sweep=True, deterministic=True, dry_run=False)
                eng1.run(num_cycles=2)
                st1 = eng1.state
                eta1 = eng1.eta_c

            d2 = Path(tmp_dream2) / "dreams"
            d2.mkdir(parents=True, exist_ok=True)
            with patch("zcc_oneirogenesis.DREAM_DIR", d2):
                eng2 = DreamEngine(seed=42, force_sweep=True, deterministic=True, dry_run=False)
                eng2.run(num_cycles=2)
                st2 = eng2.state
                eta2 = eng2.eta_c

            self.assertEqual(eta1, eta2)
            self.assertGreater(st1.generation, 0, "Live run must evolve non-trivial candidate")
            self.assertEqual(st1.generation, st2.generation)
            self.assertEqual(st1.parent_hash, st2.parent_hash)
            self.assertEqual(len(st1.lineage), len(st2.lineage))

            for l1, l2 in zip(st1.lineage, st2.lineage):
                self.assertEqual(l1.get('generation'), l2.get('generation'))
                self.assertEqual(l1.get('hash'), l2.get('hash'))
                self.assertEqual(l1.get('mutations'), l2.get('mutations'))
                self.assertEqual(l1.get('structural_score'), l2.get('structural_score'))
                self.assertEqual(l1.get('selection_score'), l2.get('selection_score'))

        finally:
            shutil.rmtree(tmp_dream1, ignore_errors=True)
            shutil.rmtree(tmp_dream2, ignore_errors=True)

    def test_F_compute_structural_score_arithmetic(self):
        """F. Pure scoring helper exact arithmetic test asserting exact expected structural score."""
        metrics = {
            'inst_count': 256692,
            'asm_size': 878495,
            'branch_count': 28141,
            'stack_depth_sum': 472848,
        }
        res = FitnessOracle.compute_structural_score(metrics)

        # Expected component contributions for baseline zcc2.s:
        # inst_count:      256692 * 4.00  = 1026768.0
        # asm_size:        878495 * 0.30  = 263548.5
        # branch_count:    28141 * 4.00   = 112564.0
        # stack_depth_sum: 472848 * 0.05  = 23642.4
        # Total Sum: 1026768.0 + 263548.5 + 112564.0 + 23642.4 = 1426522.9

        self.assertEqual(res['inst_count']['contribution'], 1026768.0)
        self.assertEqual(res['asm_size']['contribution'], 263548.5)
        self.assertEqual(res['branch_count']['contribution'], 112564.0)
        self.assertEqual(res['stack_depth_sum']['contribution'], 23642.4)

        total_contrib = sum(v['contribution'] for k, v in res.items() if k != 'structural_score')
        self.assertAlmostEqual(total_contrib, 1426522.9, places=4)
        self.assertAlmostEqual(res['structural_score'], 1426522.9, places=4)

    def test_G_report_json_canonical_metrics_alignment(self):
        """G. Asserts that reports/oneirogenesis_reproducibility.json matches live measurement of canonical zcc2.s."""
        zcc2_asm = os.path.join(REPO_ROOT, "zcc2.s")
        if not os.path.exists(zcc2_asm):
            self.skipTest("zcc2.s not found")

        report_json_path = os.path.join(REPO_ROOT, "reports", "oneirogenesis_reproducibility.json")
        with open(report_json_path) as f:
            report_data = json.load(f)

        with tempfile.TemporaryDirectory() as tmpdir:
            zcc_binary = os.path.join(tmpdir, "canonical_bin")
            p_args = [str(REPO_ROOT / p) for p in PASSES]
            subprocess.run(
                ['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                 '-Wa,--noexecstack', '-fno-unwind-tables',
                 '-Iinclude', '-I.', '-o', zcc_binary, zcc2_asm] + p_args + ['-lm'],
                capture_output=True, check=True
            )
            measured = FitnessOracle.measure(zcc_binary, "benchmark_workload.c", zcc2_asm, tmpdir, deterministic=True)

        reported_breakdown = report_data["canonical_parent_score_breakdown"]
        measured_breakdown = measured["score_breakdown"]

        # Strict object equality across value, weight, contribution, and total score
        self.assertEqual(reported_breakdown, measured_breakdown)



if __name__ == "__main__":
    unittest.main()
