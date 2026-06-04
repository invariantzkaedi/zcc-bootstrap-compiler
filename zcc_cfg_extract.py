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
    Compute the effective spectral dimension of a graph from its Laplacian spectrum.

    Uses the scaling relation: d_s = 2 * log(N) / log(λ_max / λ_1)
    where λ_1 is the smallest nonzero eigenvalue (Fiedler value)
    and λ_max is the largest eigenvalue.

    Args:
        adjacency: Graph adjacency dict from extract_cfg().

    Returns:
        d_s — effective spectral dimension (typically 1.5-4.0 for CFGs).
    """
    from zcc_criticality import _graph_laplacian, _power_iteration_eigenvalues

    nodes, L = _graph_laplacian(adjacency)
    n = len(nodes)

    if n < 4:
        return 2.0  # Fallback for trivial graphs

    # Get eigenvalues
    k = min(n, 8)
    eigenvalues = _power_iteration_eigenvalues(L, k=k, max_iter=100)

    # Find smallest nonzero eigenvalue (Fiedler value)
    lambda_1 = None
    for ev in eigenvalues:
        if ev > 1e-8:
            lambda_1 = ev
            break

    if lambda_1 is None or lambda_1 < 1e-10:
        return 2.0  # Disconnected or trivial

    # Largest eigenvalue (max degree * 2 is an upper bound, but use actual)
    lambda_max = max(eigenvalues) if eigenvalues else 1.0

    if lambda_max / lambda_1 <= 1.0:
        return 2.0

    d_s = 2.0 * math.log(n) / math.log(lambda_max / lambda_1)

    # Clamp to physically meaningful range
    d_s = max(1.0, min(6.0, d_s))

    return d_s


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
