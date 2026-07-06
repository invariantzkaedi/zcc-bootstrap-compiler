#!/usr/bin/env python3
import json
import os
from pathlib import Path
from datetime import datetime, timezone

art = Path("artifacts")
art.mkdir(parents=True, exist_ok=True)

summary_path = os.environ.get("GITHUB_STEP_SUMMARY", "")
idx_path = art / "index.json"
mut_path = art / "mutation_report.json"
perf_path = art / "perf_timeseries.json"

lines = []
lines.append("## 🔱 QEC Verification Summary")
lines.append("")
lines.append(f"- Generated (UTC): {datetime.now(timezone.utc).isoformat()}")
lines.append(f"- Artifacts dir: `{art}`")
lines.append("")

if idx_path.exists():
    try:
        idx = json.loads(idx_path.read_text(encoding='utf-8'))
        failures = idx.get("failures", [])
        lines.append("### Failure Signatures")
        lines.append(f"- Total failures: **{idx.get('count', 0)}**")
        lines.append(f"- Unique signatures: **{idx.get('unique_signatures', 0)}**")
        lines.append("")
        lines.append("| Seed | Type | Signature | Min Ratio |")
        lines.append("|---:|---|---|---:|")
        for f in failures[:20]:
            o = f.get("original_length") or 0
            m = f.get("minimized_length") or 0
            ratio = f"{m}/{o}" if o else "-"
            sig = (f.get("signature") or "")[:16]
            lines.append(f"| {f.get('seed','-')} | {f.get('failure_type','-')} | `{sig}` | {ratio} |")
    except Exception:
        pass
else:
    lines.append("### Failure Signatures")
    lines.append("- No `artifacts/index.json` found.")
lines.append("")

if mut_path.exists():
    try:
        m = json.loads(mut_path.read_text(encoding='utf-8'))
        lines.append("### Mutation")
        lines.append(f"- Total mutants: **{m.get('total_mutants','-')}**")
        lines.append(f"- Killed: **{m.get('killed','-')}**")
        lines.append(f"- Survived: **{m.get('survived','-')}**")
        lines.append(f"- Kill rate: **{m.get('kill_rate','-')}**")
        lines.append("")
    except Exception:
        pass

if perf_path.exists():
    try:
        p = json.loads(perf_path.read_text(encoding='utf-8'))
        lines.append("### Performance")
        lines.append(f"- Fast suite seconds: **{p.get('fast_suite_seconds','-')}**")
        lines.append(f"- Deep suite seconds: **{p.get('deep_suite_seconds','-')}**")
        lines.append(f"- Regression flag: **{p.get('regression','-')}**")
        lines.append("")
    except Exception:
        pass

# Determinism stamp
det_file = art / "determinism_hashes.json"
if det_file.exists():
    try:
        d = json.loads(det_file.read_text(encoding='utf-8'))
        uniq = len(set(d.get("hashes", [])))
        lines.append("### Determinism")
        lines.append(f"- Runs compared: **{len(d.get('hashes', []))}**")
        lines.append(f"- Unique hashes: **{uniq}**")
        lines.append(f"- Status: **{'PASS' if uniq == 1 else 'FAIL'}**")
        lines.append("")
    except Exception:
        pass

md = "\n".join(lines) + "\n"
(art / "summary.md").write_text(md, encoding='utf-8')

if summary_path:
    try:
        Path(summary_path).write_text(md, encoding='utf-8')
    except Exception:
        pass
print("Wrote artifacts/summary.md and GITHUB_STEP_SUMMARY (if available)")
