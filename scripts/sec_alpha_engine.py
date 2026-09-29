#!/usr/bin/env python3
"""
ZKAEDI PRIME // SEC FILING DILIGENCE ENGINE (v6.0)

Invariants enforced at runtime (fail-closed):
  I1. Every numeric fact is extracted from a filing span by an explicit pattern; nothing is hardcoded.
  I2. Every number in a fact claim appears in its cited span (numeric match with tolerance).
  I3. Every derived claim is recomputed from span-extracted inputs and must match.
  I4. The critic verdict in the deliverable is the verdict the critic actually produced.
  I5. The SHA-256 seal covers the canonical payload and can be re-verified from disk.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import re
import sys
import time
from dataclasses import dataclass, field, asdict
from enum import IntEnum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Pattern, Set, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent
ENGINE_VERSION = "6.0"
REL_TOL = 1e-6


# ======================================================================================
# 1. EXCEPTION HIERARCHY
# ======================================================================================

class ExitCode(IntEnum):
    OK = 0
    CONFIGURATION = 2
    INGESTION = 3
    EXTRACTION = 4
    FEATURE = 5
    ENTAILMENT = 6
    PROVENANCE = 7
    PACKAGING = 8
    UNEXPECTED = 70


class SECAlphaError(Exception):
    """Base error: carries a stable code, structured context, and a process exit code."""
    code: str = "SEC_ALPHA_ERROR"
    exit_code: ExitCode = ExitCode.UNEXPECTED

    def __init__(self, message: str, *, context: Optional[Dict[str, Any]] = None) -> None:
        super().__init__(message)
        self.message = message
        self.context: Dict[str, Any] = dict(context or {})

    def to_dict(self) -> Dict[str, Any]:
        cause = self.__cause__
        return {
            "error": self.code,
            "message": self.message,
            "context": self.context,
            "cause": f"{type(cause).__name__}: {cause}" if cause else None,
        }

    def __str__(self) -> str:
        ctx = ", ".join(f"{k}={v!r}" for k, v in self.context.items())
        return f"[{self.code}] {self.message}" + (f" ({ctx})" if ctx else "")


class ConfigurationError(SECAlphaError):
    code, exit_code = "CONFIGURATION_ERROR", ExitCode.CONFIGURATION

class DataIngestionError(SECAlphaError):
    code, exit_code = "DATA_INGESTION_ERROR", ExitCode.INGESTION

class SpanIndexError(DataIngestionError):
    code = "SPAN_INDEX_ERROR"

class MetricExtractionError(SECAlphaError):
    code, exit_code = "METRIC_EXTRACTION_ERROR", ExitCode.EXTRACTION

class AmbiguousMetricError(MetricExtractionError):
    """Same metric matched with conflicting values in different spans."""
    code = "AMBIGUOUS_METRIC"

class FeatureEngineeringError(SECAlphaError):
    code, exit_code = "FEATURE_ENGINEERING_ERROR", ExitCode.FEATURE

class EntailmentViolationError(SECAlphaError):
    code, exit_code = "ENTAILMENT_VIOLATION", ExitCode.ENTAILMENT

    def __init__(self, message: str, *, violations: List[Dict[str, Any]], **kw: Any) -> None:
        super().__init__(message, **kw)
        self.violations = violations
        self.context["violation_count"] = len(violations)

    def to_dict(self) -> Dict[str, Any]:
        d = super().to_dict()
        d["violations"] = self.violations
        return d

class ProvenanceError(SECAlphaError):
    code, exit_code = "PROVENANCE_ERROR", ExitCode.PROVENANCE

class ArtifactPackagingError(SECAlphaError):
    code, exit_code = "ARTIFACT_PACKAGING_ERROR", ExitCode.PACKAGING


# ======================================================================================
# 2. DATA MODELS
# ======================================================================================

@dataclass(frozen=True)
class GroundedMetric:
    name: str
    value: float
    unit: str             # usd | pct | count
    span_id: str
    quote: str            # exact matched substring from the span

@dataclass(frozen=True)
class DerivedMetric:
    name: str
    value: Optional[float]
    unit: str
    formula: str
    inputs: Tuple[str, ...]

@dataclass
class Claim:
    text: str
    kind: str             # fact | derived
    source_spans: List[str]
    derivation: Optional[Dict[str, Any]] = None
    metric_name: Optional[str] = None

@dataclass
class CriticReport:
    verdict: str          # pass | reject
    score: int
    checked: int
    passed: int
    violations: List[Dict[str, Any]] = field(default_factory=list)

@dataclass
class Dossier:
    company_name: str
    ticker: str
    fiscal_period: str
    spans: Dict[str, str]
    metrics: Dict[str, GroundedMetric]
    missing_optional: List[str]
    derived: Dict[str, DerivedMetric]
    claims: List[Claim]
    kill_criteria: List[str]
    risk_tier: Optional[str]
    critic: Optional[CriticReport] = None


# ======================================================================================
# 3. NUMERIC PRIMITIVES
# ======================================================================================

_NUM_RE = re.compile(r"(?<![A-Za-z\d.])\d[\d,]*(?:\.\d+)?")

def parse_number(raw: str) -> float:
    try:
        value = float(raw.replace(",", ""))
    except (ValueError, AttributeError) as exc:
        raise MetricExtractionError("Unparseable numeric token", context={"raw": raw}) from exc
    if not math.isfinite(value):
        raise MetricExtractionError("Non-finite numeric token", context={"raw": raw})
    return value

def numbers_in(text: str) -> List[float]:
    return [parse_number(m.group()) for m in _NUM_RE.finditer(text)]

def num_eq(a: float, b: float) -> bool:
    return math.isclose(a, b, rel_tol=REL_TOL, abs_tol=1e-9)

def safe_ratio(num: Optional[float], den: Optional[float]) -> Optional[float]:
    """Returns None (not a fake 0.0) when undefined, so gaps stay visible downstream."""
    if num is None or den is None or math.isclose(den, 0.0, abs_tol=1e-12):
        return None
    r = num / den
    return r if math.isfinite(r) else None


# ======================================================================================
# 4. INGESTION
# ======================================================================================

def ingest_filing(ticker: str, custom_text: Optional[str] = None) -> Dict[str, str]:
    t = ticker.strip().upper()
    if not re.fullmatch(r"[A-Z][A-Z.\-]{0,9}", t):
        raise ConfigurationError("Invalid ticker symbol", context={"ticker": ticker})

    try:
        if custom_text and custom_text.strip():
            raw, origin = custom_text, "custom_text"
        elif t in CURATED_FILING_REPOSITORY:
            raw, origin = CURATED_FILING_REPOSITORY[t], "curated"
        else:
            path = REPO_ROOT / "data" / "sec_cache" / f"{t}_filing.txt"
            raw, origin = path.read_text(encoding="utf-8"), str(path)
    except FileNotFoundError as exc:
        raise DataIngestionError("No filing source for ticker", context={"ticker": t}) from exc
    except (UnicodeDecodeError, OSError) as exc:
        raise DataIngestionError("Filing unreadable", context={"ticker": t}) from exc

    lines = [ln.strip() for ln in raw.splitlines() if ln.strip()]
    if sum(len(l) for l in lines) < 50:
        raise DataIngestionError("Filing content too short", context={"ticker": t, "origin": origin})

    spans = {f"s{i}": line for i, line in enumerate(lines, start=1)}
    if len(set(spans.values())) != len(spans):
        # Duplicate lines make citations ambiguous; refuse rather than guess.
        raise SpanIndexError("Duplicate span text detected", context={"ticker": t})
    return spans


# ======================================================================================
# 5. SPAN-GROUNDED METRIC EXTRACTION
# ======================================================================================

@dataclass(frozen=True)
class MetricSpec:
    name: str
    unit: str
    pattern: Pattern[str]
    required: bool
    transform: Callable[[re.Match], float] = lambda m: parse_number(next(g for g in m.groups() if g))

_ZERO_DEBT = re.compile(r"\bzero\b[^.]{0,30}\bdebt\b", re.I)

METRIC_SPECS: Tuple[MetricSpec, ...] = (
    MetricSpec("revenue_prior", "usd", re.compile(r"FY24 Revenue reached \$([\d,]+)"), True),
    MetricSpec("revenue_current", "usd", re.compile(r"FY25 Revenue expanded to \$([\d,]+)"), True),
    MetricSpec("customer_count", "count", re.compile(r"across ([\d,]+) enterprise (?:customers|clients|accounts)"), False),
    MetricSpec("net_retention", "pct", re.compile(r"Net Retention Rate of ([\d.]+)%"), False),
    MetricSpec("gross_margin", "pct", re.compile(r"Gross Margin[^%$]{0,40}?([\d.]+)%"), True),
    MetricSpec("operating_margin", "pct", re.compile(r"Operating Margin of ([\d.]+)%"), False),
    MetricSpec("free_cash_flow", "usd", re.compile(r"Free Cash Flow[^$]{0,40}\$([\d,]+)"), False),
    MetricSpec("cash", "usd", re.compile(r"\$([\d,]+) in cash|[Cc]ash[\w\s-]{0,40}?totaled \$([\d,]+)"), False),
    MetricSpec("total_debt", "usd", re.compile(r"\$([\d,]+) (?:long-term )?debt|\bzero\b[^.]{0,30}\bdebt\b", re.I), False,
               transform=lambda m: 0.0 if m.group(1) is None else parse_number(m.group(1))),
    MetricSpec("rpo", "usd", re.compile(r"(?:RPO|Remaining Performance Obligations)[^$]{0,60}\$([\d,]+)"), False),
)

def extract_metrics(spans: Dict[str, str]) -> Tuple[Dict[str, GroundedMetric], List[str]]:
    found: Dict[str, GroundedMetric] = {}
    missing: List[str] = []
    for spec in METRIC_SPECS:
        hits: List[GroundedMetric] = []
        for sid, text in spans.items():
            for m in spec.pattern.finditer(text):
                try:
                    val = spec.transform(m)
                except StopIteration as exc:
                    raise MetricExtractionError("Pattern matched without value group",
                                                context={"metric": spec.name, "span": sid}) from exc
                hits.append(GroundedMetric(spec.name, val, spec.unit, sid, m.group(0)))

        distinct = {round(h.value, 6) for h in hits}
        if len(distinct) > 1:
            raise AmbiguousMetricError("Conflicting values for metric",
                                       context={"metric": spec.name,
                                                "hits": [(h.span_id, h.value) for h in hits]})
        if hits:
            found[spec.name] = hits[0]
        elif spec.required:
            raise MetricExtractionError("Required metric not found in any span",
                                        context={"metric": spec.name})
        else:
            missing.append(spec.name)
    return found, missing


# ======================================================================================
# 6. FEATURE ENGINEERING (derived only from grounded metrics)
# ======================================================================================

ANALYST_KILL_CRITERIA: Dict[str, List[str]] = {
    "NVDA": ["TSMC advanced CoWoS packaging allocation drops by > 20%.",
             "Hyperscale CAPEX growth decelerates below 15% YoY."],
    "CRWD": ["Net customer additions turn negative for two consecutive quarters.",
             "Gross margin compresses below 72% due to cloud compute overhead."],
    "PLTR": ["US Commercial customer growth slows below 30% YoY.",
             "Government contract renewal delays exceed 6 months."],
    "SNOW": ["Net retention drops below 115% for two consecutive quarters.",
             "Storage consumption pricing migration compresses gross margins."],
}

def _v(metrics: Dict[str, GroundedMetric], name: str) -> Optional[float]:
    m = metrics.get(name)
    return m.value if m else None

def engineer_features(metrics: Dict[str, GroundedMetric]) -> Dict[str, DerivedMetric]:
    try:
        rp, rc = _v(metrics, "revenue_prior"), _v(metrics, "revenue_current")
        growth = safe_ratio(None if rp is None or rc is None else rc - rp, rp)
        fcf_margin = safe_ratio(_v(metrics, "free_cash_flow"), rc)
        cash, debt = _v(metrics, "cash"), _v(metrics, "total_debt")

        d = {
            "yoy_growth_pct": DerivedMetric("yoy_growth_pct", None if growth is None else growth * 100, "pct",
                                            "(revenue_current - revenue_prior) / revenue_prior * 100",
                                            ("revenue_prior", "revenue_current")),
            "fcf_margin_pct": DerivedMetric("fcf_margin_pct", None if fcf_margin is None else fcf_margin * 100, "pct",
                                            "free_cash_flow / revenue_current * 100",
                                            ("free_cash_flow", "revenue_current")),
            "net_cash": DerivedMetric("net_cash", None if cash is None or debt is None else cash - debt, "usd",
                                      "cash - total_debt", ("cash", "total_debt")),
            "arpa": DerivedMetric("arpa", safe_ratio(rc, _v(metrics, "customer_count")), "usd",
                                  "revenue_current / customer_count", ("revenue_current", "customer_count")),
        }
        g, f = d["yoy_growth_pct"].value, d["fcf_margin_pct"].value
        d["rule_of_40"] = DerivedMetric("rule_of_40", None if g is None or f is None else g + f, "score",
                                        "yoy_growth_pct + fcf_margin_pct", ("yoy_growth_pct", "fcf_margin_pct"))
    except (TypeError, ArithmeticError) as exc:
        raise FeatureEngineeringError("Derived metric computation failed") from exc
    return d


# ======================================================================================
# 7. CLAIM GENERATION (only from what was extracted)
# ======================================================================================

def _fmt(value: float, unit: str) -> str:
    if unit == "usd":
        return f"${value:,.0f}"
    if unit == "pct":
        return f"{value:.1f}%"
    if unit == "count":
        return f"{value:,.0f}"
    return f"{value:.1f}"

FACT_TEMPLATES = {
    "revenue_prior": "FY24 revenue was {v}.",
    "revenue_current": "FY25 revenue was {v}.",
    "customer_count": "The company reported {v} enterprise customers.",
    "net_retention": "Net Retention Rate was {v}.",
    "gross_margin": "Gross margin was {v}.",
    "operating_margin": "Operating margin was {v}.",
    "free_cash_flow": "Free cash flow was {v}.",
    "cash": "Cash and equivalents totaled {v}.",
    "total_debt": "Total debt was {v}.",
    "rpo": "Remaining performance obligations were {v}.",
}

def resolve_inputs_to_spans(inputs: Tuple[str, ...], metrics: Dict[str, GroundedMetric], derived: Dict[str, DerivedMetric]) -> List[str]:
    spans: Set[str] = set()
    for inp in inputs:
        if inp in metrics:
            spans.add(metrics[inp].span_id)
        elif inp in derived:
            spans.update(resolve_inputs_to_spans(derived[inp].inputs, metrics, derived))
    return sorted(spans)

def build_claims(metrics: Dict[str, GroundedMetric], derived: Dict[str, DerivedMetric]) -> List[Claim]:
    claims = [Claim(FACT_TEMPLATES[n].format(v=_fmt(m.value, m.unit)), "fact", [m.span_id], metric_name=n)
              for n, m in metrics.items() if n in FACT_TEMPLATES]
    for n, d in derived.items():
        if d.value is None:
            continue
        spans = resolve_inputs_to_spans(d.inputs, metrics, derived)
        title = n.replace("_", " ")
        claims.append(Claim(f"{title} computed as {_fmt(d.value, d.unit)} ({d.formula}).",
                            "derived", spans, {"metric": n, "value": d.value}, metric_name=n))
    return claims


# ======================================================================================
# 8. ADVERSARIAL CRITIC (fail-closed)
# ======================================================================================

def run_critic(dossier: Dossier, *, strict: bool = True) -> CriticReport:
    violations: List[Dict[str, Any]] = []
    passed = 0
    for idx, c in enumerate(dossier.claims):
        try:
            unknown = [s for s in c.source_spans if s not in dossier.spans]
            if not c.source_spans or unknown:
                msg = unknown or "none cited"
                raise ValueError(f"missing or unknown spans: {msg}")

            if c.kind == "fact":
                # Fact claim must cite the span its own extracted metric originated from
                if c.metric_name and c.metric_name in dossier.metrics:
                    expected_span = dossier.metrics[c.metric_name].span_id
                    if expected_span not in c.source_spans:
                        raise ValueError(f"fact claim for {c.metric_name} does not cite its source span {expected_span}")

                span_nums = [n for s in c.source_spans for n in numbers_in(dossier.spans[s])]
                # A textual "zero X" grounds 0.0 only for claims about X.
                if c.metric_name == "total_debt" and any(
                    _ZERO_DEBT.search(dossier.spans[s]) for s in c.source_spans
                ):
                    span_nums.append(0.0)

                claim_nums = numbers_in(c.text)
                if not claim_nums:
                    raise ValueError("fact claim contains no verifiable number")
                orphan = [n for n in claim_nums if not any(num_eq(n, s) for s in span_nums)]
                if orphan:
                    raise ValueError(f"numbers not present in cited spans: {orphan}")

            elif c.kind == "derived":
                deriv = c.derivation or {}
                name = deriv.get("metric")
                if name not in dossier.derived:
                    raise ValueError(f"unknown derived metric {name!r}")
                recomputed = engineer_features(dossier.metrics)[name].value
                if recomputed is None or not num_eq(recomputed, float(deriv.get("value", math.nan))):
                    raise ValueError(f"recomputation mismatch: claimed={deriv.get('value')} recomputed={recomputed}")
            else:
                raise ValueError(f"unsupported claim kind {c.kind!r}")

        except (ValueError, TypeError, KeyError, MetricExtractionError) as exc:
            violations.append({"claim_index": idx, "text": c.text, "reason": str(exc)})
        else:
            passed += 1

    checked = len(dossier.claims)
    score = 0 if checked == 0 else round(100 * passed / checked)
    report = CriticReport("pass" if checked and not violations else "reject", score, checked, passed, violations)

    if strict and report.verdict != "pass":
        raise EntailmentViolationError("Critic rejected dossier",
                                       violations=violations,
                                       context={"ticker": dossier.ticker, "score": score})
    return report


# ======================================================================================
# 9. PACKAGING & VERIFIABLE SEAL
# ======================================================================================

def canonical_bytes(obj: Any) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")

def seal(payload: Dict[str, Any]) -> Dict[str, Any]:
    body = {k: v for k, v in payload.items() if k != "provenance"}
    sealed = dict(body)
    sealed["provenance"] = {"algorithm": "sha256",
                            "scope": "canonical JSON of payload excluding 'provenance'",
                            "digest": hashlib.sha256(canonical_bytes(body)).hexdigest()}
    return sealed

def verify_seal(path: Path) -> str:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        claimed = data["provenance"]["digest"]
    except (OSError, json.JSONDecodeError, KeyError, TypeError) as exc:
        raise ProvenanceError("Deliverable unreadable or unsealed", context={"path": str(path)}) from exc
    actual = hashlib.sha256(canonical_bytes({k: v for k, v in data.items() if k != "provenance"})).hexdigest()
    if actual != claimed:
        raise ProvenanceError("Digest mismatch: deliverable was modified",
                              context={"path": str(path), "claimed": claimed, "actual": actual})
    return actual

def build_payload(d: Dossier, job_id: str, customer_id: str, elapsed_s: float) -> Dict[str, Any]:
    if d.critic is None:
        raise ArtifactPackagingError("Refusing to package un-reviewed dossier", context={"job_id": job_id})
    return {
        "engine_version": ENGINE_VERSION,
        "status": "COMPLETED",
        "dispatch_status": "NOT_DISPATCHED",
        "job_id": job_id,
        "customer_id": customer_id,
        "ticker": d.ticker,
        "company_name": d.company_name,
        "fiscal_period": d.fiscal_period,
        "execution_time_s": round(elapsed_s, 4),
        "metrics": [asdict(m) for m in d.metrics.values()],
        "metrics_unavailable": d.missing_optional,
        "derived_metrics": [asdict(x) for x in d.derived.values()],
        "claims": [asdict(c) for c in d.claims],
        "kill_criteria": {"source": "analyst_assumption", "items": d.kill_criteria},
        "concentration_risk_tier": d.risk_tier,
        "critic": asdict(d.critic),
        "source_spans": d.spans,
    }

def write_deliverables(payload: Dict[str, Any], out_dir: Path) -> Tuple[Path, Path]:
    job_id, ticker = payload["job_id"], payload["ticker"]
    json_path = out_dir / f"{job_id}.json"
    html_path = out_dir / f"memo_{ticker.lower()}_{job_id}.html"
    tmp = json_path.with_suffix(".json.tmp")
    try:
        out_dir.mkdir(parents=True, exist_ok=True)
        tmp.write_text(json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        tmp.replace(json_path)  # atomic on same filesystem
        html_path.write_text(render_html(payload), encoding="utf-8")
        verify_seal(json_path)  # read-back verification
        ledger = REPO_ROOT / "data" / "sec_audit_ledger.jsonl"
        ledger.parent.mkdir(parents=True, exist_ok=True)
        with ledger.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps({"ts": int(time.time()), "job_id": job_id, "ticker": ticker,
                                 "sha256": payload["provenance"]["digest"],
                                 "verdict": payload["critic"]["verdict"],
                                 "json_path": str(json_path)}) + "\n")
    except ProvenanceError:
        raise
    except OSError as exc:
        raise ArtifactPackagingError("Failed to write deliverables",
                                     context={"job_id": job_id, "dir": str(out_dir)}) from exc
    finally:
        if tmp.exists():
            tmp.unlink(missing_ok=True)
    return json_path, html_path

def render_html(p: Dict[str, Any]) -> str:
    e = html.escape
    rows = "".join(
        f"<tr><td>{e(m['name'])}</td><td>{e(_fmt(m['value'], m['unit']))}</td>"
        f"<td>{e(m['span_id'])}</td><td><code>{e(m['quote'])}</code></td></tr>" for m in p["metrics"])
    claims = "".join(f"<li>{e(c['text'])} <small>[{e(', '.join(c['source_spans']))}]</small></li>"
                     for c in p["claims"])
    kills = "".join(f"<li>{e(k)}</li>" for k in p["kill_criteria"]["items"])
    crit = p["critic"]
    return f"""<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<title>{e(p['company_name'])} ({e(p['ticker'])})</title>
<style>body{{font-family:system-ui,sans-serif;margin:24px;color:#0f172a}}table{{border-collapse:collapse;width:100%}}
td,th{{border-bottom:1px solid #e2e8f0;padding:6px;text-align:left;font-size:12px}}code{{font-size:11px}}</style></head><body>
<h1>{e(p['company_name'])} ({e(p['ticker'])}) — {e(p['fiscal_period'])}</h1>
<p><strong>Critic:</strong> {e(crit['verdict'].upper())} — {crit['passed']}/{crit['checked']} claims verified ({crit['score']}/100)</p>
<h2>Span-grounded metrics</h2><table><tr><th>Metric</th><th>Value</th><th>Span</th><th>Exact quote</th></tr>{rows}</table>
<p><em>Unavailable in filing:</em> {e(', '.join(p['metrics_unavailable']) or 'none')}</p>
<h2>Verified claims</h2><ul>{claims}</ul>
<h2>Kill criteria (analyst assumptions, not filing facts)</h2><ul>{kills}</ul>
<footer><code>sha256:{e(p['provenance']['digest'])}</code> • engine v{ENGINE_VERSION}</footer></body></html>"""


# ======================================================================================
# 10. ORCHESTRATION
# ======================================================================================

COMPANY_NAMES = {
    "NVDA": "NVIDIA Corporation",
    "CRWD": "CrowdStrike Holdings Inc.",
    "PLTR": "Palantir Technologies Inc.",
    "SNOW": "Snowflake Inc."
}

def run_job(ticker: str, customer_id: str, out_dir: Optional[Path] = None,
            custom_text: Optional[str] = None, strict: bool = True) -> Tuple[Dict[str, Any], Path, Path]:
    t0 = time.perf_counter()
    t = ticker.strip().upper()
    job_id = f"job_{t.lower()}_{hashlib.sha256(f'{t}{time.time_ns()}'.encode()).hexdigest()[:10]}"

    spans = ingest_filing(t, custom_text)
    metrics, missing = extract_metrics(spans)
    derived = engineer_features(metrics)

    dossier = Dossier(
        company_name=COMPANY_NAMES.get(t, f"{t} (unverified entity name)"),
        ticker=t, fiscal_period="FY25", spans=spans, metrics=metrics,
        missing_optional=missing, derived=derived, claims=build_claims(metrics, derived),
        kill_criteria=ANALYST_KILL_CRITERIA.get(t, []),
        risk_tier=None,  # concentration % is not disclosed in these filings; do not fabricate
    )
    dossier.critic = run_critic(dossier, strict=strict)

    payload = seal(build_payload(dossier, job_id, customer_id, time.perf_counter() - t0))
    json_path, html_path = write_deliverables(payload, out_dir or REPO_ROOT / "deliverables")
    return payload, json_path, html_path


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="ZKAEDI SEC Diligence Engine v6")
    ap.add_argument("--ticker", default="SNOW")
    ap.add_argument("--client", default="cust_default")
    ap.add_argument("--filing", type=Path, help="Path to a filing text file (overrides curated text)")
    ap.add_argument("--lenient", action="store_true", help="Report critic violations instead of aborting")
    ap.add_argument("--verify", type=Path, help="Verify the seal of an existing deliverable and exit")
    args = ap.parse_args(argv)

    try:
        if args.verify:
            print(f"✓ Seal valid: sha256:{verify_seal(args.verify)}")
            return ExitCode.OK
        custom = args.filing.read_text(encoding="utf-8") if args.filing else None
        payload, jp, hp = run_job(args.ticker, args.client, custom_text=custom, strict=not args.lenient)
    except SECAlphaError as err:
        print(json.dumps(err.to_dict(), indent=2, default=str), file=sys.stderr)
        return err.exit_code
    except OSError as exc:
        print(json.dumps(DataIngestionError("Cannot read --filing", context={"path": str(args.filing)})
                         .to_dict() | {"cause": str(exc)}), file=sys.stderr)
        return ExitCode.INGESTION
    except Exception as exc:  # last-resort boundary: never report success on unknown failure
        print(json.dumps({"error": "UNEXPECTED", "type": type(exc).__name__, "message": str(exc)}), file=sys.stderr)
        return ExitCode.UNEXPECTED

    c = payload["critic"]
    print(f"✓ {payload['ticker']}: critic={c['verdict'].upper()} {c['passed']}/{c['checked']} "
          f"| unavailable={payload['metrics_unavailable'] or 'none'}")
    print(f"  JSON: {jp}\n  HTML: {hp}\n  sha256:{payload['provenance']['digest']}")
    return ExitCode.OK


# ======================================================================================
# 11. CURATED REPOSITORY OF HIGH-CONVICTION FILINGS
# ======================================================================================

CURATED_FILING_REPOSITORY: Dict[str, str] = {
    "SNOW": """
Snowflake Inc. Financial Filing Q4 2025.
Line Item 1: FY24 Revenue reached $2,806,000,000 across 9,437 enterprise customers.
Line Item 2: FY25 Revenue expanded to $3,628,000,000 with Net Retention Rate of 127.0%.
Operating discipline remained resilient with Gross Margin above 76.5%.
Free Cash Flow exceeded $890,000,000 with Rule of 40 score standing at 53.8.
Balance sheet fortress maintained with $4,800,000,000 in cash and short-term investments and zero debt.
Remaining Performance Obligations (RPO) expanded to $5,200,000,000.
Kill criteria established for storage consumption pricing shifts.
""",
    "CRWD": """
CrowdStrike Holdings Inc. Financial Filing Q4 2025.
Line Item 1: FY24 Revenue reached $3,056,000,000 across 29,000 enterprise clients.
Line Item 2: FY25 Revenue expanded to $3,980,000,000 with Net Retention Rate of 119.5%.
Gross Margin sustained at 78.2% supported by Falcon platform module adoption.
Free Cash Flow reached $1,120,000,000 with Operating Margin of 22.4%.
Cash reserves totaled $3,900,000,000 against $740,000,000 long-term debt.
RPO backlog expanded to $4,800,000,000 with strong annual contract value commitments.
Kill criteria active against operational multi-cloud single-agent disruptions.
""",
    "PLTR": """
Palantir Technologies Inc. Financial Filing Q4 2025.
Line Item 1: FY24 Revenue reached $2,225,000,000 across 497 enterprise accounts.
Line Item 2: FY25 Revenue expanded to $2,860,000,000 with Net Retention Rate of 114.0%.
Gross Margin remained industry-leading at 81.2% with Operating Margin of 26.8%.
Free Cash Flow generated $950,000,000 representing robust software cash conversion.
Zero funded long-term debt with $4,000,000,000 in cash and US Treasury bills.
US Commercial expansion surged with RPO backlog reaching $1,350,000,000.
Kill criteria established for government contract renewal delays exceeding 6 months.
""",
    "NVDA": """
NVIDIA Corporation Financial Filing FY25.
Line Item 1: FY24 Revenue reached $60,922,000,000 across global hyperscale data centers.
Line Item 2: FY25 Revenue expanded to $125,500,000,000 with Data Center growth exceeding 140.0%.
Gross Margin reached 75.0% with GAAP Operating Margin of 62.5%.
Free Cash Flow reached historic record of $60,800,000,000 with Rule of 40 score exceeding 150.
Cash and marketable securities totaled $34,800,000,000 against $9,700,000,000 debt.
Remaining Performance Obligations reached $38,000,000,000 with multi-year supply commitments.
Kill criteria active against advanced CoWoS packaging substrate constraints.
"""
}

if __name__ == "__main__":
    sys.exit(main())
