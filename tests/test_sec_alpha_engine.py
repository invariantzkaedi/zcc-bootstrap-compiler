#!/usr/bin/env python3
"""
Test Suite for ZKAEDI PRIME SEC Diligence Engine (v6.0)
Formally verifies Runtime Invariants I1 through I5 under adversarial conditions.
"""
from __future__ import annotations

import json
from pathlib import Path
import pytest

from scripts.sec_alpha_engine import (
    CURATED_FILING_REPOSITORY,
    Claim,
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
    run_critic,
    run_job,
    seal,
    verify_seal,
    write_deliverables,
)


# ======================================================================================
# INVARIANT I1: Every numeric fact extracted from span by pattern (nothing hardcoded)
# ======================================================================================

def test_i1_dynamic_extraction_mutated_revenue() -> None:
    """Mutating revenue figure in filing must produce altered metric, proving dynamic extraction."""
    base_text = CURATED_FILING_REPOSITORY["SNOW"]
    mutated_text = base_text.replace("$3,628,000,000", "$9,999,000,000")
    
    spans = ingest_filing("SNOW", custom_text=mutated_text)
    metrics, missing = extract_metrics(spans)
    
    assert metrics["revenue_current"].value == 9_999_000_000.0
    assert "9,999,000,000" in metrics["revenue_current"].quote


def test_i1_missing_required_metric_raises_extraction_error() -> None:
    """Missing a required metric (e.g. gross_margin) must fail closed with MetricExtractionError."""
    incomplete_filing = (
        "Acme Corp Financial Filing.\n"
        "Line Item 1: FY24 Revenue reached $1,000,000 across 100 enterprise clients.\n"
        "Line Item 2: FY25 Revenue expanded to $2,000,000.\n"
        "Free Cash Flow reached $100,000.\n"
    )
    spans = ingest_filing("ACME", custom_text=incomplete_filing)
    with pytest.raises(MetricExtractionError) as exc_info:
        extract_metrics(spans)
    
    assert exc_info.value.context.get("metric") == "gross_margin"


def test_i1_optional_missing_metrics_not_invented() -> None:
    """Optional metrics absent from NVDA filing must appear in missing_optional, not fabricated."""
    spans = ingest_filing("NVDA")
    metrics, missing = extract_metrics(spans)
    
    assert "customer_count" in missing
    assert "net_retention" in missing
    assert "customer_count" not in metrics
    assert "net_retention" not in metrics


# ======================================================================================
# INVARIANT I2: Every number in fact claim appears in cited span (and correct source span)
# ======================================================================================

def test_i2_claim_with_unsupported_number_rejected() -> None:
    """A claim citing a span lacking its number raises EntailmentViolationError."""
    spans = ingest_filing("SNOW")
    metrics, missing = extract_metrics(spans)
    derived = engineer_features(metrics)
    claims = build_claims(metrics, derived)
    
    # Introduce a rogue claim citing s2 ("$2,806,000,000 across 9,437 enterprise customers")
    # with an ungrounded number
    claims.append(Claim(
        text="The company reported 42,000 enterprise customers.",
        kind="fact",
        source_spans=["s2"],
        metric_name="customer_count"
    ))
    
    dossier = Dossier(
        company_name="Snowflake Inc.",
        ticker="SNOW",
        fiscal_period="FY25",
        spans=spans,
        metrics=metrics,
        missing_optional=missing,
        derived=derived,
        claims=claims,
        kill_criteria=[],
        risk_tier=None,
    )
    
    with pytest.raises(EntailmentViolationError) as exc_info:
        run_critic(dossier, strict=True)
    
    violations = exc_info.value.violations
    assert any("numbers not present in cited spans" in v["reason"] for v in violations)


def test_i2_fact_claim_citing_wrong_span_rejected() -> None:
    """A fact claim must cite its own metric source span, not an unrelated span."""
    spans = ingest_filing("SNOW")
    metrics, missing = extract_metrics(spans)
    derived = engineer_features(metrics)
    claims = build_claims(metrics, derived)
    
    # Deliberately point revenue_current (extracted from s3) at header line s1
    for c in claims:
        if c.metric_name == "revenue_current":
            c.source_spans = ["s1"]
    
    dossier = Dossier(
        company_name="Snowflake Inc.",
        ticker="SNOW",
        fiscal_period="FY25",
        spans=spans,
        metrics=metrics,
        missing_optional=missing,
        derived=derived,
        claims=claims,
        kill_criteria=[],
        risk_tier=None,
    )
    
    with pytest.raises(EntailmentViolationError) as exc_info:
        run_critic(dossier, strict=True)
    
    violations = exc_info.value.violations
    assert any("does not cite its source span" in v["reason"] for v in violations)


def test_i2_zero_debt_narrowed_relaxation() -> None:
    """A span with 'zero tolerance' must NOT ground total_debt=0.0 unless matching _ZERO_DEBT."""
    filing_with_zero_tolerance = (
        "CyberSec Inc Financial Filing.\n"
        "Line Item 1: FY24 Revenue reached $100,000 across 50 enterprise accounts.\n"
        "Line Item 2: FY25 Revenue expanded to $200,000.\n"
        "Gross Margin sustained at 80.0%.\n"
        "Company maintains zero tolerance for security incidents and $50,000 long-term debt.\n"
    )
    spans = ingest_filing("TEST", custom_text=filing_with_zero_tolerance)
    metrics, missing = extract_metrics(spans)
    
    # Ensure total_debt extracted the actual number 50,000, not 0.0
    assert metrics["total_debt"].value == 50_000.0
    
    # Conversely, in SNOW ("zero debt"), total_debt is legitimately 0.0 and grounds correctly
    snow_spans = ingest_filing("SNOW")
    snow_metrics, snow_missing = extract_metrics(snow_spans)
    assert snow_metrics["total_debt"].value == 0.0
    snow_derived = engineer_features(snow_metrics)
    snow_claims = build_claims(snow_metrics, snow_derived)
    
    snow_dossier = Dossier(
        company_name="Snowflake Inc.",
        ticker="SNOW",
        fiscal_period="FY25",
        spans=snow_spans,
        metrics=snow_metrics,
        missing_optional=snow_missing,
        derived=snow_derived,
        claims=snow_claims,
        kill_criteria=[],
        risk_tier=None,
    )
    report = run_critic(snow_dossier, strict=True)
    assert report.verdict == "pass"


# ======================================================================================
# INVARIANT I3: Every derived claim recomputed from span-extracted inputs must match
# ======================================================================================

def test_i3_tampered_derived_claim_rejected() -> None:
    """A derived claim with a tampered value raises EntailmentViolationError."""
    spans = ingest_filing("CRWD")
    metrics, missing = extract_metrics(spans)
    derived = engineer_features(metrics)
    claims = build_claims(metrics, derived)
    
    # Tamper with the yoy_growth_pct claim value
    for c in claims:
        if c.metric_name == "yoy_growth_pct":
            c.derivation["value"] = 999.9  # fake value
    
    dossier = Dossier(
        company_name="CrowdStrike Holdings Inc.",
        ticker="CRWD",
        fiscal_period="FY25",
        spans=spans,
        metrics=metrics,
        missing_optional=missing,
        derived=derived,
        claims=claims,
        kill_criteria=[],
        risk_tier=None,
    )
    
    with pytest.raises(EntailmentViolationError) as exc_info:
        run_critic(dossier, strict=True)
    
    violations = exc_info.value.violations
    assert any("recomputation mismatch" in v["reason"] for v in violations)


def test_i3_rule_of_40_transitive_span_resolution() -> None:
    """rule_of_40 must transitively resolve to the spans of its inputs' inputs."""
    spans = ingest_filing("SNOW")
    metrics, missing = extract_metrics(spans)
    derived = engineer_features(metrics)
    claims = build_claims(metrics, derived)
    
    r40_claim = next(c for c in claims if c.metric_name == "rule_of_40")
    assert r40_claim.source_spans == ["s2", "s3", "s5"]


# ======================================================================================
# INVARIANT I4: Critic verdict in deliverable matches critic report exactly
# ======================================================================================

def test_i4_critic_verdict_fidelity(tmp_path: Path) -> None:
    """Payload critic summary must match the actual CriticReport verdict, score, and checks."""
    payload, jp, hp = run_job("PLTR", "test_client", out_dir=tmp_path, strict=True)
    
    assert payload["critic"]["verdict"] == "pass"
    assert payload["critic"]["score"] == 100
    assert payload["critic"]["checked"] == 15
    assert payload["critic"]["passed"] == 15
    assert payload["critic"]["violations"] == []


# ======================================================================================
# INVARIANT I5: SHA-256 seal covers canonical payload and rejects byte-level tampering
# ======================================================================================

def test_i5_seal_verification_and_tamper_rejection(tmp_path: Path) -> None:
    """verify_seal must pass on unaltered file and fail closed on byte-level tamper."""
    payload, jp, hp = run_job("NVDA", "test_client", out_dir=tmp_path, strict=True)
    
    # 1. Unaltered verify passes
    verified_hash = verify_seal(jp)
    assert verified_hash == payload["provenance"]["digest"]
    
    # 2. Mutate a byte in JSON
    raw_content = jp.read_text(encoding="utf-8")
    tampered_content = raw_content.replace('"status": "COMPLETED"', '"status": "MODIFIED"')
    tampered_path = tmp_path / "tampered.json"
    tampered_path.write_text(tampered_content, encoding="utf-8")
    
    with pytest.raises(ProvenanceError) as exc_info:
        verify_seal(tampered_path)
    
    assert "Digest mismatch" in exc_info.value.message


# ======================================================================================
# COMPREHENSIVE END-TO-END SUITE: ALL 4 TICKERS
# ======================================================================================

@pytest.mark.parametrize("ticker,expected_checked,expected_unavailable", [
    ("SNOW", 14, ["operating_margin"]),
    ("CRWD", 15, []),
    ("PLTR", 15, []),
    ("NVDA", 12, ["customer_count", "net_retention"]),
])
def test_all_four_tickers_end_to_end(
    tmp_path: Path,
    ticker: str,
    expected_checked: int,
    expected_unavailable: list[str]
) -> None:
    """End-to-end execution of all 4 tickers with verification and seal check."""
    payload, jp, hp = run_job(ticker, "test_client", out_dir=tmp_path, strict=True)
    
    assert payload["status"] == "COMPLETED"
    assert payload["critic"]["verdict"] == "pass"
    assert payload["critic"]["checked"] == expected_checked
    assert payload["critic"]["passed"] == expected_checked
    assert payload["metrics_unavailable"] == expected_unavailable
    assert jp.exists()
    assert hp.exists()
    
    # Verify seal
    assert verify_seal(jp) == payload["provenance"]["digest"]
