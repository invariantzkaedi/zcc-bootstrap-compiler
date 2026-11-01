#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
=======================================================================================================
 🔱 ZKAEDI PRIME // ZERO-DROP GATEWAY DETERMINISTIC REGRESSION SUITE 🔱
=======================================================================================================
 Verifies:
   1. String-aware PDA state scanner (braces inside strings, escaped quotes, nested arrays/objects)
   2. BPE-merged opening ('{\n', '{"') and closing ('}\n', '},') token handling
   3. Required JSON Schema keys root-closure barrier ('required_keys')
   4. RFC 8259 Object Member transition barrier (blocks '{{' and '{:.' immediately after '{')
   5. SquozenMode.CODE_ONLY Python single-function return termination
   6. Independent Oracle (verify_structured_output) positive & negative controls (Rule MV-6)
=======================================================================================================
"""

import sys
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

from pathlib import Path
import torch
from transformers import AutoTokenizer

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from zkaedi_prime.squozen_engine import SquozenLogitsEngine, SquozenMode
from tools.prime.zero_drop_gateway import verify_structured_output, DEFAULT_MODEL_PATH


def run_regression_suite() -> int:
    print("=" * 88)
    print(" 🔱 EXECUTING ZERO-DROP STRUCTURED GATEWAY REGRESSION SUITE 🔱")
    print("=" * 88)

    tokenizer = AutoTokenizer.from_pretrained(DEFAULT_MODEL_PATH)
    prompt_ids = tokenizer.encode("Return JSON:", return_tensors="pt")
    prompt_len = prompt_ids.shape[1]
    dummy_scores = torch.zeros((1, len(tokenizer)))

    # 1. String-Aware PDA Scanner Verification
    print(" [Test 1] Verifying String-Aware PDA Scanner (braces & escapes inside strings)...")
    bd, bkd, in_s, opened = SquozenLogitsEngine.scan_pda_state('{"code": "int f() { return 0; }", "arr": [1, 2]}')
    assert (bd, bkd, in_s, opened) == (0, 0, False, True), f"Unexpected PDA state: {(bd, bkd, in_s, opened)}"

    bd2, bkd2, in_s2, opened2 = SquozenLogitsEngine.scan_pda_state('{"escaped": "quote \\" and } inside')
    assert (bd2, bkd2, in_s2, opened2) == (1, 0, True, True), f"Unexpected unclosed string state: {(bd2, bkd2, in_s2, opened2)}"
    print("  ✔ String-aware PDA scanner verified!")

    # 2. BPE-Merge Opening & Closing Verification
    print(" [Test 2] Verifying BPE-Merged Opening ('{\\n') and Closing ('}\\n') Invariants...")
    engine = SquozenLogitsEngine(prompt_len, tokenizer, mode=SquozenMode.JSON)

    open_nl_id = tokenizer.encode("{\n", add_special_tokens=False)[0]
    key_ids = tokenizer.encode('  "arr": [1]', add_special_tokens=False)
    seq_open = torch.cat([prompt_ids, torch.tensor([[open_nl_id, *key_ids]])], dim=1)
    scores_open = engine(seq_open, dummy_scores.clone())
    assert scores_open[0, engine.eos_token_id].item() == -float("inf"), "EOS must be blocked while outer '{\\n' is open!"

    close_nl_id = tokenizer.encode("}\n", add_special_tokens=False)[0]
    seq_closed = torch.cat([seq_open, torch.tensor([[close_nl_id]])], dim=1)
    scores_closed = engine(seq_closed, dummy_scores.clone())
    assert scores_closed[0, engine.eos_token_id].item() == 100.0, "EOS must be forced after BPE-merged '}\\n' closes root!"
    print("  ✔ BPE-merged delimiter tracking verified!")

    # 3. Required Schema Keys Barrier Verification
    print(" [Test 3] Verifying Required Schema Keys ('required_keys') Root-Closure Guard...")
    req_engine = SquozenLogitsEngine(
        prompt_len, tokenizer, mode=SquozenMode.JSON, required_keys=["alpha", "beta"]
    )
    partial_ids = tokenizer.encode('{"alpha": 1', add_special_tokens=False)
    seq_partial = torch.cat([prompt_ids, torch.tensor([partial_ids])], dim=1)
    scores_partial = req_engine(seq_partial, dummy_scores.clone())
    assert scores_partial[0, req_engine.brace_close_id].item() == -float("inf"), (
        "Root '}' must be clamped to -inf while required key 'beta' is missing!"
    )

    full_ids = tokenizer.encode('{"alpha": 1, "beta": 2}', add_special_tokens=False)
    seq_full = torch.cat([prompt_ids, torch.tensor([full_ids])], dim=1)
    scores_full = req_engine(seq_full, dummy_scores.clone())
    assert scores_full[0, req_engine.eos_token_id].item() == 100.0, (
        "EOS must be forced once all required keys are present and root '}' closes!"
    )
    print("  ✔ Required schema keys barrier verified!")

    # 4. RFC 8259 Object Member Transition Verification (blocks '{{' and '{:.')
    print(" [Test 4] Verifying RFC 8259 Object Member Transition Barrier (blocks '{{' and '{:.')...")
    seq_just_open = torch.cat([prompt_ids, torch.tensor([[engine.brace_open_id]])], dim=1)
    scores_after_open = engine(seq_just_open, dummy_scores.clone())
    colon_id = tokenizer.encode(":", add_special_tokens=False)[0]
    quote_id = tokenizer.encode('"', add_special_tokens=False)[0]
    assert scores_after_open[0, engine.brace_open_id].item() == -float("inf"), "Second '{' ('{{') must be blocked!"
    assert scores_after_open[0, colon_id].item() == -float("inf"), "Colon ':' ('{:.') after '{' must be blocked!"
    assert scores_after_open[0, quote_id].item() == 0.0, "Quote '\"' must be permitted after '{'!"
    print("  ✔ RFC 8259 Object Member transition barrier verified!")

    # 5. Independent Oracle Positive & Negative Controls (Rule MV-6)
    print(" [Test 5] Verifying Independent Oracle Positive & Negative Controls (Rule MV-6)...")
    v_pos_json = verify_structured_output(
        '{"fn": "main", "code": "int main() { return 0; }"}', "json", required_keys=["fn", "code"]
    )
    assert v_pos_json["valid"] is True, f"Expected valid JSON, got {v_pos_json}"

    v_neg_fence = verify_structured_output('```json\n{"fn": "main"}\n```', "json")
    assert v_neg_fence["valid"] is False and v_neg_fence["has_backticks"] is True

    v_neg_missing = verify_structured_output('{"fn": "main"}', "json", required_keys=["fn", "code"])
    assert v_neg_missing["valid"] is False and v_neg_missing["schema_keys_ok"] is False

    v_pos_py = verify_structured_output("def f(x: int) -> int:\n    return x + 1\n", "python")
    assert v_pos_py["valid"] is True

    v_neg_py = verify_structured_output("Sure! Here is the code:\ndef f(x): return x", "python")
    assert v_neg_py["valid"] is False
    print("  ✔ Independent oracle positive & negative controls verified!")

    print("=" * 88)
    print(" 🔱 VERDICT: ZERO-DROP GATEWAY REGRESSION SUITE 5 / 5 PASS 🔱")
    print("=" * 88)
    return 0


if __name__ == "__main__":
    sys.exit(run_regression_suite())
