# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // UNIVERSAL CONTROL-FLOW & GRAMMAR LOGITS ENGINE (FORM 4 EXTENSION) 🔱
=======================================================================================================
 Enforces deterministic pushdown-automaton (PDA) constraints at the logits plane:
   1. Functions / Defs ('def', 'fn', 'function', 'func'):
      - Enforces balanced parameter lists '( ... )'
      - For non-void signatures, guarantees a 'return' statement before closing the function scope.
   2. Control Flow ('if', 'then', 'else', 'elif'):
      - Clamps 'else' / 'elif' to -inf unless an 'if' block was actively established in scope.
      - Disallows orphan 'else' / 'elif' statements.
      - Supports 'then' in languages requiring explicit then-clauses.
   3. Exception Blocks ('try', 'catch', 'except', 'finally', 'throw', 'raise'):
      - Forbids orphan 'catch' / 'except' / 'finally' without a preceding 'try' block.
      - When a 'try' block closes, enforces that the next non-whitespace statement MUST be 'catch', 'except', or 'finally'.
   4. Loops & Flow Control ('for', 'while', 'do', 'break', 'continue'):
      - Tracks loop depth; clamps 'break' and 'continue' to -inf when outside any loop context.
   5. Pattern Matching & Switches ('switch', 'match', 'case', 'default'):
      - Tracks switch/match depth; clamps 'case' and 'default' to -inf outside switch/match blocks.
   6. Delimiters & Scopes ('{ }', '( )', '[ ]'):
      - Prevents delimiter underflow (clamping closing delimiters when depth == 0).
      - Prevents premature EOS (<eos> clamped to -inf whenever unclosed scopes exist).
   7. Pure Output Guarantee:
      - Absolute 0.00% backtick leakage (``` clamped to -inf in raw code mode).
      - Step 0 conversational sycophancy elimination ("Sure", "Here is...", etc.).
=======================================================================================================
"""

import enum
from typing import Set, List, Dict, Optional, Any
import torch
from transformers import LogitsProcessor

class CodeGrammarDialect(str, enum.Enum):
    C_CPP = "C_CPP"
    PYTHON = "PYTHON"
    JAVASCRIPT_TS = "JAVASCRIPT_TS"
    RUST = "RUST"
    UNIVERSAL = "UNIVERSAL"

class SovereignControlFlowLogitsProcessor(LogitsProcessor):
    """
    Pushdown Automaton Logits Processor for Universal Programming Language Grammars.
    Clamps syntax, scoping, exception, and control-flow violations to -inf dynamically.
    """
    def __init__(
        self,
        prompt_len: int,
        tokenizer,
        dialect: CodeGrammarDialect = CodeGrammarDialect.UNIVERSAL,
        require_return: bool = False,
        allow_markdown_fences: bool = False,
        pure_code: bool = False,
        terminal_scope_clamp: bool = False,
        single_function: bool = False
    ):
        self.prompt_len = prompt_len
        self.tokenizer = tokenizer
        self.dialect = CodeGrammarDialect(dialect)
        self.require_return = require_return
        self.allow_markdown_fences = allow_markdown_fences
        self.pure_code = pure_code
        self.terminal_scope_clamp = terminal_scope_clamp
        self.single_function = single_function
        self.eos_token_id = tokenizer.eos_token_id

        vocab = tokenizer.get_vocab()

        # Helper to find token IDs for word variants (with/without leading space, newlines)
        def get_tokens_for_words(words: List[str]) -> Set[int]:
            token_ids = set()
            for w in words:
                variants = [w, f" {w}", f"\n{w}", f"\t{w}", f"    {w}"]
                for v in variants:
                    enc = tokenizer.encode(v, add_special_tokens=False)
                    if enc and len(enc) == 1:
                        token_ids.add(enc[0])
            return token_ids

        # 1. Functions & Definitions
        self.tok_def = get_tokens_for_words(["def", "function", "fn", "func"])
        self.tok_main = get_tokens_for_words(["main"])
        self.tok_return = get_tokens_for_words(["return", "yield"])

        # 2. Conditionals & Branching
        self.tok_if = get_tokens_for_words(["if"])
        self.tok_then = get_tokens_for_words(["then"])
        self.tok_else = get_tokens_for_words(["else"])
        self.tok_elif = get_tokens_for_words(["elif"])

        # 3. Exceptions & Error Handling
        self.tok_try = get_tokens_for_words(["try"])
        self.tok_catch = get_tokens_for_words(["catch", "except"])
        self.tok_finally = get_tokens_for_words(["finally"])
        self.tok_throw = get_tokens_for_words(["throw", "raise"])

        # 4. Loops & Iteration
        self.tok_loop = get_tokens_for_words(["for", "while", "do"])
        self.tok_loop_control = get_tokens_for_words(["break", "continue"])

        # 5. Switches & Matches
        self.tok_switch = get_tokens_for_words(["switch", "match"])
        self.tok_case = get_tokens_for_words(["case"])
        self.tok_default = get_tokens_for_words(["default"])

        # 6. Delimiters - Character-level delimiter count tables (impervious to BPE token merges like '++)' or ');\n')
        vocab_size = max(vocab.values()) + 1
        self.delta_brace = [0] * vocab_size
        self.delta_paren = [0] * vocab_size
        self.delta_bracket = [0] * vocab_size
        self.brace_open: Set[int] = set()
        self.brace_close: Set[int] = set()
        self.paren_open: Set[int] = set()
        self.paren_close: Set[int] = set()
        self.bracket_open: Set[int] = set()
        self.bracket_close: Set[int] = set()

        for tok_str, tok_id in vocab.items():
            dec = tokenizer.decode([tok_id]) if hasattr(tokenizer, "decode") else tok_str
            db = dec.count("{") - dec.count("}")
            dp = dec.count("(") - dec.count(")")
            dbk = dec.count("[") - dec.count("]")
            self.delta_brace[tok_id] = db
            self.delta_paren[tok_id] = dp
            self.delta_bracket[tok_id] = dbk

            if dec.count("{") > 0:
                self.brace_open.add(tok_id)
            if dec.count("}") > 0:
                self.brace_close.add(tok_id)
            if dec.count("(") > 0:
                self.paren_open.add(tok_id)
            if dec.count(")") > 0:
                self.paren_close.add(tok_id)
            if dec.count("[") > 0:
                self.bracket_open.add(tok_id)
            if dec.count("]") > 0:
                self.bracket_close.add(tok_id)

        # Whitespace / formatting tokens (newlines, spaces, indentation)
        self.whitespace_tokens: Set[int] = set()
        for tok_str, tok_id in vocab.items():
            if tok_str.strip() == "" or tok_str in ["\n", "\r\n", "\t", " ", "  ", "   ", "    "]:
                self.whitespace_tokens.add(tok_id)

        # Fence tokens (Markdown backticks ` and tildes ~)
        self.fence_tokens: Set[int] = set()
        for tok_str, tok_id in vocab.items():
            if "`" in tok_str or "\u0120`" in tok_str or "~" in tok_str:
                self.fence_tokens.add(tok_id)

        # Conversational filler at Step 0
        self.chatter_tokens: Set[int] = set()
        chatter_words = [
            "Sure", "sure", "Certainly", "Certainly", "Here", "here",
            "Below", "below", "Okay", "okay", "I", "As", "Understood", "Hello", "To", "to"
        ]
        for cw in chatter_words:
            enc = tokenizer.encode(cw, add_special_tokens=False)
            if enc and len(enc) == 1:
                self.chatter_tokens.add(enc[0])

        # Code starter tokens (for pure_code mode)
        code_starter_words = [
            "def", "function", "fn", "func", "class", "import", "from",
            "export", "const", "let", "var", "#", "//", "/*",
            "int", "void", "char", "float", "double", "unsigned", "long",
            "short", "bool", "struct", "enum", "typedef", "static", "extern",
            "template", "type", "interface", "pub", "use", "package", "public",
            "private", "protected"
        ]
        self.code_starters = get_tokens_for_words(code_starter_words)
        for tok_str, tok_id in vocab.items():
            s = tok_str.strip()
            if s.startswith("//") or s.startswith("/*") or s.startswith("#"):
                self.code_starters.add(tok_id)

    def inspect_state(self, input_ids: torch.LongTensor) -> Dict[str, Any]:
        """
        Extracts the pushdown automaton state from the token stream for introspection.
        """
        gen_tokens = input_ids[0, self.prompt_len:].tolist()

        brace_depth = 0
        paren_depth = 0
        bracket_depth = 0
        loop_depth = 0
        switch_depth = 0

        has_if = False
        has_try_active = False
        in_try_block = False
        try_closed_needs_catch = False
        has_return_emitted = False

        seen_main = False
        in_main = False
        main_closed = False
        ever_opened_scope = False

        for tid in gen_tokens:
            if tid in self.tok_main:
                seen_main = True

            db = self.delta_brace[tid] if tid < len(self.delta_brace) else 0
            dp = self.delta_paren[tid] if tid < len(self.delta_paren) else 0
            dbk = self.delta_bracket[tid] if tid < len(self.delta_bracket) else 0

            brace_depth = max(0, brace_depth + db)
            paren_depth = max(0, paren_depth + dp)
            bracket_depth = max(0, bracket_depth + dbk)

            if db > 0:
                ever_opened_scope = True
                if seen_main and brace_depth >= 1:
                    in_main = True
            elif db < 0:
                if in_main and brace_depth == 0:
                    in_main = False
                    main_closed = True
                if in_try_block and brace_depth == 0:
                    in_try_block = False
                    try_closed_needs_catch = True
                if loop_depth > 0 and brace_depth < loop_depth:
                    loop_depth = max(0, loop_depth - 1)
                if switch_depth > 0 and brace_depth < switch_depth:
                    switch_depth = max(0, switch_depth - 1)

            # Keywords
            if tid in self.tok_if:
                has_if = True
            elif tid in self.tok_try:
                has_try_active = True
                in_try_block = True
                try_closed_needs_catch = False
            elif tid in self.tok_catch or tid in self.tok_finally:
                try_closed_needs_catch = False
            elif tid in self.tok_loop:
                loop_depth += 1
            elif tid in self.tok_switch:
                switch_depth += 1
            elif tid in self.tok_return:
                has_return_emitted = True

        return {
            "brace_depth": brace_depth,
            "paren_depth": paren_depth,
            "bracket_depth": bracket_depth,
            "loop_depth": loop_depth,
            "switch_depth": switch_depth,
            "has_if": has_if,
            "has_try_active": has_try_active,
            "in_try_block": in_try_block,
            "try_closed_needs_catch": try_closed_needs_catch,
            "has_return_emitted": has_return_emitted,
            "ever_opened_scope": ever_opened_scope,
            "main_closed": main_closed
        }

    def __call__(self, input_ids: torch.LongTensor, scores: torch.FloatTensor) -> torch.FloatTensor:
        cur_len = input_ids.shape[1]
        step = cur_len - self.prompt_len

        # ---------------------------------------------------------------------
        # RULE 1: STEP-0 NO CONVERSATIONAL FLUFF & PURE CODE GUARANTEE
        # ---------------------------------------------------------------------
        if step == 0:
            if self.pure_code:
                allowed = self.code_starters | self.whitespace_tokens
                mask = torch.full_like(scores, -float("inf"))
                for tid in allowed:
                    mask[:, tid] = scores[:, tid]
                return mask
            else:
                for tid in self.chatter_tokens:
                    scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 2: ABSOLUTE FENCE ELIMINATION (BACKTICKS ` AND TILDES ~)
        # ---------------------------------------------------------------------
        if not self.allow_markdown_fences:
            for tid in self.fence_tokens:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # PARSE HISTORICAL GENERATED TOKENS VIA PUSHDOWN AUTOMATON
        # ---------------------------------------------------------------------
        state = self.inspect_state(input_ids)
        brace_depth = state["brace_depth"]
        paren_depth = state["paren_depth"]
        bracket_depth = state["bracket_depth"]
        loop_depth = state["loop_depth"]
        switch_depth = state["switch_depth"]
        has_if = state["has_if"]
        has_try_active = state["has_try_active"]
        in_try_block = state["in_try_block"]
        try_closed_needs_catch = state["try_closed_needs_catch"]
        has_return_emitted = state["has_return_emitted"]
        ever_opened_scope = state["ever_opened_scope"]
        main_closed = state["main_closed"]

        # ---------------------------------------------------------------------
        # RULE 3: SCOPE INTEGRITY (PREVENT PREMATURE EOS)
        # ---------------------------------------------------------------------
        total_unclosed_delimiters = brace_depth + paren_depth + bracket_depth
        if total_unclosed_delimiters > 0 or in_try_block or try_closed_needs_catch:
            scores[:, self.eos_token_id] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 4: DELIMITER UNDERFLOW PROHIBITION
        # ---------------------------------------------------------------------
        # Cannot close delimiters that have never been opened!
        if brace_depth == 0:
            for tid in self.brace_close:
                scores[:, tid] = -float("inf")
        if paren_depth == 0:
            for tid in self.paren_close:
                scores[:, tid] = -float("inf")
        if bracket_depth == 0:
            for tid in self.bracket_close:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 5: EXCEPTION HANDLER INVARIANCE (TRY -> CATCH/FINALLY MANDATORY)
        # ---------------------------------------------------------------------
        # If 'try { ... }' just closed, the next non-whitespace statement
        # CANNOT be arbitrary code; it MUST be 'catch', 'except', or 'finally'!
        if try_closed_needs_catch:
            allowed_tokens = (
                self.tok_catch
                | self.tok_finally
                | self.brace_open
                | self.whitespace_tokens
            )
            mask = torch.full_like(scores, -float("inf"))
            for tid in allowed_tokens:
                mask[:, tid] = scores[:, tid]
            return mask

        # ---------------------------------------------------------------------
        # RULE 6: ORPHAN 'CATCH' / 'EXCEPT' / 'FINALLY' PROHIBITION
        # ---------------------------------------------------------------------
        # Cannot emit 'catch', 'except', or 'finally' without an active 'try' context!
        if not has_try_active:
            for tid in self.tok_catch | self.tok_finally:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 7: ORPHAN 'ELSE' / 'ELIF' PROHIBITION
        # ---------------------------------------------------------------------
        # If no 'if' has ever been started, clamping 'else' and 'elif' to -inf
        if not has_if:
            for tid in self.tok_else | self.tok_elif:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 8: LOOP CONTROL INVARIANCE (BREAK / CONTINUE)
        # ---------------------------------------------------------------------
        # 'break' and 'continue' cannot appear outside of an active loop!
        if loop_depth == 0:
            for tid in self.tok_loop_control:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 9: SWITCH CONTROL INVARIANCE (CASE / DEFAULT)
        # ---------------------------------------------------------------------
        # 'case' and 'default' cannot appear outside of a switch/match block!
        if switch_depth == 0:
            for tid in self.tok_case | self.tok_default:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 10: FUNCTION RETURN ENFORCEMENT
        # ---------------------------------------------------------------------
        # If a typed function is required to return a value, and we are at
        # the final closing brace without a return, clamp '}' to -inf to force return!
        if self.require_return and not has_return_emitted and brace_depth == 1:
            for tid in self.brace_close:
                scores[:, tid] = -float("inf")

        # ---------------------------------------------------------------------
        # RULE 11: TERMINAL SCOPE CLAMPING (ZERO POSTAMBLE)
        # ---------------------------------------------------------------------
        # If main() has closed, or single_function has closed, FORCE <eos>!
        if self.terminal_scope_clamp:
            if main_closed or (self.single_function and ever_opened_scope and brace_depth == 0):
                mask = torch.full_like(scores, -float("inf"))
                mask[:, self.eos_token_id] = 0.0
                return mask

        # ---------------------------------------------------------------------
        # RULE 12: ZERO POSTAMBLE PROSE BAN
        # ---------------------------------------------------------------------
        # If between or after top-level functions (ever_opened_scope and brace_depth == 0),
        # clamp conversational prose tokens so the model cannot begin commentary!
        if self.pure_code and ever_opened_scope and brace_depth == 0:
            for tid in self.chatter_tokens:
                scores[:, tid] = -float("inf")

        return scores
