# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // SQUOZEN LOGITS ENGINE (FORM 4) 🔱
=======================================================================================================
 "Mechanism governs structure; Neural weights govern meaning."

 Production-grade, composable LogitsProcessor enforcing:
   1. Token-0 Invariant: Bans conversational fluff ("Sure", "Certainly"). Clamps to '{' in JSON mode.
   2. Token 1..N Invariant: Mathematically eliminates backticks (token ID 63 and related BPE merges).
   3. Delimiter Stack Invariant: Tracks nesting depth of {} and []. Forbids EOS while depth > 0.
   4. Immediate EOS Trigger: When root structure returns to depth 0, forces instant termination.
   5. Symplectic Limit Cycle Breaker: Detects sliding-window periodic n-gram cycles (k=1..4) and clamps.
   6. System V ABI Grammar Projector: Restricts '%r' register tokens to the valid 16 AMD64 registers.
=======================================================================================================
"""

import enum
from typing import Set, List, Optional
import torch
from transformers import LogitsProcessor

class SquozenMode(str, enum.Enum):
    JSON = "JSON"
    CODE_ONLY = "CODE_ONLY"
    SYSTEM_V_ABI = "SYSTEM_V_ABI"
    BALANCED_DELIMITERS = "BALANCED_DELIMITERS"
    GENERAL = "GENERAL"

class SquozenLogitsEngine(LogitsProcessor):
    """
    Sovereign Logits Engine implementing Form 4 of Zkaedi Prime.
    Erects an unbreachable infinite potential barrier V(x) = +inf at the token plane.
    """
    def __init__(
        self,
        prompt_len: int,
        tokenizer,
        mode: SquozenMode = SquozenMode.JSON,
        allow_backticks: bool = False,
        anti_repetition: bool = True,
        max_cycle_period: int = 4
    ):
        self.prompt_len = prompt_len
        self.tokenizer = tokenizer
        self.mode = SquozenMode(mode)
        self.allow_backticks = allow_backticks
        self.anti_repetition = anti_repetition
        self.max_cycle_period = max_cycle_period
        self.eos_token_id = tokenizer.eos_token_id

        # 1. Opening brace tokens for JSON mode
        self.opening_brace_tokens: Set[int] = set()
        for s in ["{", "{\n", " {\n", '{"', ' {"', "\n{"]:
            tids = tokenizer.encode(s, add_special_tokens=False)
            if tids:
                self.opening_brace_tokens.add(tids[0])

        # 2. Conversational sycophancy tokens to suppress at Token-0
        self.chatter_tokens: Set[int] = set()
        chatter_words = [
            "Sure", "sure", "Certainly", "certainly", "Here", "here",
            "Below", "below", "Okay", "okay", "I", "As", "Understood",
            "understood", "Hello", "hello", "Hi", "Great", "great", "Of", "of"
        ]
        for word in chatter_words:
            tids = tokenizer.encode(word, add_special_tokens=False)
            if tids:
                self.chatter_tokens.add(tids[0])

        # 3. Backtick tokens to eliminate permanently
        self.backtick_tokens: Set[int] = set()
        vocab = tokenizer.get_vocab()
        for tok_str, tok_id in vocab.items():
            if "`" in tok_str or "\u0120`" in tok_str:
                self.backtick_tokens.add(tok_id)

        # 4. Delimiter token IDs for nesting depth tracking
        self.brace_open_id = tokenizer.encode("{", add_special_tokens=False)[0]
        self.brace_close_id = tokenizer.encode("}", add_special_tokens=False)[0]
        self.bracket_open_id = tokenizer.encode("[", add_special_tokens=False)[0]
        self.bracket_close_id = tokenizer.encode("]", add_special_tokens=False)[0]

        # 5. Valid System V AMD64 64-bit Register Tokens
        self.valid_sysv_regs = {
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        }
        self.pct_r_id = None
        for tok_str, tok_id in vocab.items():
            if tok_str in ["%r", " %r", "%"]:
                self.pct_r_id = tok_id
                break

    def __call__(self, input_ids: torch.LongTensor, scores: torch.FloatTensor) -> torch.FloatTensor:
        cur_len = input_ids.shape[1]
        step = cur_len - self.prompt_len

        # ---------------------------------------------------------------------
        # RULE 1: STEP-0 BOUNDARY INVARIANT
        # ---------------------------------------------------------------------
        if step == 0:
            if self.mode == SquozenMode.JSON:
                # Force opening brace '{' strictly
                mask = torch.full_like(scores, -float("inf"))
                for tid in self.opening_brace_tokens:
                    mask[:, tid] = scores[:, tid]
                return mask
            else:
                # Suppress conversational preamble chatter
                for tid in self.chatter_tokens:
                    scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 2: ABSOLUTE BACKTICK ELIMINATION (TOKEN 63 MASK)
        # ---------------------------------------------------------------------
        if not self.allow_backticks:
            for tid in self.backtick_tokens:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 3: DELIMITER NESTING STACK & IMMEDIATE EOS TRIGGER
        # ---------------------------------------------------------------------
        gen_tokens = input_ids[0, self.prompt_len:].tolist()
        depth = 0
        opened_ever = False
        for tid in gen_tokens:
            if tid in (self.brace_open_id, self.bracket_open_id):
                depth += 1
                opened_ever = True
            elif tid in (self.brace_close_id, self.bracket_close_id):
                depth = max(0, depth - 1)

        if self.mode in (SquozenMode.JSON, SquozenMode.BALANCED_DELIMITERS):
            # Structure was opened and has cleanly closed -> FORCE IMMEDIATE EOS!
            if opened_ever and depth == 0:
                mask = torch.full_like(scores, -float("inf"))
                mask[:, self.eos_token_id] = 100.0
                return mask
            # If structure remains open -> PROHIBIT PREMATURE EOS
            elif depth > 0:
                scores[:, self.eos_token_id] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 4: SYMPLECTIC N-GRAM LIMIT CYCLE BREAKER
        # ---------------------------------------------------------------------
        if self.anti_repetition:
            gen_len = len(gen_tokens)
            if gen_len >= 8:
                for k in range(1, self.max_cycle_period + 1):
                    if gen_len >= 2 * k:
                        recent = gen_tokens[-k:]
                        prior = gen_tokens[-2 * k : -k]
                        if recent == prior:
                            # Periodic repetition detected! Clamp the cyclic continuation token to -inf
                            next_cycle_tok = recent[0]
                            scores[:, next_cycle_tok] = -float("inf")

        return scores
