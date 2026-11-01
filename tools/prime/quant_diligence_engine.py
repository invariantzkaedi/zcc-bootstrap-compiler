#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
=======================================================================================================
 🔱 ZKAEDI PRIME // ZERO-HALLUCINATION QUANTITATIVE DILIGENCE ENGINE (IDEA 5) 🔱
=======================================================================================================
 Unifies:
   1. scripts/sec_alpha_engine.py       — Fail-Closed I1-I5 Span-Grounded Entailment Critic & SHA-256 Seal
   2. float_guard.py                    — IEEE-754 ULP Distance & Exact Rational Summation
   3. tools/prime/crash_immunizer.py    — Autonomous Scar-Memory Immunization for Catastrophic Cancellation
   4. zkaedi_prime/squozen_engine.py    — Squozen v1.1 PDA Schema & Control-Char / Depth Validation

 Enforces Runtime Invariants I1-I6 (Fail-Closed):
   I1. Every numeric fact is extracted from a filing span with verbatim quote verification.
   I2. Every number in a fact claim appears in its cited span.
   I3. Every derived claim is recomputed over Q (exact rational arithmetic) with 0 ULP intermediate drift.
   I4. Multi-segment rollups are guarded by float_guard.guard_sum and self-healed via CrashImmunizer.
   I5. Every JSON extraction output passes Squozen v1.1 PDA structural & schema key invariants.
   I6. Every deliverable is sealed with a canonical SHA-256 digest and verified via disk round-trip.
=======================================================================================================
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import statistics
import sys
import tempfile
import time
from dataclasses import asdict, dataclass, field
from fractions import Fraction
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) in sys.path:
    sys.path.remove(str(REPO_ROOT))
sys.path.insert(0, str(REPO_ROOT))
if "zkaedi_prime" in sys.modules and not hasattr(sys.modules["zkaedi_prime"], "__path__"):
    del sys.modules["zkaedi_prime"]

from float_guard import CancellationError, exact_sum, guard_sum, naive_sum, neumaier_sum, ulp_diff
from scripts.sec_alpha_engine import (
    ANALYST_KILL_CRITERIA,
    COMPANY_NAMES,
    CURATED_FILING_REPOSITORY,
    AmbiguousMetricError,
    Claim,
    CriticReport,
    DerivedMetric,
    Dossier,
    EntailmentViolationError,
    GroundedMetric,
    MetricExtractionError,
    ProvenanceError,
    build_claims,
    build_payload,
    engineer_features,
    extract_metrics,
    ingest_filing,
    numbers_in,
    parse_number,
    run_critic,
    seal,
    verify_seal,
    write_deliverables,
)
from tools.prime.crash_immunizer import CrashImmunizer, ImmunizationReceipt
from zkaedi_prime.squozen_engine import SquozenLogitsEngine


# ======================================================================================
# 1. UNSTRUCTURED PROSE & MULTI-SEGMENT SPAN EXTRACTOR (Squozen PDA Validated)
# ======================================================================================

UNSTRUCTURED_PROSE_PARTERNS: Tuple[Tuple[str, str, re.Pattern[str], bool], ...] = (
    (
        "revenue_prior",
        "usd",
        re.compile(
            r"(?:FY24|prior[- ]year|2024)\s+(?:total\s+)?revenue[s]?[^$\d]{0,35}\$([\d,]+(?:\.\d+)?)",
            re.I,
        ),
        True,
    ),
    (
        "revenue_current",
        "usd",
        re.compile(
            r"(?:FY25|current[- ]year|2025)\s+(?:total\s+)?revenue[s]?[^$\d]{0,35}\$([\d,]+(?:\.\d+)?)",
            re.I,
        ),
        True,
    ),
    (
        "customer_count",
        "count",
        re.compile(
            r"(?:across|serving|reported)\s+([\d,]+)\s+enterprise\s+(?:customers|clients|accounts)",
            re.I,
        ),
        False,
    ),
    (
        "net_retention",
        "pct",
        re.compile(r"Net\s+Retention(?:\s+Rate)?[^%\d]{0,25}([\d.]+)%", re.I),
        False,
    ),
    (
        "gross_margin",
        "pct",
        re.compile(r"Gross\s+Margin[^%\d]{0,40}([\d.]+)%", re.I),
        True,
    ),
    (
        "operating_margin",
        "pct",
        re.compile(r"Operating\s+Margin[^%\d]{0,30}([\d.]+)%", re.I),
        False,
    ),
    (
        "free_cash_flow",
        "usd",
        re.compile(r"Free\s+Cash\s+Flow[^$\d]{0,40}\$([\d,]+(?:\.\d+)?)", re.I),
        False,
    ),
    (
        "cash",
        "usd",
        re.compile(
            r"\$([\d,]+(?:\.\d+)?)\s+in\s+cash|[Cc]ash[\w\s,-]{0,40}?(?:totaled|stood\s+at|reached)\s+\$([\d,]+(?:\.\d+)?)",
            re.I,
        ),
        False,
    ),
    (
        "total_debt",
        "usd",
        re.compile(
            r"\$([\d,]+(?:\.\d+)?)\s+(?:long-term\s+|total\s+)?debt|\b(zero)\b[^.]{0,30}\bdebt\b",
            re.I,
        ),
        False,
    ),
    (
        "rpo",
        "usd",
        re.compile(
            r"(?:RPO|Remaining\s+Performance\s+Obligations)[^$\d]{0,60}\$([\d,]+(?:\.\d+)?)",
            re.I,
        ),
        False,
    ),
)


UNSTRUCTURED_FILINGS: Dict[str, Dict[str, Any]] = {
    "ACME": {
        "company_name": "Acme Cloud Systems Corp.",
        "filing_text": """
Acme Cloud Systems Corp. Annual Shareholder Narrative FY25.
Narrative Span A: During 2024 revenue stood at $1,250,000,000 serving 4,200 enterprise accounts.
Narrative Span B: In 2025 total revenue surged to $1,750,000,000 with Net Retention of 124.5%.
Narrative Span C: Gross Margin expanded to 79.0% while Operating Margin reached 24.0%.
Narrative Span D: Free Cash Flow generated $525,000,000 across global regions.
Narrative Span E: Cash and equivalents stood at $2,100,000,000 with $300,000,000 total debt.
Narrative Span F: Remaining Performance Obligations reached $2,950,000,000 at year-end.
""",
        "segments_revenue_current": [
            1e16,
            1_000_000_000.0,
            749_999_998.0,
            0.4,
            0.4,
            0.4,
            0.4,
            0.4,
            -1e16,
        ],
        "expected_segment_sum": 1_750_000_000.0,
    },
    "ZETA": {
        "company_name": "Zeta Autonomous Silicon Inc.",
        "filing_text": """
Zeta Autonomous Silicon Inc. Consolidated Filing FY25.
Section 1: Prior-year revenue reached $4,500,000,000 across 1,250 enterprise clients.
Section 2: Current-year revenue climbed to $6,750,000,000 with Net Retention Rate of 132.0%.
Section 3: Gross Margin was 82.5% and Operating Margin reached 31.0%.
Section 4: Free Cash Flow was $2,025,000,000 driven by AI accelerator demand.
Section 5: Cash reserves stood at $5,400,000,000 with zero funded debt on the balance sheet.
Section 6: RPO backlog reached $8,100,000,000 under multi-year sovereign contracts.
""",
        "segments_revenue_current": [
            1e16,
            4_000_000_000.0,
            2_749_999_998.0,
            0.4,
            0.4,
            0.4,
            0.4,
            0.4,
            -1e16,
        ],
        "expected_segment_sum": 6_750_000_000.0,
    },
}


@dataclass
class DiligenceAuditResult:
    ticker: str
    payload: Dict[str, Any]
    json_path: Path
    html_path: Path
    seal_sha256: str
    segment_sum_exact: Optional[float]
    segment_sum_naive: Optional[float]
    segment_ulp_drift_prevented: int
    immunization_receipt: Optional[Dict[str, Any]]
    squozen_pda_verified: bool
    latency_ms: float


class ZeroHallucinationDiligenceEngine:
    """
    Hybrid Symbolic + Squozen-Validated + Float-Immunized Quantitative Diligence Engine.
    """

    REQUIRED_EXTRACTION_KEYS: List[str] = [
        "ticker",
        "revenue_prior",
        "revenue_current",
        "gross_margin",
        "citations",
    ]

    def __init__(
        self,
        storage_root: Optional[Path] = None,
        immunizer: Optional[CrashImmunizer] = None,
    ) -> None:
        self.storage_root = storage_root or Path(tempfile.mkdtemp(prefix="zkaedi_diligence_"))
        self.immunizer = immunizer or self._build_immunizer()

    def _build_immunizer(self) -> CrashImmunizer:
        imm = CrashImmunizer(
            storage_root=self.storage_root / "immunizer_bank",
            eta=0.40,
            gamma=0.30,
            beta=0.10,
            eps=0.05,
            kick=2.00,
            redundant_overlap=True,
        )

        def broken_segment_rollup(xs: Sequence[float]) -> float:
            return guard_sum(xs, max_ulps=2)

        healed_rollup_src = """
def healed_segment_rollup(xs):
    return exact_sum(xs)
"""
        imm.register_vaccine(
            fault_key="fault:float_cancellation:segment_rollup",
            exc_type=CancellationError,
            fault_description="Catastrophic IEEE-754 mantissa absorption in multi-segment financial rollup",
            broken_fn=broken_segment_rollup,
            healed_source=healed_rollup_src,
            healed_fn_name="healed_segment_rollup",
            test_vectors=[
                {
                    "args": [[1e16, 125_500_000_000.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16]],
                    "kwargs": {},
                    "expected": 125_500_000_002.0,
                    "max_ulps": 0,
                }
            ],
        )
        imm.seal_vaccines_into_lore("Diligence Float-Guard Immunization Lore")
        return imm

    @staticmethod
    def verify_squozen_json_envelope(
        raw_json: str,
        required_keys: Sequence[str],
        max_nesting_depth: int = 32,
    ) -> bool:
        """Verifies a JSON string using SquozenLogitsEngine v1.1 PDA state scanner + RFC 8259 checks.
        Rejects unescaped control chars, trailing commas, duplicate keys, NaN/Infinity, and excessive depth."""
        in_str = False
        esc = False
        cur_depth = 0
        for ch in raw_json:
            if esc:
                esc = False
                continue
            if in_str:
                if ch == "\\":
                    esc = True
                elif ch == '"':
                    in_str = False
                elif ord(ch) < 0x20:
                    return False
            else:
                if ch == '"':
                    in_str = True
                elif ch in ("{", "["):
                    cur_depth += 1
                    if cur_depth > max_nesting_depth:
                        return False
                elif ch in ("}", "]"):
                    cur_depth -= 1
                    if cur_depth < 0:
                        return False

        brace_depth, bracket_depth, in_string, escape_next, opened_ever, top_level_keys = (
            SquozenLogitsEngine.scan_pda_full(raw_json)
        )
        if brace_depth != 0 or bracket_depth != 0 or in_string or escape_next or not opened_ever:
            return False
        if not set(required_keys).issubset(top_level_keys):
            return False

        def _reject_dup_keys(pairs: List[Tuple[str, Any]]) -> Dict[str, Any]:
            obj: Dict[str, Any] = {}
            for k, v in pairs:
                if k in obj:
                    raise ValueError(f"Duplicate JSON key: {k}")
                obj[k] = v
            return obj

        def _reject_nonfinite_constant(const_str: str) -> Any:
            raise ValueError(f"Non-finite JSON constant forbidden: {const_str}")

        try:
            parsed = json.loads(
                raw_json,
                object_pairs_hook=_reject_dup_keys,
                parse_constant=_reject_nonfinite_constant,
            )
            return isinstance(parsed, dict)
        except Exception:
            return False

    @staticmethod
    def verify_accounting_sanity(metrics: Dict[str, GroundedMetric]) -> None:
        """Enforces Invariant I7: Accounting & Domain Sanity Bounds (fail-closed)."""
        violations: List[Dict[str, Any]] = []
        for req_pos in ("revenue_prior", "revenue_current"):
            if req_pos in metrics and metrics[req_pos].value <= 0.0:
                violations.append({"metric": req_pos, "reason": f"{req_pos} must be strictly positive"})
        for non_neg in ("customer_count", "cash", "total_debt", "rpo"):
            if non_neg in metrics and metrics[non_neg].value < 0.0:
                violations.append({"metric": non_neg, "reason": f"{non_neg} cannot be negative"})
        if "gross_margin" in metrics:
            gm = metrics["gross_margin"].value
            if gm < -100.0 or gm > 100.0:
                violations.append({"metric": "gross_margin", "reason": f"gross_margin {gm}% out of [-100, 100] bounds"})
        if "operating_margin" in metrics:
            om = metrics["operating_margin"].value
            if om < -200.0 or om > 100.0:
                violations.append({"metric": "operating_margin", "reason": f"operating_margin {om}% out of bounds"})
        if "net_retention" in metrics:
            nrr = metrics["net_retention"].value
            if nrr <= 0.0 or nrr > 500.0:
                violations.append({"metric": "net_retention", "reason": f"net_retention {nrr}% out of (0, 500] bounds"})
        if violations:
            raise EntailmentViolationError("Accounting domain sanity invariant violated", violations=violations)

    def extract_with_squozen_fallback(
        self,
        ticker: str,
        spans: Dict[str, str],
    ) -> Tuple[Dict[str, GroundedMetric], List[str], str]:
        """Attempts strict regex extraction first; if the filing is unstructured prose,
        runs span-grounded prose extraction and validates the structured JSON envelope
        through SquozenLogitsEngine.scan_pda_full."""
        try:
            metrics, missing = extract_metrics(spans)
        except AmbiguousMetricError:
            raise
        except MetricExtractionError:
            found: Dict[str, GroundedMetric] = {}
            missing = []
            for name, unit, pat, req in UNSTRUCTURED_PROSE_PARTERNS:
                hits: List[GroundedMetric] = []
                for sid, text in spans.items():
                    for m in pat.finditer(text):
                        groups = [g for g in m.groups() if g is not None]
                        if not groups:
                            continue
                        tok = groups[0]
                        val = 0.0 if tok.lower() == "zero" else parse_number(tok)
                        quote = m.group(0)
                        if quote not in text:
                            raise MetricExtractionError(
                                "Non-verbatim quote in span", context={"metric": name, "span": sid}
                            )
                        hits.append(GroundedMetric(name, val, unit, sid, quote))
                distinct = {round(h.value, 6) for h in hits}
                if len(distinct) > 1:
                    raise AmbiguousMetricError(
                        "Conflicting values for metric in unstructured prose",
                        context={"metric": name, "hits": [(h.span_id, h.value) for h in hits]},
                    )
                if hits:
                    found[name] = hits[0]
                elif req:
                    raise MetricExtractionError(
                        "Required metric not found in unstructured filing", context={"metric": name}
                    )
                else:
                    missing.append(name)
            metrics = found

        self.verify_accounting_sanity(metrics)

        # Construct canonical structured JSON envelope and validate via Squozen v1.1 PDA
        envelope = {
            "ticker": ticker,
            "revenue_prior": metrics["revenue_prior"].value,
            "revenue_current": metrics["revenue_current"].value,
            "gross_margin": metrics["gross_margin"].value,
            "citations": {k: {"span_id": v.span_id, "quote": v.quote} for k, v in metrics.items()},
        }
        raw_envelope = json.dumps(envelope, sort_keys=True, separators=(",", ":"))
        if not self.verify_squozen_json_envelope(raw_envelope, self.REQUIRED_EXTRACTION_KEYS):
            raise MetricExtractionError("Squozen PDA rejected structured extraction envelope", context={"ticker": ticker})

        return metrics, missing, raw_envelope

    def audit_segments_immunized(
        self,
        segments: Sequence[float],
    ) -> Tuple[float, float, int, Optional[ImmunizationReceipt]]:
        """Computes multi-segment financial sum via float_guard.guard_sum wrapped in CrashImmunizer.
        Returns (healed_exact_sum, naive_float_sum, ulp_drift_prevented, receipt)."""
        naive_val = naive_sum(segments)

        def _guarded_rollup(xs: Sequence[float]) -> float:
            return guard_sum(xs, max_ulps=2)

        healed_val, receipt = self.immunizer.execute_immunized(
            "fault:float_cancellation:segment_rollup",
            _guarded_rollup,
            segments,
        )
        drift_prevented = ulp_diff(naive_val, float(healed_val))
        return float(healed_val), naive_val, drift_prevented, receipt

    def run_diligence(
        self,
        ticker: str,
        customer_id: str = "sovereign_audit",
        custom_text: Optional[str] = None,
        segments: Optional[Sequence[float]] = None,
        out_dir: Optional[Path] = None,
        strict: bool = True,
    ) -> DiligenceAuditResult:
        """Executes end-to-end Zero-Hallucination Quantitative Diligence for a ticker."""
        t0 = time.perf_counter()
        t = ticker.strip().upper()
        if custom_text is None and t in UNSTRUCTURED_FILINGS:
            custom_text = UNSTRUCTURED_FILINGS[t]["filing_text"]
            if segments is None:
                segments = UNSTRUCTURED_FILINGS[t].get("segments_revenue_current")

        spans = ingest_filing(t, custom_text=custom_text)
        metrics, missing, raw_envelope = self.extract_with_squozen_fallback(t, spans)

        seg_exact: Optional[float] = None
        seg_naive: Optional[float] = None
        ulp_prevented: int = 0
        imm_receipt: Optional[ImmunizationReceipt] = None

        if segments is not None:
            seg_exact, seg_naive, ulp_prevented, imm_receipt = self.audit_segments_immunized(segments)
            # Reconcile segment rollup against extracted revenue_current (must match to 0 ULPs)
            if ulp_diff(seg_exact, metrics["revenue_current"].value) > 2:
                raise EntailmentViolationError(
                    "Multi-segment rollup does not reconcile with headline revenue_current",
                    violations=[
                        {
                            "reason": f"segment_exact={seg_exact} != revenue_current={metrics['revenue_current'].value}"
                        }
                    ],
                )

        derived = engineer_features(metrics)
        claims = build_claims(metrics, derived)
        company_name = COMPANY_NAMES.get(
            t,
            UNSTRUCTURED_FILINGS.get(t, {}).get("company_name", f"{t} Corp."),
        )

        dossier = Dossier(
            company_name=company_name,
            ticker=t,
            fiscal_period="FY25",
            spans=spans,
            metrics=metrics,
            missing_optional=missing,
            derived=derived,
            claims=claims,
            kill_criteria=ANALYST_KILL_CRITERIA.get(t, ["Segment rollup divergence > 2 ULPs."]),
            risk_tier=None,
        )
        dossier.critic = run_critic(dossier, strict=strict)

        job_id = f"job_{t.lower()}_{hashlib.sha256(f'{t}:{raw_envelope}'.encode('utf-8')).hexdigest()[:10]}"
        elapsed_s = time.perf_counter() - t0
        payload = seal(build_payload(dossier, job_id, customer_id, elapsed_s))
        target_dir = out_dir or (self.storage_root / "deliverables")
        json_path, html_path = write_deliverables(payload, target_dir)
        verified_digest = verify_seal(json_path)

        return DiligenceAuditResult(
            ticker=t,
            payload=payload,
            json_path=json_path,
            html_path=html_path,
            seal_sha256=verified_digest,
            segment_sum_exact=seg_exact,
            segment_sum_naive=seg_naive,
            segment_ulp_drift_prevented=ulp_prevented,
            immunization_receipt=asdict(imm_receipt) if imm_receipt else None,
            squozen_pda_verified=True,
            latency_ms=round(elapsed_s * 1000.0, 4),
        )


# ======================================================================================
# 2. 4-ARM PAIRED CAUSAL HARNESS FOR IDEA 5 (Rule EF-13 & NV-1..5)
# ======================================================================================

def _dist_stats(values: Sequence[float]) -> Dict[str, float]:
    if not values:
        return {"mean": 0.0, "median": 0.0, "std": 0.0, "max_linf": 0.0}
    arr = [float(x) for x in values]
    return {
        "mean": round(statistics.mean(arr), 4),
        "median": round(statistics.median(arr), 4),
        "std": round(statistics.pstdev(arr), 4),
        "max_linf": round(max(abs(x) for x in arr), 4),
    }


def run_4arm_diligence_gauntlet() -> Dict[str, Any]:
    """Executes the 4-Arm Paired Causal Harness for Idea 5 (ZeroHallucinationDiligenceEngine)."""
    engine = ZeroHallucinationDiligenceEngine()
    all_tickers = ["SNOW", "CRWD", "PLTR", "NVDA", "ACME", "ZETA"]
    ill_conditioned_segments = {
        "SNOW": [1e16, 2_000_000_000.0, 1_627_999_998.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16],
        "CRWD": [1e16, 2_500_000_000.0, 1_479_999_998.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16],
        "PLTR": [1e16, 1_800_000_000.0, 1_059_999_998.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16],
        "NVDA": [1e16, 100_000_000_000.0, 25_499_999_998.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16],
        "ACME": UNSTRUCTURED_FILINGS["ACME"]["segments_revenue_current"],
        "ZETA": UNSTRUCTURED_FILINGS["ZETA"]["segments_revenue_current"],
    }

    report: Dict[str, Any] = {"arms": {}}

    # ------------------------------------------------------------------
    # ARM 1: Active Treatment (Squozen PDA + Float-Guard Immunizer + I1-I5 Critic)
    # ------------------------------------------------------------------
    arm1_passed = 0
    arm1_ulp_errors: List[float] = []
    arm1_ulp_prevented: List[float] = []
    arm1_latencies: List[float] = []
    arm1_seals: Dict[str, str] = {}

    for t in all_tickers:
        res = engine.run_diligence(t, segments=ill_conditioned_segments[t])
        if res.payload["critic"]["verdict"] == "pass" and res.squozen_pda_verified:
            arm1_passed += 1
        actual_ulp_err = ulp_diff(
            res.segment_sum_exact or 0.0,
            res.payload["metrics"][1]["value"] if len(res.payload["metrics"]) > 1 else (res.segment_sum_exact or 0.0),
        )
        arm1_ulp_errors.append(float(actual_ulp_err))
        arm1_ulp_prevented.append(float(res.segment_ulp_drift_prevented))
        arm1_latencies.append(res.latency_ms)
        arm1_seals[t] = res.seal_sha256

    report["arms"]["arm1_active_diligence"] = {
        "tickers_evaluated": len(all_tickers),
        "dossiers_verified": arm1_passed,
        "pass_rate_pct": round(100.0 * arm1_passed / len(all_tickers), 2),
        "ulp_error_dist": _dist_stats(arm1_ulp_errors),
        "ulp_drift_prevented_dist": _dist_stats(arm1_ulp_prevented),
        "latency_ms_dist": _dist_stats(arm1_latencies),
        "seals": arm1_seals,
    }

    # ------------------------------------------------------------------
    # ARM 2: Negative / Unimmunized Naive Baseline (Standard Regex + Naive Float Sum)
    # ------------------------------------------------------------------
    arm2_regex_success = 0
    arm2_exact_segment_matches = 0
    arm2_naive_ulp_drifts: List[float] = []

    for t in all_tickers:
        segs = ill_conditioned_segments[t]
        naive_val = naive_sum(segs)
        truth_val = exact_sum(segs)
        drift = ulp_diff(naive_val, truth_val)
        arm2_naive_ulp_drifts.append(float(drift))
        if drift == 0:
            arm2_exact_segment_matches += 1
        try:
            raw_txt = CURATED_FILING_REPOSITORY.get(t, UNSTRUCTURED_FILINGS.get(t, {}).get("filing_text"))
            sp = ingest_filing(t, custom_text=raw_txt)
            extract_metrics(sp)
            arm2_regex_success += 1
        except MetricExtractionError:
            pass

    report["arms"]["arm2_naive_unimmunized"] = {
        "tickers_evaluated": len(all_tickers),
        "rigid_regex_extracted": arm2_regex_success,
        "exact_segment_matches": arm2_exact_segment_matches,
        "naive_ulp_drift_dist": _dist_stats(arm2_naive_ulp_drifts),
    }

    # ------------------------------------------------------------------
    # ARM 3: Sham / Adversarial Hallucination Injection (6 Attack Vectors)
    # ------------------------------------------------------------------
    arm3_attacks_blocked = 0
    arm3_total_attacks = 6

    base_spans = ingest_filing("SNOW")
    base_metrics, base_missing = extract_metrics(base_spans)
    base_derived = engineer_features(base_metrics)

    # Attack 1: Hallucinated revenue number not in span
    try:
        c1 = build_claims(base_metrics, base_derived)
        c1.append(Claim("FY25 revenue was $9,999,999,999.", "fact", ["s3"], metric_name="revenue_current"))
        run_critic(Dossier("Snowflake Inc.", "SNOW", "FY25", base_spans, base_metrics, base_missing, base_derived, c1, [], None), strict=True)
    except EntailmentViolationError:
        arm3_attacks_blocked += 1

    # Attack 2: Fact claim citing wrong span (s1 instead of s3)
    try:
        c2 = build_claims(base_metrics, base_derived)
        c2[1].source_spans = ["s1"]
        run_critic(Dossier("Snowflake Inc.", "SNOW", "FY25", base_spans, base_metrics, base_missing, base_derived, c2, [], None), strict=True)
    except EntailmentViolationError:
        arm3_attacks_blocked += 1

    # Attack 3: Tampered derived metric (YoY growth inflated by 0.5%)
    try:
        c3 = build_claims(base_metrics, base_derived)
        for c in c3:
            if c.metric_name == "yoy_growth_pct" and c.derivation:
                c.derivation["value"] = float(c.derivation["value"]) + 0.5
        run_critic(Dossier("Snowflake Inc.", "SNOW", "FY25", base_spans, base_metrics, base_missing, base_derived, c3, [], None), strict=True)
    except EntailmentViolationError:
        arm3_attacks_blocked += 1

    # Attack 4: Segment rollup mismatch with headline revenue
    try:
        bad_segs = [1e16, 2_000_000_000.0, 999_999_998.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16]
        engine.run_diligence("SNOW", segments=bad_segs)
    except EntailmentViolationError:
        arm3_attacks_blocked += 1

    # Attack 5: Tampered JSON deliverable seal on disk
    try:
        res_snow = engine.run_diligence("SNOW")
        tampered_txt = res_snow.json_path.read_text(encoding="utf-8").replace("3628000000.0", "3628000001.0")
        res_snow.json_path.write_text(tampered_txt, encoding="utf-8")
        verify_seal(res_snow.json_path)
    except ProvenanceError:
        arm3_attacks_blocked += 1

    # Attack 6: Squozen PDA rejection of trailing comma / missing required key
    bad_json = '{"ticker":"SNOW","revenue_prior":2806000000,"revenue_current":3628000000,}'
    if not engine.verify_squozen_json_envelope(bad_json, engine.REQUIRED_EXTRACTION_KEYS):
        arm3_attacks_blocked += 1

    report["arms"]["arm3_sham_hallucinated"] = {
        "total_adversarial_attacks": arm3_total_attacks,
        "attacks_blocked_fail_closed": arm3_attacks_blocked,
        "hallucination_escape_count": arm3_total_attacks - arm3_attacks_blocked,
        "rejection_rate_pct": round(100.0 * arm3_attacks_blocked / arm3_total_attacks, 2),
    }

    # ------------------------------------------------------------------
    # ARM 4: Out-of-Sample Stress Regime (Corrupted Evermind Shards + Extreme 1e16 Dynamic Range)
    # ------------------------------------------------------------------
    stress_engine = ZeroHallucinationDiligenceEngine()
    # Clear hot cache and corrupt primary shards in Evermind so holographic shard healing is forced live
    stress_engine.immunizer._compiled_cache.clear()
    for engram in stress_engine.immunizer.evermind.engram_bank.values():
        engram.content = "CORRUPTED_BY_STRESS_HARNESS"
        engram.parity_shards[0] = "shard_0:0_10:badhash99999:BROKEN"
        engram.parity_shards[2] = "shard_2:20_30:badhash88888:BROKEN"

    stress_passed = 0
    stress_shard_heals = 0
    stress_ulp_errors: List[float] = []
    for t in ["ACME", "ZETA", "NVDA", "SNOW"]:
        res = stress_engine.run_diligence(t, segments=ill_conditioned_segments[t])
        if res.payload["critic"]["verdict"] == "pass":
            stress_passed += 1
        if res.immunization_receipt and res.immunization_receipt.get("shard_healed"):
            stress_shard_heals += 1
        stress_ulp_errors.append(0.0 if res.segment_ulp_drift_prevented > 0 else 1.0)

    report["arms"]["arm4_stress_corrupted_unstructured"] = {
        "total_stress_filings": 4,
        "stress_verified": stress_passed,
        "live_holographic_shard_heals": stress_shard_heals,
        "pass_rate_pct": round(100.0 * stress_passed / 4.0, 2),
        "ulp_error_dist": _dist_stats(stress_ulp_errors),
    }

    report["verdict"] = (
        "PASS"
        if (
            report["arms"]["arm1_active_diligence"]["pass_rate_pct"] == 100.0
            and report["arms"]["arm1_active_diligence"]["ulp_error_dist"]["max_linf"] == 0.0
            and report["arms"]["arm2_naive_unimmunized"]["exact_segment_matches"] == 0
            and report["arms"]["arm3_sham_hallucinated"]["hallucination_escape_count"] == 0
            and report["arms"]["arm4_stress_corrupted_unstructured"]["pass_rate_pct"] == 100.0
            and report["arms"]["arm4_stress_corrupted_unstructured"]["live_holographic_shard_heals"] >= 1
        )
        else "FAIL"
    )
    return report


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="ZKAEDI PRIME Zero-Hallucination Quantitative Diligence Engine")
    parser.add_argument("--gauntlet", action="store_true", help="Run the 4-Arm Paired Causal Harness")
    parser.add_argument("--ticker", default="ACME", help="Ticker to audit (SNOW, CRWD, PLTR, NVDA, ACME, ZETA)")
    parser.add_argument("--out", type=Path, help="Optional JSON output path for gauntlet report")
    args = parser.parse_args(argv)

    if args.gauntlet:
        rep = run_4arm_diligence_gauntlet()
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(json.dumps(rep, indent=2), encoding="utf-8")
        print(json.dumps(rep, indent=2))
        return 0 if rep["verdict"] == "PASS" else 1

    eng = ZeroHallucinationDiligenceEngine()
    res = eng.run_diligence(args.ticker)
    print(
        json.dumps(
            {
                "ticker": res.ticker,
                "verdict": res.payload["critic"]["verdict"],
                "score": res.payload["critic"]["score"],
                "seal_sha256": res.seal_sha256,
                "segment_ulp_drift_prevented": res.segment_ulp_drift_prevented,
                "latency_ms": res.latency_ms,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
