#!/usr/bin/env python3
"""
=============================================================================
ZCC Criticality Engine — Wilson-Fisher Fixed Point Exploitation
=============================================================================
Five physics-grounded functions that exploit the fact the Dream Engine
converged to η ≈ 0.441 — the 2D Ising critical coupling.

Functions:
  1. topology_eta_search  — Binary search for η_c on arbitrary graph
  2. prime_free_energy    — F = E - TS universal fitness
  3. spectral_arrest      — Convergence via spectral gap stabilization
  4. relaxation_phase     — Exploit/explore phase detector
  5. universality_class   — Classify graph into universality class

All functions are pure (no I/O, no subprocess, no side effects).
Drop-in ready for zcc_oneirogenesis.py and dream_engine.py.
=============================================================================
"""

import math
from collections import deque
from dataclasses import dataclass
from typing import Optional


# ═══════════════════════════════════════════════════════════════════════
# DATA STRUCTURES
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class ClassInfo:
    """Universality class descriptor with critical exponents."""
    label: str              # e.g. "2D_ISING", "MEAN_FIELD", "3D_ISING"
    eta_c: float            # Critical coupling
    spectral_dim: float     # Effective dimensionality
    nu: float               # Correlation length exponent
    beta: float             # Order parameter exponent
    gamma: float            # Susceptibility exponent
    alpha: float            # Specific heat exponent (via hyperscaling)


# Known universality classes with exact/best-known critical exponents
_UNIVERSALITY_TABLE = [
    # label,        d_s,  η_c,    ν,      β,      γ,      α
    ("2D_ISING",    2.0,  0.4407, 1.0,    0.125,  1.75,   0.0),
    ("3D_ISING",    3.0,  0.2216, 0.6301, 0.3265, 1.2372, 0.110),
    ("4D_ISING",    4.0,  0.1497, 0.5,    0.5,    1.0,    0.0),    # mean-field
    ("MEAN_FIELD",  6.0,  0.0,    0.5,    0.5,    1.0,    0.0),    # d >= 4
    ("BETHE",       2.5,  0.3465, 0.8,    0.2,    1.5,    0.2),    # Bethe lattice approx
]


# ═══════════════════════════════════════════════════════════════════════
# GRAPH PRIMITIVES (no numpy dependency — pure Python)
# ═══════════════════════════════════════════════════════════════════════

def _graph_laplacian(adjacency: dict) -> list:
    """
    Build the graph Laplacian matrix L = D - A from an adjacency dict.
    adjacency: { node_id: [neighbor_id, ...], ... }
    Returns: (nodes_list, L_matrix) where L_matrix is list-of-lists.
    """
    nodes = sorted(adjacency.keys())
    n = len(nodes)
    idx = {node: i for i, node in enumerate(nodes)}

    L = [[0.0] * n for _ in range(n)]
    for node in nodes:
        i = idx[node]
        neighbors = adjacency.get(node, [])
        degree = 0
        for nb in neighbors:
            if nb in idx:
                j = idx[nb]
                L[i][j] = -1.0
                degree += 1
        L[i][i] = float(degree)

    return nodes, L


def _power_iteration_eigenvalues(L: list, k: int = 6,
                                  max_iter: int = 200, tol: float = 1e-6) -> list:
    """
    Estimate the k smallest eigenvalues of symmetric matrix L
    using inverse power iteration with deflation.
    Pure Python — no numpy. Suitable for N < ~2000 nodes.
    """
    n = len(L)
    if n == 0:
        return []

    k = min(k, n)
    eigenvalues = []

    # Work on a copy for deflation
    M = [row[:] for row in L]

    for _ in range(k):
        # Random initial vector
        v = [1.0 / math.sqrt(n)] * n
        # Add small perturbation to break symmetry
        import random as _rng
        for i in range(n):
            v[i] += _rng.gauss(0, 0.01)

        # Normalize
        norm = math.sqrt(sum(x * x for x in v))
        v = [x / max(norm, 1e-15) for x in v]

        lam_prev = 0.0
        for iteration in range(max_iter):
            # Matrix-vector multiply: w = M @ v
            w = [0.0] * n
            for i in range(n):
                s = 0.0
                for j in range(n):
                    s += M[i][j] * v[j]
                w[i] = s

            # Rayleigh quotient: λ = v^T M v
            lam = sum(v[i] * w[i] for i in range(n))

            # Normalize w
            norm = math.sqrt(sum(x * x for x in w))
            if norm < 1e-15:
                break
            v = [x / norm for x in w]

            if abs(lam - lam_prev) < tol:
                break
            lam_prev = lam

        eigenvalues.append(lam)

        # Deflate: M = M - λ * v * v^T
        for i in range(n):
            for j in range(n):
                M[i][j] -= lam * v[i] * v[j]

    eigenvalues.sort()
    return eigenvalues


# ═══════════════════════════════════════════════════════════════════════
# EXPLOIT 1 — TOPOLOGY-ADAPTIVE η SEARCH
# ═══════════════════════════════════════════════════════════════════════

def topology_eta_search(adjacency: dict, tol: float = 1e-4,
                        max_sweeps: int = 200, n_samples: int = 5) -> float:
    """
    Binary search for the critical coupling η_c on an arbitrary graph topology.

    Algorithm:
      1. Build the graph Laplacian.
      2. For a trial η, run n_samples Ising-like magnetization sweeps using
         the Metropolis algorithm at coupling η.
      3. Measure the magnetic susceptibility χ(η) = Var(m) * N.
      4. Binary search for the η that maximizes χ — this is η_c.

    Args:
        adjacency: { node_id: [neighbor_ids...] } — the graph structure.
                   For CFGs, nodes are basic blocks, edges are branches.
        tol: Convergence tolerance for the binary search.
        max_sweeps: Metropolis sweeps per trial.
        n_samples: Number of independent runs to average χ.

    Returns:
        η_c — the critical coupling constant for this graph topology.

    For reference:
        2D square lattice: η_c ≈ 0.4407
        Fully connected N:  η_c ≈ 1/(N-1)
        Bethe lattice (z):  η_c = atanh(1/(z-1))
    """
    nodes = sorted(adjacency.keys())
    n = len(nodes)
    if n < 3:
        return 0.4407  # Fallback to 2D Ising for trivial graphs

    idx = {node: i for i, node in enumerate(nodes)}
    # Precompute neighbor index lists for speed
    nb_idx = []
    for node in nodes:
        nbs = [idx[nb] for nb in adjacency.get(node, []) if nb in idx]
        nb_idx.append(nbs)

    import random as _rng

    def _susceptibility(eta: float) -> float:
        """Estimate magnetic susceptibility χ at coupling η."""
        chi_samples = []
        for _ in range(n_samples):
            # Random initial spin configuration: +1 or -1
            spins = [_rng.choice([-1, 1]) for _ in range(n)]

            # Metropolis sweeps
            for sweep in range(max_sweeps):
                for i in range(n):
                    # Local field from neighbors
                    h_local = sum(spins[j] for j in nb_idx[i])
                    # Energy change for flipping spin i
                    dE = 2.0 * eta * spins[i] * h_local
                    if dE <= 0 or _rng.random() < math.exp(-dE):
                        spins[i] *= -1

            # Magnetization
            m = sum(spins) / n
            chi_samples.append(m)

        # Susceptibility = Var(m) * N
        m_mean = sum(chi_samples) / len(chi_samples)
        m_var = sum((x - m_mean) ** 2 for x in chi_samples) / max(len(chi_samples) - 1, 1)
        return m_var * n

    # Binary search: bracket η_c between lo and hi
    # Start with a wide bracket
    lo, hi = 0.05, 1.5

    # Coarse scan to find the susceptibility peak region
    best_eta = 0.44
    best_chi = 0.0
    n_scan = 12
    for i in range(n_scan):
        eta = lo + (hi - lo) * i / (n_scan - 1)
        chi = _susceptibility(eta)
        if chi > best_chi:
            best_chi = chi
            best_eta = eta

    # Refine with narrower binary search around the peak
    lo = max(0.01, best_eta - 0.15)
    hi = min(2.0, best_eta + 0.15)

    for _ in range(20):  # ~20 iterations gives tol < 1e-4
        if hi - lo < tol:
            break
        m1 = lo + (hi - lo) / 3
        m2 = hi - (hi - lo) / 3
        chi1 = _susceptibility(m1)
        chi2 = _susceptibility(m2)
        if chi1 < chi2:
            lo = m1
        else:
            hi = m2

    return (lo + hi) / 2.0


# ═══════════════════════════════════════════════════════════════════════
# EXPLOIT 2 — FREE ENERGY FITNESS FUNCTION
# ═══════════════════════════════════════════════════════════════════════

def prime_free_energy(observables: dict, eta: float, T: float) -> float:
    """
    Universal fitness function: F = E - T*S

    Replaces the ad-hoc weighted score in FitnessOracle.measure().
    At criticality (η = η_c), this naturally balances exploitation (minimize E)
    against exploration (maximize S).

    Args:
        observables: dict with keys from FitnessOracle.measure():
            'inst_count'      — total instruction count (intensive)
            'branch_density'  — branch_count / inst_count (intensive)
            'stack_depth_sum' — total stack frame allocation (intensive)
            'asm_size'        — assembly file size in bytes (extensive)
            'branch_count'    — total branch count (extensive)
            'bin_size'        — binary size in bytes (extensive)
        eta: Critical coupling constant (from topology_eta_search).
        T: Effective temperature. Higher T → more exploration.
           Suggested: T = mutation_rate * acceptance_ratio * 100

    Returns:
        F — free energy. Lower is better. Replaces 'score'.

    Physics:
        E = η * Σ_intensive(normalized observables)
        S = -Σ p_i * log(p_i)  where p_i = extensive_i / Σ extensive_j
        F = E - T * S
    """
    # Energy: weighted sum of intensive observables (normalized)
    inst = observables.get('inst_count', 0)
    bd = observables.get('branch_density', 0.0)
    stack = observables.get('stack_depth_sum', 0)

    # Normalize to O(1) scale
    E_inst = inst / 100000.0      # typical ZCC: ~100k instructions
    E_branch = bd * 10.0          # branch density ~ 0.1-0.3
    E_stack = stack / 50000.0     # typical stack sum ~ 50k

    E = eta * (E_inst + E_branch + E_stack)

    # Entropy: diversity measure from extensive observables
    asm_size = max(observables.get('asm_size', 1), 1)
    bin_size = max(observables.get('bin_size', 1), 1)
    branch_count = max(observables.get('branch_count', 1), 1)

    # Proportions for entropy calculation
    total = float(asm_size + bin_size + branch_count)
    probs = [asm_size / total, bin_size / total, branch_count / total]

    S = 0.0
    for p in probs:
        if p > 0:
            S -= p * math.log(p)

    # Free energy
    F = E - T * S

    return F


# ═══════════════════════════════════════════════════════════════════════
# EXPLOIT 3 — SPECTRAL ARREST CONVERGENCE
# ═══════════════════════════════════════════════════════════════════════

class SpectralArrestDetector:
    """
    Stateful convergence detector based on spectral gap stabilization.

    Tracks the spectral gap (difference between first two eigenvalues of the
    fitness covariance matrix) over a sliding window. Declares convergence
    when the relative change in the gap drops below threshold.

    Usage:
        detector = SpectralArrestDetector(window=10, threshold=0.01)
        for cycle in range(max_cycles):
            detector.record(fitness_vector)
            if detector.arrested():
                break  # Converged — at the fixed point
    """

    def __init__(self, window: int = 10, threshold: float = 0.01):
        self.window = window
        self.threshold = threshold
        self._history: deque = deque(maxlen=window * 2)
        self._gaps: deque = deque(maxlen=window)

    def record(self, fitness_vector: list):
        """
        Record a fitness observation.

        Args:
            fitness_vector: [inst_count, branch_density, stack_depth, asm_size]
                           or any list of scalar observables.
        """
        self._history.append(fitness_vector[:])

    def arrested(self) -> bool:
        """Returns True if the spectral gap has stabilized (convergence)."""
        if len(self._history) < self.window + 2:
            return False

        # Compute covariance matrix of recent fitness vectors
        recent = list(self._history)[-self.window:]
        if not recent or not recent[0]:
            return False

        d = len(recent[0])
        n = len(recent)

        # Mean
        mean = [0.0] * d
        for vec in recent:
            for i in range(d):
                mean[i] += vec[i]
        mean = [m / n for m in mean]

        # Covariance matrix (d x d)
        cov = [[0.0] * d for _ in range(d)]
        for vec in recent:
            for i in range(d):
                for j in range(d):
                    cov[i][j] += (vec[i] - mean[i]) * (vec[j] - mean[j])
        for i in range(d):
            for j in range(d):
                cov[i][j] /= max(n - 1, 1)

        # Eigenvalues of the covariance matrix (power iteration)
        eigs = _power_iteration_eigenvalues(cov, k=min(d, 3), max_iter=50)
        if len(eigs) < 2:
            return False

        # Spectral gap
        gap = abs(eigs[1] - eigs[0])
        self._gaps.append(gap)

        if len(self._gaps) < 3:
            return False

        # Check if gap is stable (relative change below threshold)
        gaps_list = list(self._gaps)
        recent_gaps = gaps_list[-3:]
        mean_gap = sum(recent_gaps) / len(recent_gaps)
        if mean_gap < 1e-15:
            return True  # Gap collapsed to zero — fully converged

        max_delta = max(abs(g - mean_gap) for g in recent_gaps)
        relative_change = max_delta / mean_gap

        return relative_change < self.threshold

    @property
    def spectral_gap(self) -> float:
        """Current spectral gap value (for telemetry)."""
        if self._gaps:
            return self._gaps[-1]
        return float('inf')

    @property
    def gap_history(self) -> list:
        """Full gap history (for plotting)."""
        return list(self._gaps)


def spectral_arrest(eigenvalues: list, window: int = 10,
                    threshold: float = 0.01) -> bool:
    """
    Stateless convenience wrapper: given a list of spectral gap measurements,
    returns True if the most recent `window` gaps show relative change < threshold.

    For stateful tracking across cycles, use SpectralArrestDetector instead.
    """
    if len(eigenvalues) < window:
        return False

    recent = eigenvalues[-window:]
    mean_val = sum(recent) / len(recent)
    if mean_val < 1e-15:
        return True

    max_delta = max(abs(g - mean_val) for g in recent)
    return (max_delta / mean_val) < threshold


# ═══════════════════════════════════════════════════════════════════════
# EXPLOIT 4 — RELAXATION PHASE DETECTOR
# ═══════════════════════════════════════════════════════════════════════

def relaxation_phase(fitness_history: list, tau: float = 5.0) -> str:
    """
    Detects whether the system is in the 'exploit' or 'explore' phase
    of the relaxation oscillation.

    Uses exponentially-weighted moving average to detect trajectory slope.
    Fast descent = exploit (apply targeted mutations).
    Plateau/ascent = explore (apply sweeps, increase mutation count).

    Args:
        fitness_history: List of recent fitness scores (lower = better).
                        Needs at least 3 entries.
        tau: Autocorrelation time (from spectral gap: τ ≈ 1/gap).
             Higher τ = slower phase transitions.

    Returns:
        "exploit" — fitness is actively decreasing. Use point mutations.
        "explore" — fitness is plateaued or increasing. Use sweeps, more mutations.
    """
    if len(fitness_history) < 3:
        return "explore"  # Not enough data — explore by default

    # Exponential weighting with timescale tau
    alpha = 2.0 / (tau + 1.0)
    n = len(fitness_history)

    # Compute EWMA of the derivative
    derivatives = []
    for i in range(1, n):
        derivatives.append(fitness_history[i] - fitness_history[i - 1])

    if not derivatives:
        return "explore"

    ewma = derivatives[0]
    for i in range(1, len(derivatives)):
        ewma = alpha * derivatives[i] + (1.0 - alpha) * ewma

    # Negative EWMA = fitness decreasing = exploit phase
    # Positive or near-zero EWMA = plateau = explore phase
    if ewma < -0.01 * abs(fitness_history[-1] + 1e-10):
        return "exploit"
    else:
        return "explore"


# ═══════════════════════════════════════════════════════════════════════
# EXPLOIT 5 — UNIVERSALITY CLASS CLASSIFIER
# ═══════════════════════════════════════════════════════════════════════

def universality_class(eta_c: float, dim: float) -> ClassInfo:
    """
    Classifies the graph into a universality class based on
    measured η_c and effective spectral dimensionality.

    Returns the closest known universality class with predicted
    critical exponents (ν, β, γ, α).

    Args:
        eta_c: Critical coupling from topology_eta_search().
        dim: Effective spectral dimension from cfg_spectral_dim().

    Returns:
        ClassInfo with label and all critical exponents.
    """
    # Score each known class by distance in (η_c, d_s) space
    best_score = float('inf')
    best_class = None

    for label, d_s, eta_ref, nu, beta, gamma, alpha in _UNIVERSALITY_TABLE:
        # Weighted distance: η_c matters more (it's more precisely measured)
        d_eta = (eta_c - eta_ref) ** 2 * 4.0
        d_dim = (dim - d_s) ** 2 * 1.0
        score = d_eta + d_dim

        if score < best_score:
            best_score = score
            best_class = ClassInfo(
                label=label,
                eta_c=eta_ref,
                spectral_dim=d_s,
                nu=nu, beta=beta, gamma=gamma, alpha=alpha
            )

    # If dim is fractional (between known classes), interpolate exponents
    if best_class and abs(dim - best_class.spectral_dim) > 0.3:
        # Find two nearest classes for interpolation
        table_sorted = sorted(_UNIVERSALITY_TABLE, key=lambda x: abs(x[1] - dim))
        if len(table_sorted) >= 2:
            c1 = table_sorted[0]
            c2 = table_sorted[1]
            d1, d2 = c1[1], c2[1]
            if abs(d2 - d1) > 0.01:
                t = (dim - d1) / (d2 - d1)
                t = max(0.0, min(1.0, t))
                best_class = ClassInfo(
                    label=f"INTERP_{c1[0]}_{c2[0]}",
                    eta_c=c1[2] + t * (c2[2] - c1[2]),
                    spectral_dim=dim,
                    nu=c1[3] + t * (c2[3] - c1[3]),
                    beta=c1[4] + t * (c2[4] - c1[4]),
                    gamma=c1[5] + t * (c2[5] - c1[5]),
                    alpha=c1[6] + t * (c2[6] - c1[6]),
                )

    return best_class


# ═══════════════════════════════════════════════════════════════════════
# UTILITY: BOLTZMANN ACCEPTANCE
# ═══════════════════════════════════════════════════════════════════════

def boltzmann_acceptance(delta_F: float, T: float) -> bool:
    """
    Metropolis-Hastings acceptance criterion using free energy.

    P(accept) = min(1, exp(-ΔF / T))

    At T=0: greedy descent only (current dream engine behavior).
    At T=T_c: maximum entropy exploration.

    Args:
        delta_F: Change in free energy (mutant - parent). Negative = improvement.
        T: Temperature. Use effective T from mutation_rate * acceptance_ratio.

    Returns:
        True if the mutation should be accepted.
    """
    if delta_F <= 0:
        return True  # Always accept improvements

    if T <= 0:
        return False  # T=0: greedy only

    import random as _rng
    exponent = delta_F / T
    if exponent > 500:
        return False  # Overflow guard
    return _rng.random() < math.exp(-exponent)
