#!/usr/bin/env python3
"""
mega_report.py — ZKAEDI MEGA SWARM Scored Report Generator
Upgraded: emits per-contract vulnerability scores, not just ABI counts.

Usage:
    python mega_report.py <corpus_dir> <decomp_dir> <output_html>

Scoring model (Hamiltonian-aligned severity weights):
    CRITICAL pattern  = 10 pts   (reentrancy, selfdestruct, unchecked call)
    HIGH pattern      =  5 pts   (delegatecall, tx.origin auth, overflow path)
    MEDIUM pattern    =  2 pts   (assembly block, low-level call, event missing)
    Complexity bonus  = +1 pt per 1000 chars of decompiled output

Outputs:
    - HTML report with sortable score table
    - JSONL corpus file: mega_scored_corpus.jsonl (one record per contract)
"""
import os
import sys
import json
import re
from collections import Counter, defaultdict
from datetime import datetime

# ── Vulnerability Pattern Library ────────────────────────────────────────

PATTERNS = [
    # Critical — matched against enriched ZCC decompile output (Patches 1+2)
    #   CALL(addr), DELEGATECALL(addr), STATICCALL(addr)
    #   STORAGE[slot], MEMORY[offset], TRANSIENT[slot], CALLDATA[offset]

    # Reentrancy: CALL followed by SSTORE within 500 chars
    {"id": "REENTRANCY",     "regex": r"CALL\([^)]*\)[\s\S]{0,500}STORAGE\[",
     "severity": "CRITICAL", "weight": 10,
     "desc": "External CALL followed by STORAGE write (reentrancy pattern)"},
    {"id": "SELFDESTRUCT",   "regex": r"\bSELFDESTRUCT\b|\bselfdestruct\b|\bsuicide\b",
     "severity": "CRITICAL", "weight": 10,
     "desc": "Selfdestruct opcode present"},
    # Multiple unguarded SSTORE writes
    {"id": "UNCHECKED_SSTORE","regex": r"STORAGE\[t\d+\]\s*=\s*t\d+[\s\S]{0,200}STORAGE\[t\d+\]\s*=\s*t\d+",
     "severity": "CRITICAL", "weight": 8,
     "desc": "Multiple unguarded STORAGE writes (potential storage corruption)"},

    # High
    {"id": "DELEGATECALL",   "regex": r"DELEGATECALL\(",
     "severity": "HIGH", "weight": 6,
     "desc": "DELEGATECALL detected — callee executes in caller's storage context"},
    {"id": "TX_ORIGIN",      "regex": r"\bORIGIN\b|\btx_origin\b",
     "severity": "HIGH", "weight": 5,
     "desc": "Authentication via tx.origin / ORIGIN opcode"},
    {"id": "CALL_UNCHECKED", "regex": r"CALL\([^)]*\)(?![\s\S]{0,50}if\s*\()",
     "severity": "HIGH", "weight": 4,
     "desc": "External CALL with return value not checked within 50 chars"},
    {"id": "ARITH_OVERFLOW", "regex": r"t\d+\s*[+\-\*]\s*t\d+(?!.*assert|.*require|.*safe)",
     "severity": "HIGH", "weight": 4,
     "desc": "Arithmetic without bounds check (potential overflow)"},
    {"id": "DEEP_CALLSTACK", "regex": r"(func_0x[0-9a-f]+.*\n){5,}",
     "severity": "HIGH", "weight": 3,
     "desc": "High ABI function density (complex attack surface)"},
    # CALL followed by MEMORY write (less severe than STORAGE write)
    {"id": "CALL_MEM_WRITE", "regex": r"CALL\([^)]*\)[\s\S]{0,200}MEMORY\[",
     "severity": "HIGH", "weight": 3,
     "desc": "External CALL followed by MEMORY write"},

    # Medium
    {"id": "SSTORE_PATTERN", "regex": r"STORAGE\[t\d+\]\s*=",
     "severity": "MEDIUM", "weight": 2,
     "desc": "STORAGE write (SSTORE) — state mutation"},
    {"id": "ZERO_COMPARE",   "regex": r"t\d+\s*==\s*0x0.*\n.*STORAGE\[",
     "severity": "MEDIUM", "weight": 2,
     "desc": "Zero-comparison gating storage write (access control pattern)"},
    {"id": "MOD_ARITHMETIC", "regex": r"t\d+\s*%\s*t\d+",
     "severity": "MEDIUM", "weight": 2,
     "desc": "Modulo arithmetic (division-by-zero risk if divisor unguarded)"},

    # Legacy compatibility (pre-patch output still uses mem[])
    {"id": "MEM_READ_WRITE", "regex": r"(?:mem|MEMORY)\[t\d+\]\s*=.*\n.*(?:mem|MEMORY)\[",
     "severity": "MEDIUM", "weight": 1,
     "desc": "Memory read after write sequence (legacy pattern)"},
]


def score_contract(content: str, fname: str) -> dict:
    """Score a single decompiled C contract file."""
    findings = []
    total_score = 0

    for pat in PATTERNS:
        if re.search(pat["regex"], content, re.IGNORECASE):
            findings.append({
                "id":       pat["id"],
                "severity": pat["severity"],
                "weight":   pat["weight"],
                "desc":     pat["desc"]
            })
            total_score += pat["weight"]

    # Complexity bonus
    complexity_bonus = len(content) // 1000
    total_score += complexity_bonus

    # ABI function count
    abi_funcs = content.count("function func_0x")
    mem_ops   = content.count("mem[")

    # Regime classification (Hamiltonian-aligned)
    if total_score >= 15:
        regime = "CRITICAL"
    elif total_score >= 7:
        regime = "HIGH"
    elif total_score >= 3:
        regime = "MEDIUM"
    elif total_score > 0:
        regime = "LOW"
    else:
        regime = "CLEAN"

    return {
        "contract":         fname,
        "score":            total_score,
        "regime":           regime,
        "findings":         findings,
        "abi_functions":    abi_funcs,
        "memory_ops":       mem_ops,
        "complexity_bonus": complexity_bonus,
        "char_count":       len(content),
        "ts":               datetime.utcnow().isoformat() + "Z"
    }


def generate_mega_report(corpus_dir, decomp_dir, output_html):
    corpus_jsonl = os.path.join(os.path.dirname(output_html), "mega_scored_corpus.jsonl")

    stats = {
        "total_contracts":   0,
        "successful_decomp": 0,
        "scored":            0,
        "abi_functions":     0,
        "memory_ops":        0,
        "regime_counts":     Counter(),
        "finding_counts":    Counter(),
    }

    all_scored = []

    if not os.path.exists(decomp_dir):
        os.makedirs(decomp_dir)

    for fname in sorted(os.listdir(decomp_dir)):
        if not fname.endswith(".c"):
            continue
        stats["total_contracts"] += 1
        path = os.path.join(decomp_dir, fname)

        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                content = f.read()
        except Exception:
            continue

        stats["successful_decomp"] += 1

        rec = score_contract(content, fname)
        all_scored.append(rec)
        stats["scored"]        += 1
        stats["abi_functions"] += rec["abi_functions"]
        stats["memory_ops"]    += rec["memory_ops"]
        stats["regime_counts"][rec["regime"]] += 1
        for finding in rec["findings"]:
            stats["finding_counts"][finding["id"]] += 1

    # Sort descending by score
    all_scored.sort(key=lambda r: r["score"], reverse=True)

    # ── Write JSONL corpus ────────────────────────────────────────────────
    with open(corpus_jsonl, "w") as jf:
        for rec in all_scored:
            jf.write(json.dumps(rec) + "\n")

    # ── Write HTML Report ─────────────────────────────────────────────────
    regime_colors = {
        "CRITICAL": "#ff2244", "HIGH": "#ff8800",
        "MEDIUM":   "#ffcc00", "LOW":  "#00ccff", "CLEAN": "#00ff88"
    }

    rows = ""
    for rec in all_scored[:500]:   # cap at 500 rows for browser sanity
        color = regime_colors.get(rec["regime"], "#ffffff")
        finding_ids = ", ".join(f["id"] for f in rec["findings"]) or "—"
        rows += (
            f"<tr>"
            f"<td><a href='mega_decomp/{rec['contract']}'>{rec['contract']}</a></td>"
            f"<td style='color:{color};font-weight:bold'>{rec['regime']}</td>"
            f"<td>{rec['score']}</td>"
            f"<td>{finding_ids}</td>"
            f"<td>{rec['abi_functions']}</td>"
            f"<td>{rec['memory_ops']}</td>"
            f"</tr>\n"
        )

    regime_summary = " | ".join(
        f"<span style='color:{regime_colors[r]}'>{r}: {c}</span>"
        for r, c in sorted(stats["regime_counts"].items())
    )

    html = f"""<!DOCTYPE html>
<html>
<head>
<title>ZCC MEGA SWARM — 50k Scored Report</title>
<style>
  body {{font-family:monospace;background:#0b0c0f;color:#ccc;padding:20px}}
  h1   {{color:#00ffff}}
  h2   {{color:#ff00ff}}
  table{{border-collapse:collapse;width:100%}}
  th   {{background:#1a1a2e;color:#00ffff;padding:8px;text-align:left}}
  td   {{padding:5px 8px;border-bottom:1px solid #222}}
  tr:hover{{background:#111}}
  a    {{color:#00ffcc}}
  .stat{{background:#111;padding:12px;margin:4px;display:inline-block;border:1px solid #333}}
</style>
</head>
<body>
<h1>🔱 ZKAEDI MEGA SWARM — Scored Report</h1>
<p>{datetime.now().strftime('%Y-%m-%d %H:%M UTC')}</p>

<div>
  <span class="stat">📄 Contracts: <strong>{stats['total_contracts']:,}</strong></span>
  <span class="stat">✅ Decompiled: <strong>{stats['successful_decomp']:,}</strong></span>
  <span class="stat">🔍 Scored: <strong>{stats['scored']:,}</strong></span>
  <span class="stat">⚡ ABI funcs: <strong>{stats['abi_functions']:,}</strong></span>
  <span class="stat">💾 Mem ops: <strong>{stats['memory_ops']:,}</strong></span>
</div>

<h2>Regime Distribution</h2>
<p>{regime_summary}</p>

<h2>Top Finding Types</h2>
<p>{' | '.join(f"{k}: {v}" for k, v in stats['finding_counts'].most_common(10))}</p>

<h2>Top 500 Scored Contracts</h2>
<table>
<thead>
  <tr><th>Contract</th><th>Regime</th><th>Score</th>
      <th>Findings</th><th>ABI Funcs</th><th>Mem Ops</th></tr>
</thead>
<tbody>
{rows}
</tbody>
</table>

<p style='color:#555;margin-top:20px'>
  Full corpus: <a href='mega_scored_corpus.jsonl'>mega_scored_corpus.jsonl</a>
  ({stats['scored']:,} records)
</p>
</body>
</html>"""

    with open(output_html, "w", encoding="utf-8") as f:
        f.write(html)

    # ── Console Summary ───────────────────────────────────────────────────
    print(f"[OK] MEGA SWARM SCORED -- {stats['scored']:,} contracts")
    print(f"   Regime breakdown:")
    for regime, cnt in sorted(stats["regime_counts"].items()):
        print(f"     {regime:8s}: {cnt:6,}")
    print(f"   Top findings: {stats['finding_counts'].most_common(5)}")
    print(f"   JSONL corpus: {corpus_jsonl}")
    print(f"   HTML report:  {output_html}")
    return stats


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: mega_report.py <corpus_dir> <decomp_dir> <output_html>")
        sys.exit(1)
    generate_mega_report(sys.argv[1], sys.argv[2], sys.argv[3])
