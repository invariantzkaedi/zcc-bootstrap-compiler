#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
Deterministic Regression Suite for:
  - Idea 6: Autonomous Code Scar-Memory & Crash Immunizer (tools/prime/crash_immunizer.py)
  - Idea 5: Zero-Hallucination Quantitative Diligence Engine (tools/prime/quant_diligence_engine.py)
  - Foundational InfiniMemory fixes (engram_core.py, echo_core.py)
"""

from __future__ import annotations

import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) in sys.path:
    sys.path.remove(str(REPO_ROOT))
sys.path.insert(0, str(REPO_ROOT))
if "zkaedi_prime" in sys.modules and not hasattr(sys.modules["zkaedi_prime"], "__path__"):
    del sys.modules["zkaedi_prime"]

import pytest
from float_guard import CancellationError, exact_sum, guard_sum, ulp_diff
from infinimemory import EchoCore, EngramCore, RecallEngine, Rememory
from scripts.sec_alpha_engine import (
    AmbiguousMetricError,
    EntailmentViolationError,
    ProvenanceError,
    verify_seal,
)
from tools.prime.crash_immunizer import (
    CrashImmunizer,
    compute_ast_signature,
    run_4arm_gauntlet,
)
from tools.prime.quant_diligence_engine import (
    ZeroHallucinationDiligenceEngine,
    run_4arm_diligence_gauntlet,
)


def test_infinimemory_overlapping_parity_shard_recovery() -> None:
    """Verifies that redundant_overlap=True heals content even when 2 of 4 primary shards are destroyed."""
    core = EngramCore(num_parity_shards=4, redundant_overlap=True)
    remem = Rememory()
    original = "def healed_ratio(a, b): return a / b if abs(b) > 1e-12 else 0.0"
    eng = core.encode("vaccine:ratio", original, tags=["vaccine"])
    assert len(eng.parity_shards) == 8

    # Destroy content and corrupt 2 primary shards
    eng.content = "CORRUPTED_PAYLOAD"
    eng.parity_shards[1] = "shard_1:0_10:badhash00000:CORRUPT"
    eng.parity_shards[2] = "shard_2:10_20:badhash11111:CORRUPT"

    assert remem. heal_engram(eng) is True
    assert eng.content == original


def test_infinimemory_echo_core_key_prefix_and_semantic_recall() -> None:
    """Verifies that EchoCore finds echoes by key prefix and RecallEngine semantic search avoids SHA-256 avalanche."""
    core = EngramCore()
    echo_core = EchoCore()
    recall = RecallEngine(similarity_threshold=0.5)

    eng = core.encode(
        "crash:ZeroDivisionError:calc_ratio",
        "ZeroDivisionError in calc_ratio(num, den) when denominator den is zero",
    )
    echo_core.capture_echo(eng, amplitude=0.95)
    hits = echo_core.find_echoes_for_key("crash:ZeroDivisionError")
    assert len(hits) == 1
    assert hits[0].source_engram_id == eng.id

    sem_hits = recall.recall_semantic(
        {eng.id: eng},
        "ZeroDivisionError in calc_ratio(num, den) when denominator den == 0",
    )
    assert len(sem_hits) == 1
    assert sem_hits[0][1] > 0.75


def test_ast_alpha_normalizer_invariance() -> None:
    """Functions with identical AST structure but renamed variables produce the exact same AST hash."""
    def fn_a(alpha: float, beta: float) -> float:
        return (alpha - beta) / alpha

    def fn_b(x_val: float, y_val: float) -> float:
        return (x_val - y_val) / x_val

    _, hash_a = compute_ast_signature(fn_a)
    _, hash_b = compute_ast_signature(fn_b)
    assert hash_a == hash_b


def test_crash_immunizer_4arm_gauntlet() -> None:
    """Executes the full 4-Arm Paired Causal Harness for Idea 6."""
    rep = run_4arm_gauntlet(rounds_per_fault=3)
    assert rep["verdict"] == "PASS"
    assert rep["arms"]["arm1_active"]["recovery_rate_pct"] == 100.0
    assert rep["arms"]["arm1_active"]["ftle_lambda"] < 0.0
    assert rep["arms"]["arm2_zeroed_control"]["recovery_rate_pct"] == 0.0
    assert rep["arms"]["arm3_sham_permuted"]["unsafe_sham_executions"] == 0
    assert rep["arms"]["arm4_stress_corrupted"]["recovery_rate_pct"] == 100.0
    assert rep["arms"]["arm4_stress_corrupted"]["holographic_shard_heals"] == 5


def test_quant_diligence_4arm_gauntlet() -> None:
    """Executes the full 4-Arm Paired Causal Harness for Idea 5."""
    rep = run_4arm_diligence_gauntlet()
    assert rep["verdict"] == "PASS"
    assert rep["arms"]["arm1_active_diligence"]["pass_rate_pct"] == 100.0
    assert rep["arms"]["arm1_active_diligence"]["ulp_error_dist"]["max_linf"] == 0.0
    assert rep["arms"]["arm2_naive_unimmunized"]["exact_segment_matches"] == 0
    assert rep["arms"]["arm3_sham_hallucinated"]["hallucination_escape_count"] == 0
    assert rep["arms"]["arm4_stress_corrupted_unstructured"]["pass_rate_pct"] == 100.0


def test_quant_diligence_rejects_ungrounded_and_tampered_dossiers(tmp_path: Path) -> None:
    """Verifies fail-closed rejection on segment mismatch and post-seal tampering."""
    engine = ZeroHallucinationDiligenceEngine(storage_root=tmp_path)
    res = engine.run_diligence("ACME")
    assert res.payload["critic"]["verdict"] == "pass"
    assert res.segment_ulp_drift_prevented == 8388608
    assert verify_seal(res.json_path) == res.seal_sha256

    # Mismatched segment rollup must raise EntailmentViolationError
    with pytest.raises(EntailmentViolationError):
        engine.run_diligence("ACME", segments=[1_000_000_000.0, 500_000_000.0])

    # Tampering with sealed JSON on disk must raise ProvenanceError
    bad_json = res.json_path.read_text(encoding="utf-8").replace("1750000000.0", "1750000099.0")
    res.json_path.write_text(bad_json, encoding="utf-8")
    with pytest.raises(ProvenanceError):
        verify_seal(res.json_path)


def test_fortify_gap1_inplace_test_vector_deep_copy() -> None:
    """Verifies that functions mutating list/dict args in-place do not corrupt stored test_vectors."""
    imm = CrashImmunizer()

    def broken_scale(arr: list[int]) -> list[int]:
        raise RuntimeError("scale failed")

    healed_src = """
def healed_scale(arr):
    for i in range(len(arr)):
        arr[i] = arr[i] * 2
    return arr
"""
    vecs = [{"args": [[1, 2, 3]], "kwargs": {}, "expected": [2, 4, 6]}]
    imm.register_vaccine(
        fault_key="fault:scale_inplace",
        exc_type=RuntimeError,
        fault_description="in-place list scaling",
        broken_fn=broken_scale,
        healed_source=healed_src,
        healed_fn_name="healed_scale",
        test_vectors=vecs,
    )
    assert vecs[0]["args"][0] == [1, 2, 3]
    out, receipt = imm.execute_immunized("fault:scale_inplace", broken_scale, [1, 2, 3])
    assert out == [2, 4, 6]
    assert receipt is not None


def test_fortify_gap2_ast_sandbox_dunder_while_try_blocked() -> None:
    """Verifies that verify_ast_safety blocks dunder attributes, while loops, try blocks, and eval/getattr."""
    imm = CrashImmunizer()
    unsafe_snippets = [
        "def f(x):\n    return ().__class__.__name__",
        "def f(x):\n    while True:\n        return x",
        "def f(x):\n    try:\n        return x\n    except Exception:\n        return 0",
        "def f(x):\n    return getattr(x, 'real')",
        "def f(x):\n    return __builtins__",
    ]
    for src in unsafe_snippets:
        ok, _ = CrashImmunizer.verify_ast_safety(src)
        assert ok is False
        with pytest.raises(ValueError):
            imm.register_vaccine(
                fault_key="fault:unsafe",
                exc_type=ValueError,
                fault_description="unsafe vaccine",
                broken_fn=lambda x: x,
                healed_source=src,
                healed_fn_name="f",
                test_vectors=[{"args": [1], "kwargs": {}, "expected": 1}],
            )


def test_fortify_gap3_cross_function_identity_guard() -> None:
    """Verifies that an unrelated function raising the same exception type does not trigger a wrong vaccine."""
    imm = CrashImmunizer()

    def divide_pair(a: float, b: float) -> float:
        return a / b

    imm.register_vaccine(
        fault_key="fault:divide_pair",
        exc_type=ZeroDivisionError,
        fault_description="2-arg divide",
        broken_fn=divide_pair,
        healed_source="def healed_div(a, b):\n    return 0.0",
        healed_fn_name="healed_div",
        test_vectors=[{"args": [10.0, 0.0], "kwargs": {}, "expected": 0.0}],
    )

    def unrelated_triple_fn(x: float, y: float, z: float) -> float:
        return (x + y) / z

    with pytest.raises(ZeroDivisionError):
        imm.execute_immunized("fault:unrelated_triple", unrelated_triple_fn, 1.0, 2.0, 0.0)


def test_fortify_gap4_tail_and_multi_shard_holographic_recovery() -> None:
    """Verifies recovery when tail shard_3 + parity_3 are corrupted, or when all 4 primary shards are destroyed."""
    core = EngramCore(num_parity_shards=4, redundant_overlap=True)
    remem = Rememory()
    text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_HOLOGRAPHIC_RESILIENCE"

    # Case A: Tail shard_3 + parity_3 corrupted
    eng1 = core.encode("k1", text)
    eng1.content = "BROKEN"
    eng1.parity_shards[3] = "shard_3:0_5:bad:X"
    eng1.parity_shards[7] = "parity_3:0_5:bad:X"
    assert remem.heal_engram(eng1) is True
    assert eng1.content == text

    # Case B: All 4 primary shards (shard_0..shard_3) + parity_1 destroyed (5 of 8 shards lost!)
    eng2 = core.encode("k2", text)
    eng2.content = "BROKEN"
    for i in range(4):
        eng2.parity_shards[i] = f"shard_{i}:0_5:bad:X"
    eng2.parity_shards[5] = "parity_1:0_5:bad:X"
    assert remem.heal_engram(eng2) is True
    assert eng2.content == text


def test_fortify_gap5_unstructured_ambiguous_metric_rejected(tmp_path: Path) -> None:
    """Verifies that conflicting metric figures in unstructured prose raise AmbiguousMetricError."""
    engine = ZeroHallucinationDiligenceEngine(storage_root=tmp_path)
    conflicting_prose = (
        "Acme Cloud Systems Corp. Annual Shareholder Narrative FY25.\n"
        "Span 1: During 2024 revenue stood at $1,250,000,000 serving 4,200 enterprise accounts.\n"
        "Span 2: In 2025 total revenue surged to $1,750,000,000 with Net Retention of 124.5%.\n"
        "Span 3: However in another paragraph 2025 total revenue was $999,000,000.\n"
        "Span 4: Gross Margin expanded to 79.0% while Operating Margin reached 24.0%.\n"
    )
    with pytest.raises(AmbiguousMetricError):
        engine.run_diligence("ACME", custom_text=conflicting_prose, segments=[1_750_000_000.0])


def test_fortify_gap6_and_i7_duplicate_keys_nan_and_accounting_bounds(tmp_path: Path) -> None:
    """Verifies rejection of duplicate JSON keys, NaN/Infinity constants, and impossible >100% gross margins."""
    engine = ZeroHallucinationDiligenceEngine(storage_root=tmp_path)
    dup_json = '{"ticker":"SNOW","revenue_prior":100,"revenue_current":200,"revenue_current":999,"gross_margin":75.0,"citations":{}}'
    nan_json = '{"ticker":"SNOW","revenue_prior":100,"revenue_current":NaN,"gross_margin":75.0,"citations":{}}'
    assert engine.verify_squozen_json_envelope(dup_json, engine.REQUIRED_EXTRACTION_KEYS) is False
    assert engine.verify_squozen_json_envelope(nan_json, engine.REQUIRED_EXTRACTION_KEYS) is False

    impossible_margin_prose = (
        "Acme Cloud Systems Corp. Annual Shareholder Narrative FY25.\n"
        "Span 1: During 2024 revenue stood at $1,250,000,000 serving 4,200 enterprise accounts.\n"
        "Span 2: In 2025 total revenue surged to $1,750,000,000 with Net Retention of 124.5%.\n"
        "Span 3: Gross Margin expanded to 185.0% while Operating Margin reached 24.0%.\n"
    )
    with pytest.raises(EntailmentViolationError):
        engine.run_diligence("ACME", custom_text=impossible_margin_prose, segments=[1_750_000_000.0])
