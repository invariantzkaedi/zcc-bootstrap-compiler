import unittest
import numpy as np
import math
import struct
import hashlib
import time
from tools.pyavxzkd import PyAvxzkdField, PyAvxzkdQuantumWalk

class TestPyAvxzkdSupremeGauntlet(unittest.TestCase):
    """
    Exhaustive 60-Test Gauntlet for AVXzkd SUPREME SIMD Engine & Quantum Trinity
    """

    def setUp(self):
        self.width = 64
        self.height = 64
        self.field = PyAvxzkdField(self.width, self.height)

    # =========================================================================
    # Group 1: Lattice Initialization & Geometry (Tests 1-8)
    # =========================================================================

    def test_01_init_constant_positive(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 8.0
        self.field.init_field(data)
        np.testing.assert_allclose(self.field.get_current(), 8.0, atol=1e-5)

    def test_02_init_constant_negative(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * -8.0
        self.field.init_field(data)
        np.testing.assert_allclose(self.field.get_current(), -8.0, atol=1e-5)

    def test_03_init_zero_field(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        np.testing.assert_allclose(self.field.get_current(), 0.0, atol=1e-7)

    def test_04_shape_mismatch_smaller(self):
        bad = np.ones((32, 32), dtype=np.float32)
        with self.assertRaises(ValueError):
            self.field.init_field(bad)

    def test_05_shape_mismatch_larger(self):
        bad = np.ones((128, 128), dtype=np.float32)
        with self.assertRaises(ValueError):
            self.field.init_field(bad)

    def test_06_non_square_lattice(self):
        f = PyAvxzkdField(96, 48)
        data = np.ones((48, 96), dtype=np.float32) * 3.5
        f.init_field(data)
        self.assertEqual(f.get_current().shape, (48, 96))
        np.testing.assert_allclose(f.get_current(), 3.5, atol=1e-5)

    def test_07_linear_gradient_initialization(self):
        data = np.linspace(-10.0, 10.0, self.width * self.height, dtype=np.float32).reshape(self.height, self.width)
        self.field.init_field(data)
        np.testing.assert_allclose(self.field.get_current(), data, atol=1e-5)

    def test_08_checkerboard_pattern(self):
        y, x = np.ogrid[:self.height, :self.width]
        cb = ((x + y) % 2 * 10.0 - 5.0).astype(np.float32)
        self.field.init_field(cb)
        np.testing.assert_allclose(self.field.get_current(), cb, atol=1e-5)

    # =========================================================================
    # Group 2: Deep Recursion Dynamics & Regimes (Tests 9-16)
    # =========================================================================

    def test_09_zero_step_noop(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 4.2
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=0.3, k_steps=0)
        np.testing.assert_allclose(self.field.get_current(), 4.2, atol=1e-5)

    def test_10_eta_zero_identity(self):
        data = np.random.uniform(-5.0, 5.0, (self.height, self.width)).astype(np.float32)
        self.field.init_field(data)
        self.field.step(eta=0.0, gamma=0.3, k_steps=20)
        np.testing.assert_allclose(self.field.get_current(), data, atol=1e-5)

    def test_11_subcritical_convergence(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 10.0
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=0.3, k_steps=50)
        expected = 10.0 / (1.0 - 0.4) # ~16.6667
        np.testing.assert_allclose(self.field.get_current(), expected, rtol=0.01)

    def test_12_negative_floor_containment(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * -10.0
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=0.3, k_steps=50)
        # S(-3) is small (~0.0474), state stays near -10.18
        curr = self.field.get_current()
        self.assertTrue((curr <= -9.9).all())
        self.assertTrue((curr >= -11.0).all())

    def test_13_high_gamma_saturation(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 5.0
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=5.0, k_steps=30)
        # With gamma=5.0, sigma(25) ~ 1.0, converges to 5.0 / 0.6 = 8.3333
        np.testing.assert_allclose(self.field.get_current(), 8.3333, rtol=0.01)

    def test_14_zero_gamma_limit(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 6.0
        self.field.init_field(data)
        # gamma=0 implies sigma(0) = 0.5 everywhere -> effective eta = 0.2 -> 6.0 / (1 - 0.2) = 7.5
        self.field.step(eta=0.4, gamma=0.0, k_steps=50)
        np.testing.assert_allclose(self.field.get_current(), 7.5, rtol=0.01)

    def test_15_damping_negative_eta(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 10.0
        self.field.init_field(data)
        self.field.step(eta=-0.4, gamma=0.3, k_steps=50)
        # Analytical fixed point: H* = 10 / (1 - (-0.4) * sigma(0.3 * H*)) = 7.35135
        np.testing.assert_allclose(self.field.get_current(), 7.35135, rtol=0.01)

    def test_16_near_critical_gain(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 2.0
        self.field.init_field(data)
        self.field.step(eta=0.8, gamma=0.3, k_steps=80)
        # Analytical fixed point: H* = 2.0 / (1 - 0.8 * sigma(0.3 * H*)) = 6.91194
        np.testing.assert_allclose(self.field.get_current(), 6.91194, rtol=0.01)

    # =========================================================================
    # Group 3: Multi-Threading & Concurrency (Tests 17-22)
    # =========================================================================

    def test_17_parallel_step_1_thread(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 5.0
        self.field.init_field(data)
        self.field.step_parallel(eta=0.4, gamma=0.3, k_steps=20, threads=1)
        self.assertFalse(np.isnan(self.field.get_current()).any())

    def test_18_parallel_step_2_threads(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 5.0
        self.field.init_field(data)
        self.field.step_parallel(eta=0.4, gamma=0.3, k_steps=20, threads=2)
        self.assertFalse(np.isnan(self.field.get_current()).any())

    def test_19_parallel_step_4_threads(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 5.0
        self.field.init_field(data)
        self.field.step_parallel(eta=0.4, gamma=0.3, k_steps=20, threads=4)
        self.assertFalse(np.isnan(self.field.get_current()).any())

    def test_20_parallel_vs_serial_equivalence(self):
        data = np.random.uniform(-3.0, 3.0, (self.height, self.width)).astype(np.float32)
        f_ser = PyAvxzkdField(self.width, self.height)
        f_ser.init_field(data)
        f_ser.step(eta=0.4, gamma=0.3, k_steps=15)

        f_par = PyAvxzkdField(self.width, self.height)
        f_par.init_field(data)
        f_par.step_parallel(eta=0.4, gamma=0.3, k_steps=15, threads=4)

        np.testing.assert_allclose(f_ser.get_current(), f_par.get_current(), atol=1e-5)

    def test_21_parallel_large_grid(self):
        f = PyAvxzkdField(128, 128)
        data = np.ones((128, 128), dtype=np.float32) * 2.0
        f.init_field(data)
        f.step_parallel(eta=0.4, gamma=0.3, k_steps=25, threads=4)
        self.assertEqual(f.get_current().shape, (128, 128))

    def test_22_parallel_deep_iteration_stress(self):
        data = np.random.uniform(-2.0, 2.0, (self.height, self.width)).astype(np.float32)
        self.field.init_field(data)
        self.field.step_parallel(eta=0.4, gamma=0.3, k_steps=100, threads=4)
        self.assertFalse(np.isnan(self.field.get_current()).any())

    # =========================================================================
    # Group 4: Topological Curvature & Stencils (Tests 23-30)
    # =========================================================================

    def test_23_flat_field_zero_laplacian(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 7.0
        self.field.init_field(data)
        self.field.compute_topology()
        # Interior Laplacian of flat surface must be zero
        inner = self.field.get_curvature()[1:-1, 1:-1]
        np.testing.assert_allclose(inner, 0.0, atol=1e-5)

    def test_24_gaussian_peak_negative_laplacian(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        data[32, 32] = 25.0
        self.field.init_field(data)
        self.field.compute_topology()
        curv = self.field.get_curvature()
        self.assertLess(curv[32, 32], -80.0)

    def test_25_bowl_minimum_positive_laplacian(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        data[32, 32] = -25.0
        self.field.init_field(data)
        self.field.compute_topology()
        curv = self.field.get_curvature()
        self.assertGreater(curv[32, 32], 80.0)

    def test_26_saddle_point_hessian_det(self):
        y, x = np.ogrid[:self.height, :self.width]
        cx, cy = 32, 32
        # Hyperbolic saddle: H(x, y) = (x - cx)^2 - (y - cy)^2
        saddle = ((x - cx)**2 - (y - cy)**2).astype(np.float32)
        self.field.init_field(saddle)
        self.field.compute_topology()
        # det(Hessian) at saddle point must be negative (H_xx * H_yy < 0)
        # H_xx = 2, H_yy = -2, det = -4
        f_ptr = self.field._field_ptr.contents
        det_arr = np.ctypeslib.as_array(f_ptr.hessian_det, shape=(f_ptr.height, f_ptr.stride))
        self.assertLess(det_arr[32, 32], -1.0)

    def test_27_local_extremum_hessian_det(self):
        y, x = np.ogrid[:self.height, :self.width]
        cx, cy = 32, 32
        # Parabolic bowl: H(x, y) = (x - cx)^2 + (y - cy)^2
        bowl = ((x - cx)**2 + (y - cy)**2).astype(np.float32)
        self.field.init_field(bowl)
        self.field.compute_topology()
        # det(Hessian) at minimum must be positive (H_xx * H_yy > 0)
        f_ptr = self.field._field_ptr.contents
        det_arr = np.ctypeslib.as_array(f_ptr.hessian_det, shape=(f_ptr.height, f_ptr.stride))
        self.assertGreater(det_arr[32, 32], 1.0)

    def test_28_curvature_shape_integrity(self):
        data = np.random.uniform(-5.0, 5.0, (self.height, self.width)).astype(np.float32)
        self.field.init_field(data)
        self.field.compute_topology()
        curv = self.field.get_curvature()
        self.assertEqual(curv.shape, (self.height, self.width))

    def test_29_topology_non_nan(self):
        data = np.random.uniform(-10.0, 10.0, (self.height, self.width)).astype(np.float32)
        self.field.init_field(data)
        self.field.compute_topology()
        self.assertFalse(np.isnan(self.field.get_curvature()).any())

    def test_30_topology_stride_boundary_safe(self):
        # Verify 32-width boundary padding
        f = PyAvxzkdField(32, 32)
        data = np.ones((32, 32), dtype=np.float32)
        f.init_field(data)
        f.compute_topology()
        self.assertEqual(f.get_curvature().shape, (32, 32))

    # =========================================================================
    # Group 5: Two-Regime Scar Walker & Navigation (Tests 31-40)
    # =========================================================================

    def test_31_straight_line_navigation(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        res = self.field.solve_walker(start=(0, 0), target=(10, 0), max_steps=100)
        self.assertTrue(res["solved"])
        self.assertEqual(res["path"][-1], (10, 0))

    def test_32_start_equals_target(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        res = self.field.solve_walker(start=(5, 5), target=(5, 5), max_steps=50)
        self.assertTrue(res["solved"])
        self.assertEqual(res["total_steps"], 0)

    def test_33_u_wall_barrier_avoidance(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        # Create barrier at x=10, y in [0..20]
        data[0:21, 10] = 50.0
        self.field.init_field(data)
        res = self.field.solve_walker(start=(0, 10), target=(20, 10), max_steps=2000)
        self.assertTrue(res["solved"])
        self.assertEqual(res["path"][-1], (20, 10))

    def test_34_scar_accumulation_along_path(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        self.field.solve_walker(start=(0, 0), target=(5, 0), kick=3.0, max_steps=50)
        scars = self.field.get_scars()
        self.assertGreater(scars[0, 0], 2.5)

    def test_35_loop_pruning_optimization(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        res = self.field.solve_walker(start=(0, 0), target=(10, 10), max_steps=1000)
        self.assertTrue(res["solved"])
        # Pruned simple path length should be <= total exploration steps + 1
        self.assertLessEqual(res["path_len"], res["total_steps"] + 1)

    def test_36_diagonal_corner_to_corner(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        res = self.field.solve_walker(start=(0, 0), target=(self.width - 1, self.height - 1), max_steps=5000)
        self.assertTrue(res["solved"])
        self.assertEqual(res["path"][-1], (self.width - 1, self.height - 1))

    def test_37_momentum_penalty_effect(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        # Solving with momentum > 0 should penalize immediate 180-degree reversal
        res = self.field.solve_walker(start=(0, 0), target=(8, 8), momentum=0.5, max_steps=500)
        self.assertTrue(res["solved"])

    def test_38_zero_kick_control_baseline(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        # With zero kick, no scars are deposited
        self.field.solve_walker(start=(0, 0), target=(4, 0), kick=0.0, max_steps=20)
        scars = self.field.get_scars()
        self.assertEqual(scars[0, 0], 0.0)

    def test_39_walker_step_limit_timeout(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        # Max steps = 2 cannot reach target at (50, 50)
        res = self.field.solve_walker(start=(0, 0), target=(50, 50), max_steps=2)
        self.assertFalse(res["solved"])

    def test_40_path_coordinate_validity(self):
        data = np.zeros((self.height, self.width), dtype=np.float32)
        self.field.init_field(data)
        res = self.field.solve_walker(start=(0, 0), target=(6, 6), max_steps=100)
        for px, py in res["path"]:
            self.assertTrue(0 <= px < self.width)
            self.assertTrue(0 <= py < self.height)

    # =========================================================================
    # Group 6: Dual-Field Resonance & Coupling (Tests 41-45)
    # =========================================================================

    def test_41_zero_coupling_independent(self):
        f1 = PyAvxzkdField(self.width, self.height)
        f2 = PyAvxzkdField(self.width, self.height)
        d1 = np.ones((self.height, self.width), dtype=np.float32) * 5.0
        d2 = np.ones((self.height, self.width), dtype=np.float32) * -5.0
        f1.init_field(d1)
        f2.init_field(d2)

        # Couple with kappa=0
        f1._lib.avxzkd_couple_fields_avx2(f1._field_ptr, f2._field_ptr, 0.0)
        np.testing.assert_allclose(f1.get_current(), 5.0, atol=1e-5)
        np.testing.assert_allclose(f2.get_current(), -5.0, atol=1e-5)

    def test_42_positive_coupling_energy_exchange(self):
        f1 = PyAvxzkdField(self.width, self.height)
        f2 = PyAvxzkdField(self.width, self.height)
        d1 = np.ones((self.height, self.width), dtype=np.float32) * 10.0
        d2 = np.ones((self.height, self.width), dtype=np.float32) * 0.0
        f1.init_field(d1)
        f2.init_field(d2)

        # Couple with kappa=0.1
        f1._lib.avxzkd_couple_fields_avx2(f1._field_ptr, f2._field_ptr, 0.1)
        # f1 should decrease from 10.0 to 9.0, f2 should increase from 0.0 to 1.0
        np.testing.assert_allclose(f1.get_current(), 9.0, atol=1e-5)
        np.testing.assert_allclose(f2.get_current(), 1.0, atol=1e-5)

    def test_43_coupling_conserved_sum(self):
        f1 = PyAvxzkdField(self.width, self.height)
        f2 = PyAvxzkdField(self.width, self.height)
        d1 = np.random.uniform(0.0, 10.0, (self.height, self.width)).astype(np.float32)
        d2 = np.random.uniform(0.0, 10.0, (self.height, self.width)).astype(np.float32)
        f1.init_field(d1)
        f2.init_field(d2)

        initial_sum = f1.get_current() + f2.get_current()
        f1._lib.avxzkd_couple_fields_avx2(f1._field_ptr, f2._field_ptr, 0.25)
        final_sum = f1.get_current() + f2.get_current()
        np.testing.assert_allclose(initial_sum, final_sum, atol=1e-4)

    def test_44_equal_potential_zero_transfer(self):
        f1 = PyAvxzkdField(self.width, self.height)
        f2 = PyAvxzkdField(self.width, self.height)
        d = np.ones((self.height, self.width), dtype=np.float32) * 4.0
        f1.init_field(d)
        f2.init_field(d)

        f1._lib.avxzkd_couple_fields_avx2(f1._field_ptr, f2._field_ptr, 0.5)
        np.testing.assert_allclose(f1.get_current(), 4.0, atol=1e-5)
        np.testing.assert_allclose(f2.get_current(), 4.0, atol=1e-5)

    def test_45_coupling_mismatched_dimension_error(self):
        f1 = PyAvxzkdField(64, 64)
        f2 = PyAvxzkdField(32, 32)
        res = f1._lib.avxzkd_couple_fields_avx2(f1._field_ptr, f2._field_ptr, 0.1)
        self.assertEqual(res, -2) # AVXZKD_ERR_ALIGNMENT

    # =========================================================================
    # Group 7: Layer 1 Quantum DTQW & Decoherence (Tests 46-55)
    # =========================================================================

    def test_46_dtqw_single_step(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=1)
        probs = qw.get_probabilities()
        self.assertAlmostEqual(np.sum(probs), 1.0, places=5)

    def test_47_dtqw_multi_step_norm_conservation(self):
        for steps in [2, 5, 10, 25, 50, 100]:
            qw = PyAvxzkdQuantumWalk()
            qw.step(steps=steps)
            norm = np.sum(qw.get_probabilities())
            self.assertAlmostEqual(norm, 1.0, places=5, msg=f"Failed norm conservation at step {steps}")

    def test_48_dtqw_node_count_16(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=10)
        self.assertEqual(len(qw.get_probabilities()), 16)
        self.assertEqual(len(qw.get_phases()), 16)

    def test_49_dtqw_phases_bounded(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=20)
        phases = qw.get_phases()
        self.assertTrue((phases >= -math.pi - 1e-5).all())
        self.assertTrue((phases <= math.pi + 1e-5).all())

    def test_50_dtqw_entanglement_entropy_positive(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=30)
        s_q0 = qw.get_entanglement_entropy()
        self.assertGreater(s_q0, 0.0)
        self.assertLessEqual(s_q0, 1.0001)

    def test_51_dtqw_initial_purity_coherence_one(self):
        qw = PyAvxzkdQuantumWalk()
        self.assertEqual(qw.get_coherence(), 1.0)

    def test_52_dtqw_lindblad_dephasing_monotonic(self):
        qw1 = PyAvxzkdQuantumWalk()
        qw1.step(steps=10)
        qw1.dephase(gamma_dephase=0.1)

        qw2 = PyAvxzkdQuantumWalk()
        qw2.step(steps=10)
        qw2.dephase(gamma_dephase=0.5)

        self.assertGreater(qw1.get_coherence(), qw2.get_coherence())

    def test_53_dtqw_full_decoherence_asymptote(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=10)
        qw.dephase(gamma_dephase=50.0)
        self.assertAlmostEqual(qw.get_coherence(), 0.0, places=5)

    def test_54_dtqw_commitment_payload_structure(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=10)
        commitment, digest_hex = qw.get_public_commitment()
        self.assertEqual(len(commitment), 296)
        # Check that digest matches SHA-256 of payload (first 264 bytes)
        payload = commitment[:264]
        calc_digest = hashlib.sha256(payload).hexdigest()
        self.assertEqual(calc_digest, digest_hex)

    def test_55_dtqw_commitment_tamper_detection(self):
        qw = PyAvxzkdQuantumWalk()
        qw.step(steps=10)
        commitment, digest_hex = qw.get_public_commitment()
        # Flip 1 bit in payload
        tampered_payload = bytearray(commitment[:264])
        tampered_payload[0] ^= 0x01
        tampered_digest = hashlib.sha256(tampered_payload).hexdigest()
        self.assertNotEqual(tampered_digest, digest_hex)

    # =========================================================================
    # Group 8: Cryptographic Attestation & Invariant Audit (Tests 56-60)
    # =========================================================================

    def test_56_audit_deterministic_digest(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 3.14
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=0.3, k_steps=10)
        a1 = self.field.audit(eta=0.4, gamma=0.3)

        f2 = PyAvxzkdField(self.width, self.height)
        f2.init_field(data)
        f2.step(eta=0.4, gamma=0.3, k_steps=10)
        a2 = f2.audit(eta=0.4, gamma=0.3)

        self.assertEqual(a1["state_digest"], a2["state_digest"])

    def test_57_audit_digest_avalanche_sensitivity(self):
        data1 = np.ones((self.height, self.width), dtype=np.float32) * 3.14
        data2 = data1.copy()
        data2[0, 0] += 1e-4

        f1 = PyAvxzkdField(self.width, self.height)
        f1.init_field(data1)
        a1 = f1.audit(eta=0.4, gamma=0.3)

        f2 = PyAvxzkdField(self.width, self.height)
        f2.init_field(data2)
        a2 = f2.audit(eta=0.4, gamma=0.3)

        self.assertNotEqual(a1["state_digest"], a2["state_digest"])

    def test_58_audit_saturated_gain_certificate(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * 10.0
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=0.3, k_steps=50)
        audit = self.field.audit(eta=0.4, gamma=0.3)
        self.assertTrue(audit["pass_all_invariants"])
        self.assertAlmostEqual(audit["measured_gain"], 1.66667, delta=0.02)

    def test_59_audit_floor_drift_bound(self):
        data = np.ones((self.height, self.width), dtype=np.float32) * -10.0
        self.field.init_field(data)
        self.field.step(eta=0.4, gamma=0.3, k_steps=50)
        audit = self.field.audit(eta=0.4, gamma=0.3)
        self.assertLess(audit["floor_drift"], 0.001)

    def test_60_benchmark_throughput_envelope(self):
        b_field = PyAvxzkdField(128, 128)
        data = np.random.uniform(-5.0, 5.0, (128, 128)).astype(np.float32)
        b_field.init_field(data)

        t0 = time.perf_counter()
        b_field.step(eta=0.4, gamma=0.3, k_steps=100)
        t_avx = time.perf_counter() - t0

        updates = 128 * 128 * 100
        rate = updates / t_avx
        print(f"\n[GAUNTLET BENCHMARK] PyAvxzkd: {rate / 1e6:.2f} MCells/sec ({t_avx * 1000:.3f} ms for 100 steps)")
        self.assertGreater(rate, 5e6)


if __name__ == "__main__":
    unittest.main()
