#!/usr/bin/env python3
"""
ir_prime_ranker.py — PRIME-powered function prioritizer for ZCC IR optimizer.

Replaces the naive "sort by line count, take top 50" selection in
ir_analysis_report.py with topology-adaptive Wilson-Fisher PRIME ranking.

HOW IT SLOTS IN:
    1. ir_analysis_report.py parses IR → funcs dict
    2. THIS FILE ranks funcs using ir_prime_pass (topology-adaptive η)
    3. Ollama analysis runs on the HOT tier first, then WARM if budget allows
    4. report_to_ledger.py ingests as before

IMPACT:
    Before: top-50 by line count  → biased toward giant fns, misses hot small fns
    After:  PRIME topology-aware  → ranks by actual optimization potential,
            accounting for call graph coupling + branch density + size

STANDALONE USAGE:
    python3 ir_prime_ranker.py <ir_file.ir> [--top 50] [--tier HOT]

PROGRAMMATIC USAGE:
    from ir_prime_ranker import rank_ir_functions, parse_functions_to_dicts
    ranked = rank_ir_functions(funcs_dict, top=50)

REQUIRES: numpy (already in ZCC venv), wf_extended_custom.py on PYTHONPATH
    OR run with: PYTHONPATH=h:/agents/enhanced-ui python3 ir_prime_ranker.py
"""

import sys
import re
import json
import argparse
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ── Try to import PRIME; graceful fallback to size-sort if unavailable ──────
try:
    sys.path.insert(0, str(Path(__file__).parent))
    # Try local directory first (copy wf_extended_custom.py next to this file)
    from wf_extended_custom import ir_prime_pass, ir_dependency_matrix, ir_hamiltonian_field
    import numpy as np
    PRIME_AVAILABLE = True
except ImportError:
    try:
        # Try enhanced-ui path (Windows PYTHONPATH)
        sys.path.insert(0, r'h:\agents\enhanced-ui')
        from wf_extended_custom import ir_prime_pass, ir_dependency_matrix, ir_hamiltonian_field
        import numpy as np
        PRIME_AVAILABLE = True
    except ImportError:
        PRIME_AVAILABLE = False
        print("[ir_prime_ranker] WARNING: wf_extended_custom not found — "
              "falling back to size-sort. Add h:\\agents\\enhanced-ui to PYTHONPATH.",
              file=sys.stderr)


# ── IR parsing ───────────────────────────────────────────────────────────────

def parse_functions(text: str) -> Dict[str, List[str]]:
    """Parse ZCC IR text → {func_name: [lines]}. Identical to ir_analysis_report.py."""
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


def extract_function_metadata(name: str, lines: List[str]) -> Dict:
    """
    Extract PRIME-relevant metrics from ZCC IR function lines.

    Counts:
        inst_count:    total IR instructions
        label_count:   .LN label definitions (control flow nodes)
        write_count:   STORE + assignment instructions
        branch_count:  JMP/JE/JNE/JL/JG/JLE/JGE/JZ/JNZ/BR instructions
        call_depth:    number of CALL instructions (proxy for call depth)
        calls:         list of called function names (for dependency graph)
        reads:         globals/memory locations read
        writes:        globals/memory locations written
    """
    inst_count   = 0
    label_count  = 0
    write_count  = 0
    branch_count = 0
    call_depth   = 0
    calls        = []
    reads        = []
    writes       = []

    # Instruction patterns
    BRANCH_OPS  = re.compile(r'\b(JMP|JE|JNE|JL|JG|JLE|JGE|JZ|JNZ|BR|JUMP|jmp|je|jne|jl|jg)\b')
    CALL_PAT    = re.compile(r'\bCALL\s+(\S+)', re.IGNORECASE)
    STORE_PAT   = re.compile(r'\bSTORE\b', re.IGNORECASE)
    LABEL_PAT   = re.compile(r'^\.L\w+:')
    ASSIGN_PAT  = re.compile(r'%t\d+\s*=')
    LOAD_PAT    = re.compile(r'\bLOAD\b.*%stack_-(\d+)', re.IGNORECASE)
    STORE_ADDR  = re.compile(r'\bSTORE\b.*%stack_-(\d+)', re.IGNORECASE)

    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith(';'):
            continue

        inst_count += 1

        if LABEL_PAT.match(stripped):
            label_count += 1
            continue

        if BRANCH_OPS.search(stripped):
            branch_count += 1

        call_match = CALL_PAT.search(stripped)
        if call_match:
            call_depth += 1
            callee = call_match.group(1).strip('(),')
            # Filter out register/temp names
            if callee and not callee.startswith('%') and not callee.startswith('.'):
                calls.append(callee)

        if STORE_PAT.search(stripped):
            write_count += 1
            m = STORE_ADDR.search(stripped)
            if m:
                writes.append(f'slot_{m.group(1)}')
        elif ASSIGN_PAT.search(stripped):
            write_count += 1

        m = LOAD_PAT.search(stripped)
        if m:
            reads.append(f'slot_{m.group(1)}')

    return {
        'name':         name,
        'inst_count':   max(inst_count, 1),
        'label_count':  label_count,
        'write_count':  max(write_count, 1),
        'branch_count': branch_count,
        'call_depth':   call_depth,
        'calls':        calls,
        'reads':        reads,
        'writes':       writes,
        'line_count':   len(lines),
    }


def parse_functions_to_dicts(funcs: Dict[str, List[str]],
                              max_func_lines: int = 2000) -> List[Dict]:
    """
    Convert {name: lines} → list of metadata dicts for ir_prime_pass.
    Skips giant functions (> max_func_lines) — same cap as ir_analysis_report.py.
    """
    result = []
    skipped_giants = []
    for name, lines in funcs.items():
        if len(lines) > max_func_lines:
            skipped_giants.append((name, len(lines)))
            continue
        result.append(extract_function_metadata(name, lines))

    if skipped_giants:
        print(f"[ir_prime_ranker] Skipped {len(skipped_giants)} giant functions "
              f"(>{max_func_lines} lines): "
              f"{', '.join(n for n,_ in skipped_giants[:3])}"
              f"{'...' if len(skipped_giants) > 3 else ''}",
              file=sys.stderr)
    return result


# ── PRIME ranking ────────────────────────────────────────────────────────────

def rank_ir_functions(
    funcs: Dict[str, List[str]],
    top: int = 50,
    max_func_lines: int = 2000,
    tier_filter: Optional[str] = None,
    eta: Optional[float] = None,
    iters: int = 2000,
    seed: int = 42,
    verbose: bool = True,
) -> List[Tuple[str, List[str], Dict]]:
    """
    Rank IR functions by PRIME optimization potential.

    Returns list of (name, lines, metadata) tuples sorted HOT→WARM→COLD,
    then by PRIME energy within each tier.

    Args:
        funcs:          {name: lines} from parse_functions()
        top:            maximum functions to return
        max_func_lines: skip functions larger than this
        tier_filter:    if set ('HOT'|'WARM'|'COLD'), return only that tier
        eta:            override PRIME coupling (default: topology_eta_search)
        iters:          PRIME iterations per run
        seed:           RNG seed
        verbose:        print ranking summary to stderr

    Returns:
        [(name, lines, {'energy', 'tier', 'inst_count', ...}), ...]
        In the same format ir_analysis_report.py expects for (fname, flines).
    """
    fn_dicts = parse_functions_to_dicts(funcs, max_func_lines)

    if not fn_dicts:
        if verbose:
            print("[ir_prime_ranker] No eligible functions after filtering.", file=sys.stderr)
        return []

    if not PRIME_AVAILABLE:
        # Fallback: size sort (original behavior)
        if verbose:
            print("[ir_prime_ranker] Fallback: size sort.", file=sys.stderr)
        eligible = [(n, l) for n, l in funcs.items() if len(l) <= max_func_lines]
        eligible.sort(key=lambda x: len(x[1]), reverse=True)
        return [(n, l, {'energy': -len(l), 'tier': 'HOT', 'inst_count': len(l)})
                for n, l in eligible[:top]]

    # ── Run PRIME ──
    if verbose:
        print(f"[ir_prime_ranker] Running PRIME on {len(fn_dicts)} functions "
              f"(iters={iters}, eta={'auto' if eta is None else f'{eta:.3f}'})...",
              file=sys.stderr)

    result = ir_prime_pass(fn_dicts, eta=eta or 0.44, iters=iters, seed=seed)

    if verbose:
        print(f"[ir_prime_ranker] Global tier={result['tier']} "
              f"F={result['free_energy']:.2f} "
              f"η_used={eta or 0.44:.3f}",
              file=sys.stderr)
        print(f"[ir_prime_ranker] HOT={len(result['hot_functions'])} "
              f"WARM={len(result['warm_functions'])} "
              f"COLD={len(result['cold_functions'])}",
              file=sys.stderr)

    # Build output: (name, lines, metadata) triples
    name_to_lines = funcs
    name_to_meta  = {r[0]: {'energy': r[1], 'tier': r[2]} for r in result['ranked']}

    # Also attach inst_count from fn_dicts
    name_to_dict  = {d['name']: d for d in fn_dicts}

    output = []
    tier_order = {'HOT': 0, 'WARM': 1, 'COLD': 2}

    for name, energy, tier in result['ranked']:
        if tier_filter and tier != tier_filter:
            continue
        if name not in name_to_lines:
            continue
        meta = {
            'energy':      energy,
            'tier':        tier,
            'inst_count':  name_to_dict.get(name, {}).get('inst_count', 0),
            'line_count':  len(name_to_lines[name]),
            'call_depth':  name_to_dict.get(name, {}).get('call_depth', 0),
            'branch_count':name_to_dict.get(name, {}).get('branch_count', 0),
        }
        output.append((name, name_to_lines[name], meta))

    # Sort: HOT first, then by energy ascending (most negative = hottest)
    output.sort(key=lambda x: (tier_order.get(x[2]['tier'], 2), x[2]['energy']))

    if verbose:
        print(f"[ir_prime_ranker] Returning top {min(top, len(output))} functions.",
              file=sys.stderr)
        if output:
            top3 = output[:3]
            print(f"[ir_prime_ranker] Top 3:", file=sys.stderr)
            for name, _, meta in top3:
                print(f"  {name:40s} tier={meta['tier']} "
                      f"E={meta['energy']:8.4f} "
                      f"inst={meta['inst_count']}",
                      file=sys.stderr)

    return output[:top]


# ── Drop-in replacement for ir_analysis_report.py eligible list ──────────────

def prime_eligible(
    funcs: Dict[str, List[str]],
    top: int = 50,
    max_func_lines: int = 2000,
) -> List[Tuple[str, List[str]]]:
    """
    Drop-in replacement for the `eligible` sort in ir_analysis_report.py.

    Original code:
        eligible = [(n, l) for n, l in funcs.items() if len(l) <= MAX_FUNC_LINES]
        eligible.sort(key=lambda x: len(x[1]), reverse=True)
        targets = eligible[:TOP]

    Replacement:
        targets = prime_eligible(funcs, top=TOP, max_func_lines=MAX_FUNC_LINES)

    Returns list of (name, lines) — same shape as original `targets`.
    """
    ranked = rank_ir_functions(funcs, top=top, max_func_lines=max_func_lines)
    return [(name, lines) for name, lines, _ in ranked]


# ── Tier distribution analysis ───────────────────────────────────────────────

def tier_distribution(
    funcs: Dict[str, List[str]],
    max_func_lines: int = 2000,
    eta: Optional[float] = None,
    iters: int = 2000,
    seed: int = 42,
) -> Dict:
    """
    Run PRIME on all eligible functions and return tier distribution stats.

    Use this to measure the impact of topology-adaptive η on the dataset.
    Compare against the zcc-ir-prime-v2 expected distribution:
        Expected: 519 LEGENDARY / 350 EPIC / 529 RARE / 650 UNCOMMON / 547 COMMON

    Returns:
        {
            'total':          int,
            'eta_used':       float,
            'free_energy':    float,
            'global_tier':    str,
            'tiers':          {'HOT': int, 'WARM': int, 'COLD': int},
            'hot_functions':  [str, ...],
            'warm_functions': [str, ...],
            'size_vs_prime':  [{'name', 'size_rank', 'prime_rank', 'delta'}, ...],
        }
    """
    fn_dicts = parse_functions_to_dicts(funcs, max_func_lines)
    if not fn_dicts or not PRIME_AVAILABLE:
        return {'error': 'PRIME unavailable or no eligible functions'}

    result = ir_prime_pass(fn_dicts, eta=eta or 0.44, iters=iters, seed=seed)

    # Build size ranking for comparison
    name_to_lines  = {d['name']: d['line_count'] for d in fn_dicts}
    size_ranked    = sorted(name_to_lines.items(), key=lambda x: -x[1])
    size_rank_map  = {name: i+1 for i, (name, _) in enumerate(size_ranked)}

    prime_ranked   = result['ranked']  # [(name, energy, tier), ...]
    prime_rank_map = {name: i+1 for i, (name, _, _) in enumerate(prime_ranked)}

    # Functions where PRIME rank differs significantly from size rank
    size_vs_prime = []
    for name, _, tier in prime_ranked:
        sr = size_rank_map.get(name, 0)
        pr = prime_rank_map.get(name, 0)
        delta = sr - pr  # positive = PRIME ranks it higher than size does
        if abs(delta) > len(fn_dicts) * 0.1:  # >10% rank shift
            size_vs_prime.append({
                'name':        name,
                'tier':        tier,
                'size_rank':   sr,
                'prime_rank':  pr,
                'delta':       delta,
                'note':        'PRIME ranks higher' if delta > 0 else 'PRIME ranks lower',
            })
    size_vs_prime.sort(key=lambda x: -abs(x['delta']))

    return {
        'total':          len(fn_dicts),
        'eta_used':       eta or 0.44,
        'free_energy':    result['free_energy'],
        'global_tier':    result['tier'],
        'tiers': {
            'HOT':  len(result['hot_functions']),
            'WARM': len(result['warm_functions']),
            'COLD': len(result['cold_functions']),
        },
        'hot_functions':  result['hot_functions'],
        'warm_functions': result['warm_functions'],
        'size_vs_prime':  size_vs_prime[:20],  # top 20 rank disagreements
    }


# ── CLI ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='PRIME-rank ZCC IR functions for optimization prioritization.'
    )
    parser.add_argument('ir_file', nargs='?',
                        help='ZCC IR file (default: stdin)')
    parser.add_argument('--top', type=int, default=50,
                        help='Number of functions to return (default: 50)')
    parser.add_argument('--tier', choices=['HOT', 'WARM', 'COLD'],
                        help='Filter to specific tier only')
    parser.add_argument('--eta', type=float, default=None,
                        help='Override PRIME coupling constant (default: 0.44)')
    parser.add_argument('--iters', type=int, default=2000,
                        help='PRIME iterations (default: 2000)')
    parser.add_argument('--max-lines', type=int, default=2000,
                        help='Skip functions larger than this (default: 2000)')
    parser.add_argument('--distribution', action='store_true',
                        help='Show tier distribution stats and exit')
    parser.add_argument('--json', action='store_true',
                        help='Output JSON instead of human-readable')
    parser.add_argument('--quiet', action='store_true',
                        help='Suppress progress output')
    args = parser.parse_args()

    # Read IR
    if args.ir_file:
        text = Path(args.ir_file).read_text()
    else:
        text = sys.stdin.read()

    funcs = parse_functions(text)
    print(f"[ir_prime_ranker] Parsed {len(funcs)} functions from IR.",
          file=sys.stderr)

    if args.distribution:
        dist = tier_distribution(funcs, max_func_lines=args.max_lines,
                                  eta=args.eta, iters=args.iters)
        if args.json:
            print(json.dumps(dist, indent=2))
        else:
            print(f"\n{'═'*56}")
            print(f"  IR PRIME TIER DISTRIBUTION")
            print(f"{'─'*56}")
            print(f"  Total eligible : {dist.get('total', 0)}")
            print(f"  η used         : {dist.get('eta_used', 0):.4f}")
            print(f"  Global tier    : {dist.get('global_tier', '?')}")
            print(f"  Free energy    : {dist.get('free_energy', 0):.2f}")
            print(f"{'─'*56}")
            tiers = dist.get('tiers', {})
            total = dist.get('total', 1)
            for t, n in tiers.items():
                bar = '█' * int(30 * n / total)
                print(f"  {t:5s}  {n:5d}  {bar}")
            print(f"{'─'*56}")
            svp = dist.get('size_vs_prime', [])
            if svp:
                print(f"  Top rank disagreements (PRIME vs size):")
                for x in svp[:10]:
                    arrow = '↑' if x['delta'] > 0 else '↓'
                    print(f"  {arrow} {x['name'][:38]:38s} "
                          f"Δrank={x['delta']:+4d} tier={x['tier']}")
            print(f"{'═'*56}")
        return

    # Rank and output
    ranked = rank_ir_functions(
        funcs, top=args.top, max_func_lines=args.max_lines,
        tier_filter=args.tier, eta=args.eta, iters=args.iters,
        verbose=not args.quiet,
    )

    if args.json:
        out = [{'rank': i+1, 'name': name, **meta}
               for i, (name, _, meta) in enumerate(ranked)]
        print(json.dumps(out, indent=2))
    else:
        print(f"\n{'─'*60}")
        print(f"{'RANK':>4}  {'TIER':5}  {'ENERGY':>9}  {'INST':>6}  {'LINES':>6}  NAME")
        print(f"{'─'*60}")
        for i, (name, _, meta) in enumerate(ranked):
            print(f"{i+1:>4}  {meta['tier']:5}  {meta['energy']:>9.4f}  "
                  f"{meta['inst_count']:>6}  {meta['line_count']:>6}  {name}")
        print(f"{'─'*60}")
        print(f"Total: {len(ranked)} functions returned.")


if __name__ == '__main__':
    main()
