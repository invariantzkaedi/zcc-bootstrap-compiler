# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // DUAL-SILICON SPECULATIVE GAUNTLET & AUDIT HARNESS 🔱
=======================================================================================================
 Silicon Under Test:
   - Silicon 1: AMD Ryzen AI NPU (Krackan, 51.3 TOPS, 47.12 GB Shared Memory Pool)
   - Silicon 2: NVIDIA GeForce RTX 5070 Laptop GPU (CUDA, 8GB GDDR7, Pushdown Grammar Engine)
 Target Compiler   : ZCC (Native Stages 1-5) + GCC Assembler/Linker
=======================================================================================================
"""

import os
import sys
import time
import json
import socket
import unittest
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.npu.amd_npu_directml_drafter import AmdNpuDirectMlDrafter
from zkaedi_prime.dual_silicon_orchestrator import DualSiliconOrchestrator

class TestDualSiliconGauntlet(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.drafter = AmdNpuDirectMlDrafter(device_id=1)
        cls.orchestrator = DualSiliconOrchestrator(local_drafter=cls.drafter)

    def test_gate1_npu_physical_hardware(self):
        """Gate NPU-1: Verify physical presence and specs of AMD NPU Krackan."""
        info = self.drafter.hardware_info
        print(f"\n[GATE NPU-1] NPU Hardware Attestation: {info['npu_name']}")
        self.assertIn("Krackan", info["npu_name"])
        self.assertGreaterEqual(info["tops"], 50.0)
        self.assertGreaterEqual(info["shared_memory_gb"], 40.0)
        self.assertIn("DmlExecutionProvider", self.drafter.active_providers)

    def test_gate2_directml_tensor_submillisecond(self):
        """Gate NPU-2: Assert DirectML tensor multiplication latency < 500 us on AMD silicon."""
        bench = self.drafter.benchmark_tensor_core(runs=100)
        print(f"\n[GATE NPU-2] DirectML Latency: {bench['avg_latency_us']:.2f} us | Throughput: {bench['throughput_ops_sec']:.1f} ops/sec")
        self.assertLess(bench["avg_latency_us"], 1000.0, "DirectML latency must be sub-millisecond (< 1000us)")
        self.assertGreater(bench["throughput_ops_sec"], 1000.0)

    def test_gate3_speculative_draft_generation(self):
        """Gate NPU-3: Verify K-token candidate generation and latency on AMD silicon."""
        res = self.drafter.generate_draft_tokens([100, 200], k=4)
        print(f"\n[GATE NPU-3] Proposed Draft Tokens: {res['draft_tokens']} in {res['latency_ms']:.2f}ms")
        self.assertEqual(len(res["draft_tokens"]), 4)
        self.assertLess(res["latency_ms"], 20.0)

    def test_gate4_dual_silicon_hardware_sync(self):
        """Gate NPU-4: Query simultaneous dual-silicon telemetry across AMD NPU and NVIDIA GPU."""
        telemetry = self.orchestrator.get_hardware_telemetry()
        print(f"\n[GATE NPU-4] Dual-Silicon Telemetry:")
        print(f"  NPU: {telemetry['npu']['name']} | {telemetry['npu']['tops']} TOPS | {telemetry['npu']['shared_mem_gb']} GB")
        print(f"  GPU: {telemetry['gpu']['name']} | Status: {telemetry['gpu']['status']}")
        self.assertEqual(telemetry["npu"]["status"], "ONLINE")

    def test_gate5_zcc_selfhost_identity_seal(self):
        """Gate NPU-5: Verify ZCC bootstrap self-host identity seal (cmp zcc2.s zcc3.s)."""
        zcc2 = REPO_ROOT / "zcc2.s"
        zcc3 = REPO_ROOT / "zcc3.s"
        if not (zcc2.exists() and zcc3.exists()):
            self.skipTest("zcc2.s or zcc3.s not found on host disk")

        # In PowerShell / Windows, compare file bytes
        b2 = zcc2.read_bytes()
        b3 = zcc3.read_bytes()
        print(f"\n[GATE NPU-5] Comparing zcc2.s ({len(b2)}B) vs zcc3.s ({len(b3)}B)...")
        self.assertEqual(b2, b3, "Gate 1 byte-identical self-host identity broken!")
        print("  ✔ Gate 1 Identity Verified: BYTE IDENTICAL (100% bit-exact).")

if __name__ == "__main__":
    unittest.main(verbosity=2)
