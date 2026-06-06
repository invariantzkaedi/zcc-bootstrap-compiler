#!/usr/bin/env python3
"""
report_to_ledger.py — Parse zcc_analyze.py markdown report into repair ledger

Reads report.md output from zcc_analyze.py and appends structured findings
to H:\agents\zcc_repair_ledger.jsonl

Usage:
    python3 report_to_ledger.py --report report.md
    python3 report_to_ledger.py --report report.md --ledger /mnt/h/agents/zcc_repair_ledger.jsonl
    python3 report_to_ledger.py --report report.md --dry-run
"""

import argparse, json, re, sys
from datetime import datetime, timezone
from pathlib import Path

LEDGER_DEFAULT = "/mnt/h/agents/zcc_repair_ledger.jsonl"

# Severity keywords → priority
CRITICAL_KEYWORDS = [
    "buffer overflow", "out-of-bounds", "undefined behavior", "memory corruption",
    "null pointer", "use after free", "segfault", "type mismatch"
]
HIGH_KEYWORDS = [
    "dead code", "unreachable", "uninitialized", "correctness",
    "missing type", "potential crash", "memory access"
]
OPT_KEYWORDS = [
    "constant propagation", "constant folding", "dead code elimination",
    "redundant load", "common subexpression", "loop unrolling",
    "register allocation", "branch prediction", "loop invariant"
]

def classify_finding(text):
    text_lower = text.lower()
    if any(k in text_lower for k in CRITICAL_KEYWORDS):
        return "CRITICAL"
    if any(k in text_lower for k in HIGH_KEYWORDS):
        return "HIGH"
    if any(k in text_lower for k in OPT_KEYWORDS):
        return "OPT"
    return "INFO"

def extract_bullet_points(section_text):
    """Extract bullet points from a markdown section."""
    bullets = []
    for line in section_text.splitlines():
        line = line.strip()
        # Match *, -, or numbered bullets
        m = re.match(r'^[\*\-\d\.]+\s+\*{0,2}(.+?)\*{0,2}:?\s*(.*)', line)
        if m:
            title = m.group(1).strip()
            detail = m.group(2).strip()
            text = f"{title}: {detail}".strip(': ')
            bullets.append(text)
    return bullets

def parse_report(report_text):
    """Parse markdown report into list of function analysis dicts."""
    results = []

    # Split by function headers: ## funcname (N nodes) or ## funcname
    func_blocks = re.split(r'\n## ', report_text)

    for block in func_blocks[1:]:  # skip header
        lines = block.strip().splitlines()
        if not lines:
            continue

        # Extract function name and node count
        header = lines[0].strip()
        m = re.match(r'(\S+)\s*\((\d+)\s*nodes?\)', header)
        if m:
            func_name = m.group(1)
            node_count = int(m.group(2))
        else:
            func_name = header.split()[0] if header.split() else "unknown"
            node_count = 0

        body = "\n".join(lines[1:])

        # Extract sections
        sections = {}
        section_pattern = re.split(r'\n#+\s+', body)
        current_sections = re.findall(r'\n#+\s+([^\n]+)', body)

        # Parse each named section
        opt_text = ""
        dead_text = ""
        correctness_text = ""

        for i, sec_title in enumerate(current_sections):
            sec_body = section_pattern[i + 1] if i + 1 < len(section_pattern) else ""
            tl = sec_title.lower()
            if "optim" in tl:
                opt_text += sec_body
            elif "dead" in tl:
                dead_text += sec_body
            elif "correct" in tl or "issue" in tl:
                correctness_text += sec_body

        # Build findings list
        findings = []

        for bullet in extract_bullet_points(opt_text):
            findings.append({
                "category": "OPTIMIZATION",
                "severity": "OPT",
                "finding": bullet[:300],
            })

        for bullet in extract_bullet_points(dead_text):
            findings.append({
                "category": "DEAD_CODE",
                "severity": "HIGH",
                "finding": bullet[:300],
            })

        for bullet in extract_bullet_points(correctness_text):
            sev = classify_finding(bullet)
            findings.append({
                "category": "CORRECTNESS",
                "severity": sev,
                "finding": bullet[:300],
            })

        # If no bullets parsed, store the raw analysis as a single finding
        if not findings and body.strip():
            findings.append({
                "category": "ANALYSIS",
                "severity": "INFO",
                "finding": body.strip()[:500],
            })

        results.append({
            "func":      func_name,
            "nodes":     node_count,
            "findings":  findings,
            "raw":       body.strip()[:1000],
        })

    return results

def load_existing_ledger(ledger_path):
    """Load existing ledger entries to avoid duplicates."""
    existing = set()
    if Path(ledger_path).exists():
        with open(ledger_path, encoding="utf-8") as f:
            for line in f:
                try:
                    r = json.loads(line)
                    existing.add(r.get("func", "") + r.get("finding", "")[:50])
                except:
                    pass
    return existing

def main():
    p = argparse.ArgumentParser(description="Parse zcc_analyze report into repair ledger")
    p.add_argument("--report",  required=True, help="Path to report.md from zcc_analyze.py")
    p.add_argument("--ledger",  default=LEDGER_DEFAULT, help=f"Ledger path (default: {LEDGER_DEFAULT})")
    p.add_argument("--source",  default="zcc_analyze", help="Source tag for ledger entries")
    p.add_argument("--dry-run", action="store_true", help="Print entries without writing")
    p.add_argument("--verbose", "-v", action="store_true")
    args = p.parse_args()

    report_text = Path(args.report).read_text(encoding="utf-8")
    results = parse_report(report_text)

    if not results:
        print("No function analyses found in report.", file=sys.stderr)
        sys.exit(1)

    print(f"Parsed {len(results)} functions from report", file=sys.stderr)

    existing = load_existing_ledger(args.ledger) if not args.dry_run else set()
    ts = datetime.now(timezone.utc).isoformat()

    written = 0
    skipped = 0
    entries = []

    for result in results:
        func = result["func"]
        for finding in result["findings"]:
            dedup_key = func + finding["finding"][:50]
            if dedup_key in existing:
                skipped += 1
                continue

            entry = {
                "ts":       ts,
                "source":   args.source,
                "func":     func,
                "nodes":    result["nodes"],
                "category": finding["category"],
                "severity": finding["severity"],
                "finding":  finding["finding"],
                "status":   "OPEN",
            }
            entries.append(entry)
            existing.add(dedup_key)
            written += 1

            if args.verbose or args.dry_run:
                print(f"  [{finding['severity']:8}] {func}: {finding['finding'][:80]}")

    if args.dry_run:
        print(f"\nDry run: {written} entries would be written, {skipped} skipped")
        return

    with open(args.ledger, "a", encoding="utf-8") as f:
        for entry in entries:
            f.write(json.dumps(entry) + "\n")

    print(f"Ledger: {written} entries written, {skipped} duplicates skipped")
    print(f"Ledger path: {args.ledger}")

    # Summary by severity
    from collections import Counter
    sev_counts = Counter(e["severity"] for e in entries)
    print(f"Severity breakdown: {dict(sev_counts)}")

if __name__ == "__main__":
    main()
