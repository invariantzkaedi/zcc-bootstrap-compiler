#!/usr/bin/env python3
"""
ir_analysis_report.py — ZCC IR analysis via Ollama.
PATCHED: PRIME-powered function prioritization replaces naive size sort.

Original: top-50 by line count (largest first)
Patched:  ir_prime_ranker.rank_ir_functions() — topology-adaptive PRIME ranking
          HOT tier functions (highest optimization potential) analyzed first.

One-line change at line ~144:
  BEFORE: eligible.sort(key=lambda x: len(x[1]), reverse=True)
  AFTER:  eligible = prime_eligible(funcs, top=TOP, max_func_lines=MAX_FUNC_LINES)
          (or graceful fallback to size sort if PRIME unavailable)
"""

import sys
import re
import subprocess
import time
from pathlib import Path

MODEL = "zkaedi-ir-telemetry"   # or llama3.2:3b for faster runs
MAX_FUNC_LINES = 2000           # skip giants (sqlite3VdbeExec=61503 lines)
MAX_LINES = 150                 # truncate per function for prompt budget
TOP = 50                        # functions to analyze per run
TIMEOUT = 90                    # seconds per function
OUT = "report.md"

SYSTEM = ("You are a ZCC IR expert. Analyze this intermediate representation. "
          "Registers=%tN, stack=%stack_-N, labels=.LN. "
          "Analyze for: optimizations, dead code, correctness issues. "
          "Be concise and specific.")


# ── PRIME integration (graceful: falls back to size sort if unavailable) ────
def _try_import_prime():
    """Try to import ir_prime_ranker from the tools directory or PYTHONPATH."""
    import importlib.util, os
    # Search order: same directory as this script, then enhanced-ui
    search_dirs = [
        str(Path(__file__).parent),
        r'h:\agents\enhanced-ui',
        r'H:\agents\enhanced-ui',
    ]
    for d in search_dirs:
        p = Path(d) / 'ir_prime_ranker.py'
        if p.exists():
            spec = importlib.util.spec_from_file_location('ir_prime_ranker', str(p))
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
    # Try normal import (PYTHONPATH)
    try:
        import ir_prime_ranker
        return ir_prime_ranker
    except ImportError:
        return None

_ranker = _try_import_prime()

def _prime_eligible(funcs):
    """
    PRIME-ranked replacement for the naive size sort.
    Returns [(name, lines), ...] sorted HOT→WARM→COLD, then by energy.
    Falls back to size sort if PRIME unavailable.
    """
    if _ranker is None:
        print("[ir_analysis_report] PRIME unavailable — using size sort fallback.",
              file=sys.stderr)
        eligible = [(n, l) for n, l in funcs.items() if len(l) <= MAX_FUNC_LINES]
        eligible.sort(key=lambda x: len(x[1]), reverse=True)
        return eligible[:TOP]

    print(f"[ir_analysis_report] PRIME ranking {len(funcs)} functions...",
          file=sys.stderr)
    ranked = _ranker.rank_ir_functions(
        funcs,
        top=TOP,
        max_func_lines=MAX_FUNC_LINES,
        iters=2000,
        seed=42,
        verbose=True,
    )
    # Return (name, lines) tuples — same shape as original eligible list
    return [(name, lines) for name, lines, meta in ranked]


# ── Original functions (unchanged) ──────────────────────────────────────────

def norm(ir):
    """Deterministic IR normalization — strips variable names, preserves structure."""
    for p, r in [
        (r'%t\d+',       '%tN'),
        (r'%stack_-\d+', '%stack_-N'),
        (r'\.L\d+',      '.LN'),
        (r'imm=\d+',     'imm=N'),
        (r'; line \d+',  '; line N'),
    ]:
        ir = re.sub(p, r, ir)
    return ir


def parse_functions(text):
    """Parse ZCC IR text → dict of { func_name: [lines] }."""
    funcs, cur, lines = {}, None, []
    for line in text.splitlines():
        m = re.match(r';\s*func\s+(\S+)\s*->', line)
        if m:
            if cur:
                funcs[cur] = lines
            cur, lines = m.group(1), [line]
        elif cur:
            lines.append(line)
    if cur:
        funcs[cur] = lines
    return funcs


def run(ir_file=None):
    text = Path(ir_file).read_text() if ir_file else sys.stdin.read()
    funcs = parse_functions(text)
    print(f"Found {len(funcs)} functions", file=sys.stderr)

    # ── PATCHED: PRIME ranking replaces naive size sort ──────────────────────
    # ORIGINAL (3 lines):
    #   eligible = [(n, l) for n, l in funcs.items() if len(l) <= MAX_FUNC_LINES]
    #   eligible.sort(key=lambda x: len(x[1]), reverse=True)
    #   targets = eligible[:TOP]
    #
    # REPLACEMENT (1 call):
    targets = _prime_eligible(funcs)
    # ── END PATCH ────────────────────────────────────────────────────────────

    print(f"Analyzing {len(targets)} functions (PRIME-ranked, ≤{MAX_FUNC_LINES} lines each)",
          file=sys.stderr)

    report = ["# ZCC IR Analysis Report\n"]
    ok = 0

    # Restart ollama before batch to avoid stale state
    subprocess.run(["ollama", "stop", MODEL], capture_output=True)
    time.sleep(2)

    for i, (fname, flines) in enumerate(targets):
        print(f"\n[{i+1}/{len(targets)}] {fname} ({len(flines)} lines)",
              file=sys.stderr)
        ir = norm("\n".join(flines))
        # Truncate if still too long
        if len(ir.splitlines()) > MAX_LINES:
            ir = "\n".join(ir.splitlines()[:MAX_LINES]) + "\n  ; ..."
        prompt = f"{SYSTEM}\n\n{ir}"
        t0 = time.time()
        try:
            r = subprocess.run(["ollama", "run", MODEL, prompt],
                               capture_output=True, text=True, timeout=TIMEOUT)
            elapsed = time.time() - t0
            if r.returncode != 0:
                report.append(f"## {fname} ({len(flines)} nodes)\n\n**ERROR**\n\n---\n")
                continue
            report.append(f"## {fname} ({len(flines)} nodes)\n\n{r.stdout.strip()}\n\n---\n")
            ok += 1
            print(f"  Done in {elapsed:.1f}s", file=sys.stderr)
        except subprocess.TimeoutExpired:
            print(f"  TIMEOUT", file=sys.stderr)
            report.append(f"## {fname} ({len(flines)} nodes)\n\n**TIMEOUT**\n\n---\n")

    Path(OUT).write_text("\n".join(report))
    print(f"\nDone: {ok}/{len(targets)} | Report: {OUT}", file=sys.stderr)


if __name__ == "__main__":
    run(sys.argv[1] if len(sys.argv) > 1 else None)
