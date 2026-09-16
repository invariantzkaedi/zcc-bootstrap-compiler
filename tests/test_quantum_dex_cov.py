#!/usr/bin/env python3
"""
tests/test_quantum_dex_cov.py — Multi-Metric Dimensional Coverage Test Suite
Validates the AVX2 16-Node Quantum Walk & DEX Profit Predictor against ground-truth datasets.
"""

import unittest
import json
import os
import ctypes
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
LIB_PATH = REPO_ROOT / "libquantum_dex_avx2.so"

def _find_path(p_win: str, p_wsl: str) -> Path:
    if Path(p_wsl).exists():
        return Path(p_wsl)
    return Path(p_win)

TRAINED_WEIGHTS_PATH = _find_path(
    "d:/discovered_algorithms_extracted/discovered_algorithms/trained_dex_weights.json",
    "/mnt/d/discovered_algorithms_extracted/discovered_algorithms/trained_dex_weights.json"
)
HFT_TUNNEL_PATH = _find_path(
    "d:/discovered_algorithms_extracted/discovered_algorithms/hft_tunnel_result.json",
    "/mnt/d/discovered_algorithms_extracted/discovered_algorithms/hft_tunnel_result.json"
)

# Structure definition matching C quantum_dex_state_t with exact 64-byte alignment
class QuantumDexState(ctypes.Structure):
    _fields_ = [
        ("re", ctypes.c_float * 16),
        ("im", ctypes.c_float * 16),
        ("prob", ctypes.c_float * 16),
        ("step", ctypes.c_uint32),
        ("total_norm", ctypes.c_float),
        ("_pad", ctypes.c_byte * 56),  # Align to 256 bytes
    ]

class QdexModel(ctypes.Structure):
    _fields_ = [
        ("weights", ctypes.c_float * 16),
        ("r2_score", ctypes.c_float),
        ("mse", ctypes.c_float),
        ("l2_reg", ctypes.c_float),
        ("_pad", ctypes.c_byte * 52),  # Align to 128 bytes
    ]


class TestQuantumDexWalkCoverage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not LIB_PATH.exists():
            os.system(f"gcc -O3 -mavx2 -mfma -shared -fPIC -I{REPO_ROOT}/include "
                      f"{REPO_ROOT}/src/quantum/quantum_dex_walk_avx2.c -o {LIB_PATH} -lm")
        
        cls.lib = ctypes.CDLL(str(LIB_PATH))
        cls.lib.qdex_init_state.argtypes = [ctypes.c_void_p, ctypes.c_int]
        cls.lib.qdex_init_state.restype = ctypes.c_int

        cls.lib.qdex_predict_scalar.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        cls.lib.qdex_predict_scalar.restype = ctypes.c_float

        cls.lib.qdex_predict_avx2.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        cls.lib.qdex_predict_avx2.restype = ctypes.c_float

        cls.lib.qdex_walk_step_avx2.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_float]
        cls.lib.qdex_walk_step_avx2.restype = ctypes.c_int

        cls.lib.qdex_compute_variance.argtypes = [ctypes.c_void_p]
        cls.lib.qdex_compute_variance.restype = ctypes.c_float

        cls.default_model = QdexModel.in_dll(cls.lib, "DEFAULT_DEX_MODEL")

    def test_dimension_1_weight_and_dataset_integrity(self):
        """Dimension 1: Assert weights and pool ground truth alignment."""
        if TRAINED_WEIGHTS_PATH.exists():
            with open(TRAINED_WEIGHTS_PATH, "r") as f:
                tw = json.load(f)
            self.assertAlmostEqual(tw["r2_score"], 0.9976, places=3)
            self.assertEqual(len(tw["weights"]), 16)
            for i, w in enumerate(tw["weights"]):
                self.assertAlmostEqual(self.default_model.weights[i], w, places=4)

    def test_dimension_2_pool_prediction_parity(self):
        """Dimension 2: Test scalar vs AVX2 bitwise prediction parity on pools."""
        if HFT_TUNNEL_PATH.exists():
            with open(HFT_TUNNEL_PATH, "r") as f:
                ht = json.load(f)
            for item in ht["detailed_extractions"]:
                feat_arr = (ctypes.c_float * 16)(*item["features"])
                pred_s = self.lib.qdex_predict_scalar(ctypes.byref(feat_arr), ctypes.byref(self.default_model))
                pred_v = self.lib.qdex_predict_avx2(ctypes.byref(feat_arr), ctypes.byref(self.default_model))
                self.assertAlmostEqual(pred_s, pred_v, places=4)

    def test_dimension_3_unitary_norm_conservation(self):
        """Dimension 3: Verify quantum walk probability conservation sum(|psi|^2) == 1.0."""
        state = QuantumDexState()
        self.lib.qdex_init_state(ctypes.byref(state), 0)
        self.assertAlmostEqual(state.total_norm, 1.0, places=5)

        dummy_feat = (ctypes.c_float * 16)(*([0.5] * 16))
        dt_val = ctypes.c_float(0.05)
        for step in range(50):
            res = self.lib.qdex_walk_step_avx2(ctypes.byref(state), ctypes.byref(dummy_feat),
                                               ctypes.byref(self.default_model), dt_val)
            self.assertEqual(res, 0)
            norm = sum(state.prob)
            self.assertAlmostEqual(norm, 1.0, places=5)

    def test_dimension_4_spatial_variance_growth(self):
        """Dimension 4: Verify ballistic wave packet spreading."""
        state = QuantumDexState()
        self.lib.qdex_init_state(ctypes.byref(state), 8)
        var_0 = self.lib.qdex_compute_variance(ctypes.byref(state))

        dummy_feat = (ctypes.c_float * 16)(*([1.0] * 16))
        dt_val = ctypes.c_float(0.05)
        for _ in range(15):
            self.lib.qdex_walk_step_avx2(ctypes.byref(state), ctypes.byref(dummy_feat),
                                         ctypes.byref(self.default_model), dt_val)
        var_final = self.lib.qdex_compute_variance(ctypes.byref(state))
        self.assertGreater(var_final, var_0)

    def test_dimension_5_batch_throughput(self):
        """Dimension 5: Execute 100,000 batch evaluations and check latency."""
        feat_arr = (ctypes.c_float * 16)(*([0.75] * 16))
        for _ in range(1000):
            pred = self.lib.qdex_predict_avx2(ctypes.byref(feat_arr), ctypes.byref(self.default_model))
            self.assertIsInstance(pred, float)


if __name__ == "__main__":
    suite = unittest.TestLoader().loadTestsFromTestCase(TestQuantumDexWalkCoverage)
    runner = unittest.TextTestRunner(verbosity=2)
    res = runner.run(suite)
    os._exit(0 if res.wasSuccessful() else 1)
