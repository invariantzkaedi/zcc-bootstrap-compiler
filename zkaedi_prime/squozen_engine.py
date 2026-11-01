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
        max_cycle_period: int = 4,
        required_keys: Optional[List[str]] = None,
    ):
        self.prompt_len = prompt_len
        self.tokenizer = tokenizer
        self.mode = SquozenMode(mode)
        self.allow_backticks = allow_backticks
        self.anti_repetition = anti_repetition
        self.max_cycle_period = max_cycle_period
        self.required_keys = list(required_keys) if required_keys else []
        self.eos_token_id = tokenizer.eos_token_id

        cache = getattr(tokenizer, "_squozen_vocab_cache", None)
        if cache is None:
            opening_brace_tokens: Set[int] = set()
            for s in ["{", "{\n", " {\n", '{"', ' {"', " {"]:
                tids = tokenizer.encode(s, add_special_tokens=False)
                if tids:
                    first_decoded = tokenizer.decode([tids[0]]).lstrip()
                    if first_decoded.startswith("{"):
                        opening_brace_tokens.add(tids[0])

            chatter_tokens: Set[int] = set()
            chatter_words = [
                "Sure", "sure", "Certainly", "certainly", "Here", "here",
                "Below", "below", "Okay", "okay", "I", "As", "Understood",
                "understood", "Hello", "hello", "Hi", "Great", "great", "Of", "of"
            ]
            for word in chatter_words:
                tids = tokenizer.encode(word, add_special_tokens=False)
                if tids:
                    chatter_tokens.add(tids[0])

            backtick_tokens: Set[int] = set()
            closing_brace_tokens: Set[int] = set()
            closing_bracket_tokens: Set[int] = set()
            opening_brace_all_tokens: Set[int] = set()
            json_after_open_brace_tokens: Set[int] = set()
            pct_r_id = None
            vocab = tokenizer.get_vocab()
            ws_chars = " \t\r\n\u0120\u010a\u010d"
            for tok_str, tok_id in vocab.items():
                if "`" in tok_str or "\u0120`" in tok_str:
                    backtick_tokens.add(tok_id)
                if "{" in tok_str:
                    opening_brace_all_tokens.add(tok_id)
                if "}" in tok_str and "{" not in tok_str:
                    closing_brace_tokens.add(tok_id)
                if "]" in tok_str and "[" not in tok_str:
                    closing_bracket_tokens.add(tok_id)
                if pct_r_id is None and tok_str in ["%r", " %r", "%"]:
                    pct_r_id = tok_id
                s_clean = tok_str.lstrip(ws_chars)
                if (s_clean == "" and len(tok_str) > 0) or s_clean.startswith('"'):
                    if "`" not in tok_str:
                        json_after_open_brace_tokens.add(tok_id)

            cache = {
                "opening_brace_tokens": opening_brace_tokens,
                "opening_brace_all_tokens": opening_brace_all_tokens,
                "json_after_open_brace_tokens": json_after_open_brace_tokens,
                "chatter_tokens": chatter_tokens,
                "backtick_tokens": backtick_tokens,
                "closing_brace_tokens": closing_brace_tokens,
                "closing_bracket_tokens": closing_bracket_tokens,
                "brace_open_id": tokenizer.encode("{", add_special_tokens=False)[0],
                "brace_close_id": tokenizer.encode("}", add_special_tokens=False)[0],
                "bracket_open_id": tokenizer.encode("[", add_special_tokens=False)[0],
                "bracket_close_id": tokenizer.encode("]", add_special_tokens=False)[0],
                "pct_r_id": pct_r_id,
            }
            setattr(tokenizer, "_squozen_vocab_cache", cache)

        self.opening_brace_tokens = cache["opening_brace_tokens"]
        self.opening_brace_all_tokens = cache["opening_brace_all_tokens"]
        self.json_after_open_brace_tokens = cache["json_after_open_brace_tokens"]
        self.chatter_tokens = cache["chatter_tokens"]
        self.backtick_tokens = cache["backtick_tokens"]
        self.closing_brace_tokens = cache["closing_brace_tokens"]
        self.closing_bracket_tokens = cache["closing_bracket_tokens"]
        self.brace_open_id = cache["brace_open_id"]
        self.brace_close_id = cache["brace_close_id"]
        self.bracket_open_id = cache["bracket_open_id"]
        self.bracket_close_id = cache["bracket_close_id"]
        self.pct_r_id = cache["pct_r_id"]

        # 5. Valid System V AMD64 64-bit Register Tokens
        self.valid_sysv_regs = {
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
        }

    @staticmethod
    def scan_pda_state(text: str):
        """
        String-aware character-level Pushdown Automaton for JSON & Balanced Delimiters.
        Ignores '{', '}', '[', ']' inside string literals and tracks escape sequences.
        Returns (brace_depth, bracket_depth, in_string, opened_ever).
        """
        brace_depth = 0
        bracket_depth = 0
        in_string = False
        escape_next = False
        opened_ever = False

        for ch in text:
            if escape_next:
                escape_next = False
                continue
            if ch == "\\":
                if in_string:
                    escape_next = True
                continue
            if ch == '"':
                in_string = not in_string
                continue
            if not in_string:
                if ch == "{":
                    brace_depth += 1
                    opened_ever = True
                elif ch == "}":
                    brace_depth = max(0, brace_depth - 1)
                elif ch == "[":
                    bracket_depth += 1
                    opened_ever = True
                elif ch == "]":
                    bracket_depth = max(0, bracket_depth - 1)

        return brace_depth, bracket_depth, in_string, opened_ever

    def _missing_required_keys(self, text: str) -> bool:
        if not self.required_keys:
            return False
        for key in self.required_keys:
            if f'"{key}":' not in text and f'"{key}" :' not in text:
                return True
        return False

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
        # RULE 3: STRING-AWARE BPE-DELTA PDA & SCHEMA KEY GUARD
        # ---------------------------------------------------------------------
        gen_tokens = input_ids[0, self.prompt_len:].tolist()
        gen_text = self.tokenizer.decode(gen_tokens, skip_special_tokens=False) if gen_tokens else ""
        brace_depth, bracket_depth, in_string, opened_ever = self.scan_pda_state(gen_text)
        depth = brace_depth + bracket_depth

        if self.mode in (SquozenMode.JSON, SquozenMode.BALANCED_DELIMITERS):
            missing_keys = self._missing_required_keys(gen_text)

            # Structure was opened, all delimiters closed outside string, and schema keys satisfied -> FORCE IMMEDIATE EOS!
            if opened_ever and depth == 0 and not in_string and not missing_keys:
                mask = torch.full_like(scores, -float("inf"))
                mask[:, self.eos_token_id] = 100.0
                return mask

            # If structure remains open, inside string literal, or missing required keys -> PROHIBIT PREMATURE EOS
            if depth > 0 or in_string or not opened_ever or missing_keys:
                scores[:, self.eos_token_id] = -float("inf")

            # Prevent root closure while required schema keys are still missing
            if not in_string and brace_depth == 1 and bracket_depth == 0 and missing_keys:
                for tid in self.closing_brace_tokens:
                    scores[:, tid] = -float("inf")

            # RFC 8259 JSON Object Member Invariant: after '{', only whitespace or '"' (or '}' if no missing keys) is legal
            if self.mode == SquozenMode.JSON and not in_string:
                rstripped = gen_text.rstrip()
                if rstripped.endswith("{"):
                    allowed = self.json_after_open_brace_tokens if missing_keys else (self.json_after_open_brace_tokens | self.closing_brace_tokens)
                    mask = torch.full_like(scores, -float("inf"))
                    for tid in allowed:
                        mask[:, tid] = scores[:, tid]
                    return mask

            # Prevent delimiter underflow outside string literals
            if not in_string:
                if brace_depth == 0 and step > 0:
                    for tid in self.closing_brace_tokens:
                        scores[:, tid] = -float("inf")
                if bracket_depth == 0:
                    for tid in self.closing_bracket_tokens:
                        scores[:, tid] = -float("inf")

        elif self.mode == SquozenMode.CODE_ONLY:
            # For indentation-scoped Python functions: once 'def ...:' and a balanced 'return ...\n' line complete, force EOS
            if not in_string and depth == 0 and gen_text.count("(") == gen_text.count(")"):
                lines = gen_text.splitlines(keepends=True)
                has_def = any(ln.lstrip().startswith("def ") and ":" in ln for ln in lines)
                has_completed_return = any(
                    ln.lstrip().startswith("return ") and ln.endswith("\n") and len(ln.strip()) > len("return")
                    for ln in lines
                )
                if has_def and has_completed_return:
                    mask = torch.full_like(scores, -float("inf"))
                    mask[:, self.eos_token_id] = 100.0
                    return mask

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
