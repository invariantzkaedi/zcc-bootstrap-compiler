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

    # 6. Nested-Object & String-Value Key Spoofing Immunity
    print(" [Test 6] Verifying Nested-Object & String-Value Key Spoofing Immunity...")
    spoof_nested_ids = tokenizer.encode('{"alpha": {"beta": 1}', add_special_tokens=False)
    seq_spoof_nested = torch.cat([prompt_ids, torch.tensor([spoof_nested_ids])], dim=1)
    scores_spoof_nested = req_engine(seq_spoof_nested, dummy_scores.clone())
    assert scores_spoof_nested[0, req_engine.brace_close_id].item() == -float("inf"), (
        "Nested key 'beta' inside 'alpha' must NOT satisfy top-level required_keys!"
    )
    print("  ✔ Nested-object key spoofing immunity verified!")

    # 7. RFC 8259 Trailing Comma Prohibition
    print(" [Test 7] Verifying RFC 8259 Trailing Comma Prohibition ('{\"alpha\": 1, ')...")
    comma_ids = tokenizer.encode('{"alpha": 1, ', add_special_tokens=False)
    seq_comma = torch.cat([prompt_ids, torch.tensor([comma_ids])], dim=1)
    scores_comma = engine(seq_comma, dummy_scores.clone())
    assert scores_comma[0, engine.brace_close_id].item() == -float("inf"), (
        "Closing '}' immediately after trailing comma ',' must be blocked!"
    )
    assert scores_comma[0, quote_id].item() == 0.0, "Quote '\"' for next key must be allowed after ','!"
    print("  ✔ RFC 8259 trailing comma prohibition verified!")

    # 8. RFC 8259 Section 7 Raw Unescaped Control Character Shield Inside JSON Strings
    print(" [Test 8] Verifying Raw Control Character Shield ('\\n', '\\r', '\\t') Inside Open Strings...")
    in_str_ids = tokenizer.encode('{"code": "int main() {', add_special_tokens=False)
    seq_in_str = torch.cat([prompt_ids, torch.tensor([in_str_ids])], dim=1)
    scores_in_str = engine(seq_in_str, dummy_scores.clone())
    raw_nl_id = tokenizer.encode("\n", add_special_tokens=False)[0]
    assert scores_in_str[0, raw_nl_id].item() == -float("inf"), (
        "Raw unescaped newline inside JSON string literal must be clamped to -inf!"
    )
    print("  ✔ Raw unescaped control character shield verified!")

    # 9. Bounded Nesting Depth Cap (max_nesting_depth = 64)
    print(" [Test 9] Verifying Bounded Recursion / Nesting Depth Cap (max_nesting_depth=64)...")
    deep_ids = tokenizer.encode("{" + '"a": [' * 65, add_special_tokens=False)
    seq_deep = torch.cat([prompt_ids, torch.tensor([deep_ids])], dim=1)
    scores_deep = engine(seq_deep, dummy_scores.clone())
    assert scores_deep[0, engine.bracket_open_id].item() == -float("inf"), (
        "Further '[' opening at depth >= 64 must be clamped to -inf!"
    )
    assert scores_deep[0, engine.brace_open_id].item() == -float("inf"), (
        "Further '{' opening at depth >= 64 must be clamped to -inf!"
    )
    print("  ✔ Bounded nesting depth cap verified!")

    # 10. Multi-Batch (B=2) Isolation & All-(-inf) Deadlock Recovery Shield
    print(" [Test 10] Verifying Multi-Batch (B=2) Isolation & All-(-inf) Deadlock Recovery Shield...")
    r0_toks = tokenizer.encode('{"a": "x', add_special_tokens=False)
    r1_toks = tokenizer.encode('{"a": 1}', add_special_tokens=False)
    x_tok = tokenizer.encode("x", add_special_tokens=False)[0]
    while len(r0_toks) < len(r1_toks):
        r0_toks.append(x_tok)
    assert len(r0_toks) == len(r1_toks)
    batch_seq = torch.tensor([
        prompt_ids[0].tolist() + r0_toks,
        prompt_ids[0].tolist() + r1_toks,
    ])
    batch_scores = torch.zeros((2, len(tokenizer)))
    out_batch = engine(batch_seq, batch_scores)
    assert out_batch[0, engine.eos_token_id].item() == -float("inf"), "Row 0 (in_string) must block EOS!"
    assert out_batch[1, engine.eos_token_id].item() == 100.0, "Row 1 (closed root) must force EOS!"

    # All-(-inf) deadlock recovery check
    all_neg_inf = torch.full((1, len(tokenizer)), -float("inf"))
    recovered = engine(seq_in_str, all_neg_inf)
    assert torch.isfinite(recovered[0]).any().item() is True, "All-(-inf) row must recover with finite fallback token!"
    assert recovered[0, engine.quote_id].item() == 0.0, "In-string deadlock fallback must unclamp quote token!"
    print("  ✔ Multi-batch (B=2) isolation & deadlock recovery shield verified!")

    print("=" * 88)
    print(" 🔱 VERDICT: HARDENED ZERO-DROP GATEWAY REGRESSION SUITE 10 / 10 PASS 🔱")
    print("=" * 88)
    return 0


if __name__ == "__main__":
    sys.exit(run_regression_suite())

