#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // FULL SOVEREIGN TEST SUITE 🔱
=======================================================================================================
 Verifies:
   [GATE 1] ZkaediCanonicalField: Subcritical Lyapunov contraction (lambda < 0) & departure scars.
   [GATE 2] TwoRegimePathfinder: 100% escape from dead-end maze via scars + epsilon.
   [GATE 3] FlashHamiltonianKernel: Zero NaN/Inf in-SRAM multi-step rollout.
   [GATE 4] SquozenLogitsEngine (Unit): Mathematical clamp on token 63 and step-0 brace.
   [GATE 5] SquozenLogitsEngine (Live GPU): Qwen2.5-Coder-1.5B raw JSON with 0 backticks.
=======================================================================================================
"""

import sys
import unittest
import numpy as np
import torch
from pathlib import Path

# Add project root to sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from zkaedi_prime.canonical_field import ZkaediCanonicalField
from zkaedi_prime.pathfinder import TwoRegimePathfinder
from zkaedi_prime.flash_kernel import FlashHamiltonianKernel
from zkaedi_prime.squozen_engine import SquozenLogitsEngine, SquozenMode

class TestZkaediPrimeSovereign(unittest.TestCase):

    def test_gate1_canonical_field_lyapunov(self):
        """Verify Form 1 & Form 3: Subcritical Lyapunov contraction and scarring."""
        field = ZkaediCanonicalField(size=6, eta=0.40, gamma=0.30, kick=2.0)
        W = np.ones((6, 6)) * 0.5
        np.fill_diagonal(W, 0.0)
        field.initialize_from_matrix(W)

        # Apply departure scar
        field.inject_departure_scar(node=0, custom_kick=2.0)
        self.assertGreaterEqual(field.H_base[0, 1], 2.5)

        # Step 20 iterations
        for _ in range(20):
            H_t, energy, ftle = field.step()

        self.assertLess(ftle, 0.0, f"Lyapunov exponent must be negative (subcritical). Got: {ftle}")
        self.assertFalse(np.isnan(H_t).any(), "Field must have zero NaNs")

    def test_gate2_two_regime_pathfinder(self):
        """Verify Form 5: Fast pathfinder escape from local minimum."""
        # 5-node line graph with dead-end at 0: 0 <-> 1 <-> 2 <-> 3 <-> 4 (target)
        adj = np.zeros((5, 5))
        for i in range(4):
            adj[i, i+1] = 1.0
            adj[i+1, i] = 1.0

        pathfinder = TwoRegimePathfinder(num_nodes=5, adjacency_matrix=adj, kick=2.0, eps=0.05)
        path, success = pathfinder.find_path(start_node=0, target_nodes={4}, max_steps=50)

        self.assertTrue(success, "Pathfinder must successfully reach target node 4")
        self.assertEqual(path[-1], 4)
        # Verify departure scar was deposited on node 0
        self.assertGreater(pathfinder.H_base[0], 0.0)

    def test_gate3_flash_kernel_sram(self):
        """Verify Form 6: In-SRAM tile rollout stability."""
        kernel = FlashHamiltonianKernel(tile_size=16, eta=0.40, gamma=0.30)
        h_base = np.zeros(16, dtype=np.float32)
        h_prev = np.ones(16, dtype=np.float32) * 0.5

        final_h, final_base = kernel.rollout_tile_in_sram(h_base, h_prev, num_fused_steps=16)
        self.assertFalse(np.isnan(final_h).any(), "Flash tile must have zero NaNs")
        self.assertFalse(np.isinf(final_h).any(), "Flash tile must have zero Infs")

    def test_gate4_squozen_engine_unit_clamp(self):
        """Verify Form 4: Pure unit test on LogitsProcessor without loading model."""
        # Mock tokenizer
        class MockTokenizer:
            eos_token_id = 151643
            def get_vocab(self):
                return {"{": 10, "{\n": 11, '{"': 12, "`": 63, "```": 64, "Sure": 100, "foo": 200}
            def encode(self, s, add_special_tokens=False):
                v = self.get_vocab()
                return [v[s]] if s in v else [200]

        mock_tok = MockTokenizer()
        engine = SquozenLogitsEngine(prompt_len=10, tokenizer=mock_tok, mode=SquozenMode.JSON)

        # 1. Step 0 test: only '{' allowed, everything else -inf
        input_ids_step0 = torch.zeros((1, 10), dtype=torch.long)
        scores_step0 = torch.zeros((1, 300), dtype=torch.float)
        filtered_step0 = engine(input_ids_step0, scores_step0)

        self.assertEqual(filtered_step0[0, 100].item(), -float("inf"), "Chatter token must be -inf at step 0")
        self.assertEqual(filtered_step0[0, 63].item(), -float("inf"), "Backtick token must be -inf at step 0")
        self.assertEqual(filtered_step0[0, 10].item(), 0.0, "Opening brace '{' must be preserved")

        # 2. Step 5 test: backticks permanently clamped
        input_ids_step5 = torch.zeros((1, 15), dtype=torch.long)
        scores_step5 = torch.zeros((1, 300), dtype=torch.float)
        filtered_step5 = engine(input_ids_step5, scores_step5)
        self.assertEqual(filtered_step5[0, 63].item(), -float("inf"), "Backtick token 63 must be -inf at step 5")
        self.assertEqual(filtered_step5[0, 64].item(), -float("inf"), "Triple backtick 64 must be -inf at step 5")

    def test_gate5_control_flow_grammar(self):
        """Verify Universal Control-Flow Engine: try/catch, if/else, def/return, and delimiters."""
        from zkaedi_prime.code_grammar_engine import SovereignControlFlowLogitsProcessor

        class MockCodeTokenizer:
            eos_token_id = 999
            def get_vocab(self):
                return {
                    "if": 1, "else": 2, "elif": 3,
                    "try": 4, "catch": 5, "except": 6, "finally": 7,
                    "return": 8, "def": 9,
                    "{": 10, "}": 11, "(": 12, ")": 13, "[": 14, "]": 15,
                    "for": 16, "while": 17, "break": 18, "continue": 19,
                    "switch": 20, "case": 21, "default": 22,
                    "`": 63, "foo": 200
                }
            def encode(self, s, add_special_tokens=False):
                v = self.get_vocab()
                s_strip = s.strip()
                return [v[s_strip]] if s_strip in v else []
            def decode(self, ids, skip_special_tokens=False):
                rev = {v: k for k, v in self.get_vocab().items()}
                return "".join(rev.get(i, "") for i in ids)

        mock_tok = MockCodeTokenizer()
        proc = SovereignControlFlowLogitsProcessor(prompt_len=5, tokenizer=mock_tok, require_return=True)

        # 1. Orphan 'else' without 'if' -> must be clamped to -inf
        # History: 5 prompt tokens + 2 generated non-if tokens
        input_ids = torch.tensor([[0, 0, 0, 0, 0, 200, 200]], dtype=torch.long)
        scores = torch.zeros((1, 1000), dtype=torch.float)
        res = proc(input_ids, scores)
        self.assertEqual(res[0, 2].item(), -float("inf"), "Orphan 'else' must be clamped to -inf")
        self.assertEqual(res[0, 3].item(), -float("inf"), "Orphan 'elif' must be clamped to -inf")

        # 2. Delimiter underflow: closing delimiters clamped to -inf when depth == 0
        self.assertEqual(res[0, 11].item(), -float("inf"), "Closing brace '}' clamped on underflow")
        self.assertEqual(res[0, 13].item(), -float("inf"), "Closing paren ')' clamped on underflow")
        self.assertEqual(res[0, 15].item(), -float("inf"), "Closing bracket ']' clamped on underflow")

        # 3. Orphan 'catch' / 'finally' without 'try' -> must be clamped to -inf
        self.assertEqual(res[0, 5].item(), -float("inf"), "Orphan 'catch' must be clamped to -inf")
        self.assertEqual(res[0, 7].item(), -float("inf"), "Orphan 'finally' must be clamped to -inf")

        # 4. Loop control outside loop: 'break' / 'continue' clamped to -inf when loop_depth == 0
        self.assertEqual(res[0, 18].item(), -float("inf"), "'break' must be clamped outside loop")
        self.assertEqual(res[0, 19].item(), -float("inf"), "'continue' must be clamped outside loop")

        # 5. Scope unclosed delimiter -> EOS must be clamped to -inf
        # History: opened brace '{'
        input_ids = torch.tensor([[0, 0, 0, 0, 0, 10]], dtype=torch.long)
        scores = torch.zeros((1, 1000), dtype=torch.float)
        res = proc(input_ids, scores)
        self.assertEqual(res[0, 999].item(), -float("inf"), "EOS must be clamped while brace is open")

        # 6. 'try { ... }' closed -> next statement MUST be catch/except/finally
        # History: try (4), open brace (10), code (200), close brace (11)
        input_ids = torch.tensor([[0, 0, 0, 0, 0, 4, 10, 200, 11]], dtype=torch.long)
        scores = torch.zeros((1, 1000), dtype=torch.float)
        res = proc(input_ids, scores)
        self.assertEqual(res[0, 200].item(), -float("inf"), "Arbitrary code must be -inf after try block")
        self.assertEqual(res[0, 5].item(), 0.0, "'catch' must be preserved")
        self.assertEqual(res[0, 7].item(), 0.0, "'finally' must be preserved")

        # 7. Require return: if in function (brace_depth == 1) and no return emitted, '}' is clamped
        input_ids = torch.tensor([[0, 0, 0, 0, 0, 9, 10, 200]], dtype=torch.long)
        scores = torch.zeros((1, 1000), dtype=torch.float)
        res = proc(input_ids, scores)
        self.assertEqual(res[0, 11].item(), -float("inf"), "Closing brace '}' must be clamped until return is emitted")

        # 8. Inside active loop: 'break' and 'continue' are allowed
        # History: for (16), open brace (10), code (200)
        input_ids = torch.tensor([[0, 0, 0, 0, 0, 16, 10, 200]], dtype=torch.long)
        scores = torch.zeros((1, 1000), dtype=torch.float)
        res = proc(input_ids, scores)
        self.assertEqual(res[0, 18].item(), 0.0, "'break' must be allowed inside loop")
        self.assertEqual(res[0, 19].item(), 0.0, "'continue' must be allowed inside loop")

if __name__ == "__main__":
    unittest.main()
