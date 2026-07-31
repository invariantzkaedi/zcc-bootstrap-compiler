#!/usr/bin/env python3
"""
Unit Test Suite for Next-Gen X86-64 Peephole Scanners & C-AST Rule Synthesizer
-----------------------------------------------------------------------------
Verifies:
  1. Detection and replacement of bit-test patterns (andq $1 + testq -> testb $1).
  2. Blueprint application logic in tools/apply_oneirogenesis_blueprint.py.
  3. Patch synthesis, dry-run previews, backup creation, and rollback integrity
     in tools/blueprint-source-synthesizer.py.
"""

import unittest
import os
import sys
import json
import tempfile
import shutil
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from zcc_dream_mutations import MutationEngine
from tools.apply_oneirogenesis_blueprint import apply_sweep_testb_bit_test, apply_blueprint

import importlib.util
synth_path = REPO_ROOT / "tools" / "blueprint-source-synthesizer.py"
spec = importlib.util.spec_from_file_location("blueprint_source_synthesizer", synth_path)
synth_mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(synth_mod)
RuleSynthesizer = synth_mod.RuleSynthesizer


class TestNextGenScannersAndSynthesizer(unittest.TestCase):

    def setUp(self):
        self.engine = MutationEngine(seed=42)

    def test_01_sweep_testb_bit_test_scanner_detection(self):
        """1. Asserts that _sweep_testb_bit_test detects and replaces andq $1 + testq pairs."""
        asm_lines = [
            "    movq -8(%rbp), %rax\n",
            "    andq $1, %rax\n",
            "    testq %rax, %rax\n",
            "    jne .L1\n",
        ]
        muts = self.engine._sweep_testb_bit_test(asm_lines)
        self.assertEqual(len(muts), 1)
        self.assertEqual(muts[0].name, "sweep_testb_bit_test")
        self.assertEqual(muts[0].sweep_count, 1)

        mutated = self.engine.apply_mutation(asm_lines, muts[0])
        self.assertTrue(any("testb $1, %al" in l for l in mutated))
        self.assertFalse(any("andq $1, %rax" in l for l in mutated))
        self.assertFalse(any("testq %rax, %rax" in l for l in mutated))

    def test_02_apply_sweep_testb_bit_test_blueprint_helper(self):
        """2. Asserts that apply_sweep_testb_bit_test in apply_oneirogenesis_blueprint transforms registers correctly."""
        asm_lines = [
            "    andq $1, %rcx\n",
            "    testq %rcx, %rcx\n",
            "    andq $1, %r11\n",
            "    testq %r11, %r11\n",
        ]
        new_lines, count = apply_sweep_testb_bit_test(asm_lines)
        self.assertEqual(count, 2)
        self.assertEqual(new_lines[0].strip(), "testb\t$1, %cl")
        self.assertEqual(new_lines[1].strip(), "testb\t$1, %r11b")

    def test_03_rule_synthesizer_patch_generation(self):
        """3. Asserts that RuleSynthesizer constructs valid patch rules for G1 and G2 blueprints."""
        synth = RuleSynthesizer()

        bp_g2 = {
            "algo_id": "sweep_cmpq_zero_to_testq",
            "verified_savings": {"inst_count_delta": 0, "asm_size_delta": -1700}
        }
        pat, repl, name = synth.synthesize_patch(bp_g2)
        self.assertIn("testq %%%s, %%%s", repl)
        self.assertIn("Comparison shortening", name)

        bp_g1 = {
            "algo_id": "sweep_branch_straighten",
            "verified_savings": {"inst_count_delta": -495, "asm_size_delta": -1027}
        }
        pat1, repl1, name1 = synth.synthesize_patch(bp_g1)
        self.assertIn("strcmp(node->jmp_target", repl1)
        self.assertIn("Branch straightening", name1)

    def test_04_rule_synthesizer_backup_and_rollback_integrity(self):
        """4. Asserts that RuleSynthesizer creates backups and preserves file integrity during patching."""
        with tempfile.TemporaryDirectory(prefix="test_synth_") as td:
            target_c = os.path.join(td, "part5.c")
            with open(target_c, "w") as f:
                f.write("/* ANCHOR_CMP_START */\n    emit_asm(\"cmpq $%ld, %%%s\", right->val, reg_name(left->reg));\n    /* ANCHOR_CMP_END */\n")

            synth = RuleSynthesizer(journal_dir=td, target_file=target_c)
            bp_g2 = {
                "algo_id": "sweep_cmpq_zero_to_testq",
                "verified_savings": {"inst_count_delta": 0, "asm_size_delta": -1700}
            }
            pat, repl, name = synth.synthesize_patch(bp_g2)

            # Test Dry-Run Mode
            res_dry = synth.apply_patch(pat, repl, name, dry_run=True)
            self.assertTrue(res_dry)
            self.assertFalse(os.path.exists(target_c + ".bak"))

            # Test Active Patch Mode
            res_active = synth.apply_patch(pat, repl, name, dry_run=False)
            self.assertTrue(res_active)
            self.assertTrue(os.path.exists(target_c + ".bak"))

            with open(target_c) as f:
                content = f.read()
            self.assertIn("testq %%%s, %%%s", content)


if __name__ == "__main__":
    unittest.main()
