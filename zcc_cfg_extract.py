#!/usr/bin/env python3
"""
=============================================================================
ZCC CFG Extractor — Assembly to Graph Adjacency
=============================================================================
Extracts a Control Flow Graph adjacency matrix from x86-64 assembly (.s files).

Nodes: labels (.L*, function labels)
Edges: branch instructions (jcc, jmp, call)

Output format: { label: [target_labels...] } — ready for topology_eta_search()

Also computes:
  - Effective spectral dimension d_s via Laplacian eigenvalue analysis
  - Basic graph statistics (nodes, edges, degree distribution)
=============================================================================
"""

import re
import math
from typing import Optional


# x86-64 branch mnemonics
_BRANCH_MNEMONICS = frozenset({
    'je', 'jne', 'jl', 'jle', 'jg', 'jge',
    'ja', 'jae', 'jb', 'jbe', 'jmp',
    'js', 'jns', 'jo', 'jno', 'jz', 'jnz',
    'jc', 'jnc', 'jp', 'jnp', 'jpe', 'jpo',
    'jrcxz', 'jecxz', 'loop', 'loope', 'loopne',
    'call',
})

# Label patterns
_LABEL_RE = re.compile(r'^(\.[A-Za-z_]\w*|[A-Za-z_]\w*):')
_LOCAL_LABEL_RE = re.compile(r'^\.[A-Z_a-z]\w*$')


def extract_cfg(asm_lines: list) -> dict:
    """
    Extract a Control Flow Graph from x86-64 assembly lines.

    Args:
        asm_lines: List of assembly source lines (from .s file).

    Returns:
        adjacency: { label: [target_labels...] }
        Each label is a basic block entry point.
        Edges represent branch targets and fall-through.
    """
    adjacency = {}
    label_set = set()
    current_label = "__entry__"

    # Pass 1: Collect all labels
    for line in asm_lines:
        stripped = line.strip()
        m = _LABEL_RE.match(stripped)
        if m:
            lbl = m.group(1)
            label_set.add(lbl)

    # Pass 2: Build edges
    adjacency[current_label] = []

    for i, line in enumerate(asm_lines):
        stripped = line.strip()

        # Label definition — new basic block
        m = _LABEL_RE.match(stripped)
        if m:
            new_label = m.group(1)
            # Fall-through edge from previous block (if it didn't end with jmp/ret)
            if current_label and current_label in adjacency:
                # Check if previous instruction was unconditional jump or ret
                prev_is_term = False
                for j in range(i - 1, max(i - 3, -1), -1):
                    prev = asm_lines[j].strip()
                    if not prev or prev.startswith('.') or prev.startswith('#'):
                        continue
                    mnemonic = prev.split()[0] if prev.split() else ''
                    if mnemonic in ('jmp', 'ret', 'retq', 'ud2', 'hlt'):
                        prev_is_term = True
                    break

                if not prev_is_term and new_label not in adjacency.get(current_label, []):
                    adjacency.setdefault(current_label, []).append(new_label)

            current_label = new_label
            adjacency.setdefault(current_label, [])
            continue

        # Skip non-instructions
        if not stripped or stripped.startswith('.') or stripped.startswith('#'):
            continue

        parts = stripped.split()
        if not parts:
            continue

        mnemonic = parts[0].rstrip(':')

        # Branch instruction — extract target
        if mnemonic in _BRANCH_MNEMONICS and len(parts) >= 2:
            target = parts[1].rstrip(',')
            # Only track internal labels (not register-indirect or PLT)
            if target in label_set:
                adjacency.setdefault(current_label, [])
                if target not in adjacency[current_label]:
                    adjacency[current_label].append(target)
                # Ensure target exists as a node
                adjacency.setdefault(target, [])

        # ret/retq — no outgoing edges (terminal block)
        elif mnemonic in ('ret', 'retq'):
            pass  # Block ends, no fall-through

    # Clean up: remove isolated nodes with no edges at all
    # (keep nodes that have edges TO them even if they have no outgoing)
    all_targets = set()
    for targets in adjacency.values():
        all_targets.update(targets)

    # Remove nodes with no incoming AND no outgoing edges (pure noise)
    to_remove = []
    for node, targets in adjacency.items():
        if not targets and node not in all_targets and node != "__entry__":
            to_remove.append(node)
    for node in to_remove:
        del adjacency[node]

    return adjacency


def cfg_spectral_dim(adjacency: dict) -> float:
    """
    Compute effective spectral dimension via sparse Laplacian eigensolver.
    Uses scipy.sparse — O(N) memory, sub-second for N=22k.
    Falls back to degree-based estimate if scipy unavailable.
    d_s = 2 * log(N) / log(lambda_max / lambda_1)
    """
    nodes = sorted(adjacency.keys())
    n = len(nodes)
    if n < 4:
        return 2.0
    try:
        import numpy as np
        import scipy.sparse as sp
        import scipy.sparse.linalg as spla
        idx = {node: i for i, node in enumerate(nodes)}
        rows, cols, data = [], [], []
        degrees = [0] * n
        for node in nodes:
            i = idx[node]
            for nb in adjacency.get(node, []):
                if nb in idx:
                    j = idx[nb]
                    rows.append(i); cols.append(j); data.append(-1.0)
                    degrees[i] += 1
        for i, d in enumerate(degrees):
            rows.append(i); cols.append(i); data.append(float(d))
        L = sp.csr_matrix((data, (rows, cols)), shape=(n, n))
        k_small = min(6, n - 2)
        vals_small, _ = spla.eigsh(L, k=k_small, which="SM", tol=1e-4, maxiter=1000)
        vals_small = sorted(abs(v) for v in vals_small)
        lambda_1 = next((v for v in vals_small if v > 1e-8), None)
        if lambda_1 is None:
            return 2.0
        vals_large, _ = spla.eigsh(L, k=1, which="LM", tol=1e-4, maxiter=1000)
        lambda_max = abs(vals_large[0])
        if lambda_max / lambda_1 <= 1.0:
            return 2.0
        import math
        return 2.0 * math.log(n) / math.log(lambda_max / lambda_1)
    except Exception:
        import math
        degrees = [len(adjacency.get(nd, [])) for nd in nodes]
        avg_deg = sum(degrees) / max(n, 1)
        max_deg = max(degrees) if degrees else 1
        if max_deg < 1:
            return 2.0
        return 2.0 * math.log(n) / math.log(max(max_deg / max(avg_deg, 1e-8), 1.001))

def cfg_stats(adjacency: dict) -> dict:
    """Basic graph statistics for diagnostics."""
    n_nodes = len(adjacency)
    n_edges = sum(len(targets) for targets in adjacency.values())
    degrees = [len(targets) for targets in adjacency.values()]
    avg_degree = sum(degrees) / max(n_nodes, 1)
    max_degree = max(degrees) if degrees else 0

    return {
        'nodes': n_nodes,
        'edges': n_edges,
        'avg_degree': round(avg_degree, 2),
        'max_degree': max_degree,
    }
