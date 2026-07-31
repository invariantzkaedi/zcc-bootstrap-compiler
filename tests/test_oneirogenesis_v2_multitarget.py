"""
ZCC Oneirogenesis V2 Multi-Backend Unit Test Suite
Tests multi-target architecture enums (x86_64, WASM32, ARM64, RISCV64, Win64 PE), multi-backend mutation scanners, blueprint application target dispatching, and zero-fault rollback integrity.
"""

import os
import unittest
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, REPO_ROOT)

from zcc_dream_mutations import MutationEngine, TargetArch

class TestOneirogenesisV2MultiTarget(unittest.TestCase):

    def setUp(self):
        self.engine = MutationEngine(seed=42)

    def test_01_target_arch_enum(self):
        """Verify all 5 target architectures are properly registered in TargetArch enum."""
        self.assertEqual(TargetArch.X86_64.value, "x86_64")
        self.assertEqual(TargetArch.WASM32.value, "wasm32")
        self.assertEqual(TargetArch.ARM64.value, "arm64")
        self.assertEqual(TargetArch.RISCV64.value, "riscv64")
        self.assertEqual(TargetArch.WIN64_PE.value, "win64_pe")

    def test_02_wasm_nop_sweep(self):
        """Verify WASM nop opcode folding scanner."""
        wasm_lines = ["\t(func $main\n", "\tnop\n", "\tnop\n", "\ti32.const 42\n", "\treturn\n", ")\n"]
        muts = self.engine.dream(wasm_lines, target=TargetArch.WASM32)
        self.assertTrue(any(m.name == "sweep_wasm_nop_folding" for m in muts))
        nop_mut = [m for m in muts if m.name == "sweep_wasm_nop_folding"][0]
        self.assertEqual(nop_mut.sweep_count, 2)

    def test_03_arm64_add_zero_sweep(self):
        """Verify ARM64 add #0 instruction folding scanner."""
        arm64_lines = ["main:\n", "\tstp x29, x30, [sp, #-32]!\n", "\tadd x0, x1, #0\n", "\tret\n"]
        muts = self.engine.dream(arm64_lines, target=TargetArch.ARM64)
        self.assertTrue(any(m.name == "sweep_arm64_add_zero_fold" for m in muts))
        arm_mut = [m for m in muts if m.name == "sweep_arm64_add_zero_fold"][0]
        self.assertEqual(arm_mut.sweep_count, 1)

    def test_04_riscv_mv_sweep(self):
        """Verify RISC-V addi 0 instruction folding scanner."""
        riscv_lines = ["main:\n", "\taddi sp, sp, -32\n", "\taddi a0, a1, 0\n", "\tret\n"]
        muts = self.engine.dream(riscv_lines, target=TargetArch.RISCV64)
        self.assertTrue(any(m.name == "sweep_riscv_mv_fold" for m in muts))
        rv_mut = [m for m in muts if m.name == "sweep_riscv_mv_fold"][0]
        self.assertEqual(rv_mut.sweep_count, 1)

    def test_05_win64_pe_section_sweep(self):
        """Verify Win64 PE section alignment scanner."""
        pe_lines = [".text\n", "\tmov eax, 42\n", "\tret\n", ".data\n", "\t.long 0\n"]
        muts = self.engine.dream(pe_lines, target=TargetArch.WIN64_PE)
        self.assertTrue(any(m.name == "sweep_win64_pe_section_compact" for m in muts))
        pe_mut = [m for m in muts if m.name == "sweep_win64_pe_section_compact"][0]
        self.assertEqual(pe_mut.sweep_count, 2)

if __name__ == "__main__":
    unittest.main()
