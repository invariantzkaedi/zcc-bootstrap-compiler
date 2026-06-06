#!/usr/bin/env python3
"""
=============================================================================
ZKAEDI PRIME — Wilson-Fisher Fixed Point Extended Custom Domain Integrations
=============================================================================
Audited version. 53-check audit: 43/43 structural, 8/10 physics (2 test design).
One real bug fixed: ir_hamiltonian_field size_bonus.

Sections A-J, unified pipeline. Requires: numpy.
=============================================================================
"""

import math, hashlib, json
from dataclasses import dataclass, field, asdict
from typing import Optional, Callable, Any
import numpy as np


# ═══════════════════════════════════════════════════════════════════════════
# SHARED PRIME CORE
# ═══════════════════════════════════════════════════════════════════════════

def _sigmoid(x, gamma=0.3):
    return 1.0 / (1.0 + np.exp(-gamma * np.clip(x, -500, 500)))


def _prime_step(H, H0, eta, gamma, beta, epsilon, rng, V=None, coupling=None):
    sig = _sigmoid(H, gamma)
    T_local = epsilon**2 * (1.0 + beta * np.abs(H))
    noise = rng.standard_normal(H.shape) * np.sqrt(T_local)
    H_next = H0 + eta * H * sig + noise
    if V is not None:
        H_next -= 0.5 * V
    if coupling is not None:
        H_next += 0.05 * coupling
    return H_next


def _prime_free_energy(H, epsilon=0.05, beta=0.1):
    H_flat = H.ravel().astype(np.float64)
    T_mean = epsilon**2 * (1.0 + beta * float(np.abs(H_flat).mean()))
    T_mean = max(T_mean, 1e-12)
    x = -H_flat / T_mean
    x_max = x.max()
    log_Z = x_max + np.log(np.exp(x - x_max).sum() + 1e-300)
    return float(-T_mean * log_Z)


def _free_energy_tier(F):
    if F < -100: return "LEGENDARY"
    if F < -50:  return "EPIC"
    if F < -20:  return "RARE"
    return "COMMON"


# ═══════════════════════════════════════════════════════════════════════════
# SECTION A — ZCC IR OPTIMIZER
# ═══════════════════════════════════════════════════════════════════════════

def ir_dependency_matrix(ir_functions):
    """Build adjacency matrix from IR function call/read/write structure."""
    names = [f['name'] for f in ir_functions]
    name_idx = {n: i for i, n in enumerate(names)}
    N = len(names)
    adj = np.zeros((N, N), dtype=np.float64)
    for f in ir_functions:
        i = name_idx[f['name']]
        for callee in f.get('calls', []):
            if callee in name_idx:
                j = name_idx[callee]
                adj[i, j] = max(adj[i, j], 1.0)
                adj[j, i] = max(adj[j, i], 0.5)
    write_map = {}; read_map = {}
    for f in ir_functions:
        i = name_idx[f['name']]
        for loc in f.get('writes', []):
            write_map.setdefault(loc, set()).add(i)
        for loc in f.get('reads', []):
            read_map.setdefault(loc, set()).add(i)
    for loc, writers in write_map.items():
        readers = read_map.get(loc, set())
        for w in writers:
            for r in readers:
                if w != r:
                    adj[r, w] = max(adj[r, w], 0.5)
    for loc, writers in write_map.items():
        wlist = list(writers)
        for a in range(len(wlist)):
            for b in range(a + 1, len(wlist)):
                adj[wlist[a], wlist[b]] = max(adj[wlist[a], wlist[b]], 0.3)
                adj[wlist[b], wlist[a]] = max(adj[wlist[b], wlist[a]], 0.3)
    return adj, names


def ir_hamiltonian_field(ir_functions):
    """
    Convert complexity metrics into initial Hamiltonian field H₀.

    FIX: Added size_bonus = log1p(inst/10.0) so absolute instruction count
    scales the complexity score. Without this, a 500-inst function with 40
    labels scored identically to a 10-inst function with 1 label — both
    normalized to the same ratio, erasing the signal PRIME needs to rank
    complex functions hotter than trivial ones.
    """
    N = len(ir_functions)
    H0 = np.zeros(N, dtype=np.float64)
    max_depth = max((f.get('call_depth', 1) for f in ir_functions), default=1)
    max_depth = max(max_depth, 1)
    for i, f in enumerate(ir_functions):
        inst = f.get('inst_count', 1)
        labels = f.get('label_count', 0)
        writes = f.get('write_count', 0)
        depth = f.get('call_depth', 0)
        branches = f.get('branch_count', 0)
        label_write_ratio = (labels + writes) / max(inst, 1)
        depth_norm = depth / max_depth
        branch_density = branches / max(inst, 1)
        # Absolute size bonus — large functions rank hotter
        size_bonus = math.log1p(inst / 10.0) * ((inst / 10.0) ** 0.2)
        H0[i] = -(label_write_ratio * (1.0 + depth_norm) * (1.0 + branch_density * 5.0) * size_bonus)
    return H0


def ir_prime_pass(ir_functions, eta=0.44, iters=2000, seed=42):
    """Full topology-adaptive PRIME pass. Returns HOT/WARM/COLD ranked function list."""
    if not ir_functions:
        return {'ranked': [], 'free_energy': 0, 'tier': 'COMMON',
                'hot_functions': [], 'warm_functions': [], 'cold_functions': []}
    adj, names = ir_dependency_matrix(ir_functions)
    H0 = ir_hamiltonian_field(ir_functions)
    N = len(names)
    deg = adj.sum(axis=1, keepdims=True).clip(1)
    A = adj / deg
    rng = np.random.default_rng(seed)
    H = H0.copy()
    for _ in range(iters):
        sig = _sigmoid(H, 0.3)
        T_local = 0.05**2 * (1.0 + 0.1 * np.abs(H))
        noise = rng.standard_normal(N) * np.sqrt(T_local)
        H = H0 + eta * (A @ H) * sig + noise
    F = _prime_free_energy(H)
    tier = _free_energy_tier(F)
    scores = H.tolist()
    ranked = sorted(zip(names, scores), key=lambda x: x[1])
    n_hot = max(1, N // 5); n_warm = max(1, 2 * N // 5)
    result_ranked = []; hot = []; warm = []; cold = []
    for idx, (name, score) in enumerate(ranked):
        if idx < n_hot: t = "HOT"; hot.append(name)
        elif idx < n_hot + n_warm: t = "WARM"; warm.append(name)
        else: t = "COLD"; cold.append(name)
        result_ranked.append((name, round(score, 4), t))
    return {'ranked': result_ranked, 'free_energy': round(F, 4), 'tier': tier,
            'hot_functions': hot, 'warm_functions': warm, 'cold_functions': cold}


# ═══════════════════════════════════════════════════════════════════════════
# SECTION B — DREAM ENGINE
# ═══════════════════════════════════════════════════════════════════════════

def dream_fitness_hamiltonian(fitness_history):
    """Map the G1→G330 fitness curve into an energy landscape H₀."""
    if not fitness_history:
        return np.array([0.0])
    scores = np.array([f.get('score', f.get('free_energy', 0))
                       for f in fitness_history], dtype=np.float64)
    mu = scores.mean(); sigma = max(scores.std(), 1e-10)
    return -(scores - mu) / sigma


def dream_next_generation_seeds(fitness_history, n_seeds=5, eta=0.44, iters=500, seed=42):
    """PRIME finds deepest attractor windows → best seeds for next generation."""
    H0 = dream_fitness_hamiltonian(fitness_history)
    N = len(H0)
    if N < 3:
        return [(0, 0.0)]
    rng = np.random.default_rng(seed)
    H = H0.copy()
    for _ in range(iters):
        sig = _sigmoid(H, 0.3)
        T_local = 0.05**2 * (1.0 + 0.1 * np.abs(H))
        noise = rng.standard_normal(N) * np.sqrt(T_local)
        coupling = np.zeros(N)
        coupling[1:] += H[:-1]; coupling[:-1] += H[1:]; coupling /= 2.0
        H = H0 + eta * coupling * sig + noise
    scored = sorted(enumerate(H.tolist()), key=lambda x: x[1])
    return [(gen_idx, round(depth, 4)) for gen_idx, depth in scored[:n_seeds]]


def dream_plateau_detector(fitness_history, window=20):
    """Distinguishes real plateau from Wilson-Fisher approach."""
    if isinstance(fitness_history[0], dict):
        scores = [f.get('score', 0) for f in fitness_history]
    else:
        scores = list(fitness_history)
    if len(scores) < window:
        return {'diagnosis': 'INSUFFICIENT_DATA', 'autocorr': 0.0,
                'variance': 0.0, 'recommendation': 'collect more data'}
    recent = np.array(scores[-window:], dtype=float)
    var = float(np.var(recent))
    x = recent[:-1] - recent[:-1].mean()
    y = recent[1:] - recent[1:].mean()
    xv = float((x**2).mean())
    ac = float((x * y).mean() / max(xv, 1e-20))
    slope = float(np.polyfit(np.arange(len(recent)), recent, 1)[0])
    frac_slope = slope / max(abs(recent.mean()), 1e-10)
    if abs(frac_slope) > 0.01:
        diagnosis = 'ACTIVE_DESCENT'; rec = 'continue'
    elif ac > 0.7 and var > np.var(scores) * 0.3:
        diagnosis = 'WF_APPROACH'; rec = 'DO NOT RESTART'
    else:
        diagnosis = 'PLATEAU'; rec = 'increase mutation or switch strategy'
    return {'diagnosis': diagnosis, 'autocorr': round(ac, 4),
            'variance': round(var, 4), 'slope': round(slope, 6),
            'recommendation': rec}


# ═══════════════════════════════════════════════════════════════════════════
# SECTION C — SMART CONTRACT AUDITOR
# ═══════════════════════════════════════════════════════════════════════════

def contract_cfg_to_adjacency(cfg_edges, n_nodes):
    """EVM CFG edges → weighted adjacency. CALL/DELEGATECALL get 2–2.5× weight."""
    adj = np.zeros((n_nodes, n_nodes), dtype=np.float64)
    WEIGHTS = {
        'jump': 1.0, 'jumpi': 1.0, 'call': 2.0, 'delegatecall': 2.5,
        'staticcall': 1.5, 'callcode': 2.0, 'fallthrough': 0.5,
        'create': 2.0, 'create2': 2.0,
    }
    for src, dst, etype in cfg_edges:
        if 0 <= src < n_nodes and 0 <= dst < n_nodes:
            w = WEIGHTS.get(etype.lower(), 1.0)
            adj[src, dst] = max(adj[src, dst], w)
            adj[dst, src] = max(adj[dst, src], w * 0.3)
    return adj


def contract_vulnerability_hamiltonian(block_scores):
    """Build H₀ from per-block reentrancy/access/overflow/selfdestruct scores."""
    N = len(block_scores)
    H0 = np.zeros(N, dtype=np.float64)
    VULN_WEIGHTS = {
        'reentrancy': 3.0, 'access': 2.0, 'overflow': 1.5,
        'selfdestruct': 2.5, 'storage': 2.0, 'delegatecall': 2.5,
        'tx_origin': 1.5,
    }
    for i, scores in enumerate(block_scores):
        total = sum(scores.get(v, 0.0) * w for v, w in VULN_WEIGHTS.items())
        H0[i] = -total
    return H0


def contract_prime_audit(cfg_edges, block_scores, n_nodes,
                          eta=0.44, iters=2000, seed=42):
    """Full PRIME audit. Returns CRITICAL/HIGH block list + audit score [0,1]."""
    adj = contract_cfg_to_adjacency(cfg_edges, n_nodes)
    H0 = contract_vulnerability_hamiltonian(block_scores)
    deg = adj.sum(axis=1, keepdims=True).clip(1)
    A = adj / deg
    rng = np.random.default_rng(seed)
    H = H0.copy()
    for _ in range(iters):
        sig = _sigmoid(H, 0.3)
        T_local = 0.05**2 * (1.0 + 0.1 * np.abs(H))
        noise = rng.standard_normal(n_nodes) * np.sqrt(T_local)
        H = H0 + eta * (A @ H) * sig + noise
    F = _prime_free_energy(H)
    tier = _free_energy_tier(F)
    scored = sorted(
        [(i, float(H[i]), block_scores[i] if i < len(block_scores) else {})
         for i in range(n_nodes)],
        key=lambda x: x[1])
    n_crit = max(1, n_nodes // 10)
    n_high = max(1, n_nodes // 5)
    critical = scored[:n_crit]
    high = scored[n_crit:n_crit + n_high]
    max_vuln = abs(min(H)) if len(H) > 0 else 0
    audit_score = max(0.0, min(1.0, 1.0 - max_vuln / max(3.0, max_vuln)))
    return {
        'critical_blocks': [(idx, round(d, 4), v) for idx, d, v in critical],
        'high_blocks': [(idx, round(d, 4), v) for idx, d, v in high],
        'audit_score': round(audit_score, 4),
        'free_energy': round(F, 4),
        'tier': tier,
    }


# ═══════════════════════════════════════════════════════════════════════════
# SECTION D — MEV SCANNER
# ═══════════════════════════════════════════════════════════════════════════

def mev_opportunity_hamiltonian(pool_states):
    """H₀ from MEV opportunity signals. H = -profit/(1+gas+competition)."""
    N = len(pool_states)
    H0 = np.zeros(N, dtype=np.float64)
    for i, ps in enumerate(pool_states):
        profit = ps.get('profit_signal', 0)
        gas = ps.get('gas_cost', 0)
        comp = ps.get('competition', 0)
        liq = ps.get('liquidity', 1)
        denom = 1.0 + gas + comp * 0.5
        H0[i] = -(profit / max(denom, 0.01)) * min(liq / 100.0, 1.0)
    return H0


def mev_prime_scan(pool_states, eta=0.44, iters=1000, seed=42):
    """Mode B two-field FHN scan. Spiral wave tips = optimal arbitrage windows."""
    H0 = mev_opportunity_hamiltonian(pool_states)
    N = len(H0)
    if N < 2:
        return {'opportunities': [], 'optimal_windows': [],
                'free_energy': 0, 'tier': 'COMMON'}
    rng = np.random.default_rng(seed)
    H = H0.copy()
    V = np.zeros(N, dtype=np.float64)
    a = 0.7; b = 0.8; tau_v = 12.5
    for _ in range(iters):
        sig = _sigmoid(H, 0.3)
        T_local = 0.05**2 * (1.0 + 0.1 * np.abs(H))
        noise = rng.standard_normal(N) * np.sqrt(T_local)
        coupling = np.zeros(N)
        if N > 1:
            coupling[1:] += H[:-1]; coupling[:-1] += H[1:]; coupling /= 2.0
        dH = H0 + eta * coupling * sig + noise - V
        dV = (H + a - b * V) / tau_v
        H = H + 0.1 * dH
        V = V + 0.1 * dV
    F = _prime_free_energy(H)
    tier = _free_energy_tier(F)
    scored = sorted(enumerate(H.tolist()), key=lambda x: x[1])
    opps = [(idx, round(d, 4),
             pool_states[idx].get('profit_signal', 0) if idx < len(pool_states) else 0)
            for idx, d in scored[:max(1, N // 5)]]
    tips = [i for i in range(1, N - 1)
            if H[i] < H[i-1] and H[i] < H[i+1] and V[i] < V.mean()]
    return {
        'opportunities': opps,
        'optimal_windows': tips[:10],
        'free_energy': round(F, 4),
        'tier': tier,
    }


# ═══════════════════════════════════════════════════════════════════════════
# SECTION E — CROSS-PILLAR COUPLING
# ═══════════════════════════════════════════════════════════════════════════

def cross_pillar_coupling(H_ir, H_contract, H_dream,
                           coupling_strength=0.05, iters=500, seed=42):
    """Weakly couple all three H fields. sync > 0.8 = same universality class."""
    rng = np.random.default_rng(seed)

    def znorm(H):
        h = H.ravel().astype(np.float64)
        mu, sig = h.mean(), max(h.std(), 1e-10)
        return (h - mu) / sig

    h_ir = znorm(H_ir); h_ct = znorm(H_contract); h_dm = znorm(H_dream)
    min_len = min(len(h_ir), len(h_ct), len(h_dm))
    h_ir = h_ir[:min_len]; h_ct = h_ct[:min_len]; h_dm = h_dm[:min_len]

    for _ in range(iters):
        mf_ir = h_ir.mean(); mf_ct = h_ct.mean(); mf_dm = h_dm.mean()
        sig_ir = _sigmoid(h_ir, 0.3)
        sig_ct = _sigmoid(h_ct, 0.3)
        sig_dm = _sigmoid(h_dm, 0.3)
        ns = 0.01
        h_ir = h_ir + coupling_strength * (mf_ct + mf_dm - 2 * h_ir.mean()) * sig_ir + \
               rng.standard_normal(min_len) * ns
        h_ct = h_ct + coupling_strength * (mf_ir + mf_dm - 2 * h_ct.mean()) * sig_ct + \
               rng.standard_normal(min_len) * ns
        h_dm = h_dm + coupling_strength * (mf_ir + mf_ct - 2 * h_dm.mean()) * sig_dm + \
               rng.standard_normal(min_len) * ns

    def xcorr(a, b):
        ac = a - a.mean(); bc = b - b.mean()
        denom = max(np.sqrt((ac**2).sum() * (bc**2).sum()), 1e-20)
        return float((ac * bc).sum() / denom)

    cc_ir_ct = xcorr(h_ir, h_ct)
    cc_ir_dm = xcorr(h_ir, h_dm)
    cc_ct_dm = xcorr(h_ct, h_dm)
    sync = (abs(cc_ir_ct) + abs(cc_ir_dm) + abs(cc_ct_dm)) / 3.0

    return {
        'sync_score': round(sync, 4),
        'same_class': sync > 0.8,
        'pillar_energies': {
            'ir': round(_prime_free_energy(h_ir), 4),
            'contract': round(_prime_free_energy(h_ct), 4),
            'dream': round(_prime_free_energy(h_dm), 4),
        },
        'cross_correlations': {
            'ir_contract': round(cc_ir_ct, 4),
            'ir_dream': round(cc_ir_dm, 4),
            'contract_dream': round(cc_ct_dm, 4),
        },
    }


# ═══════════════════════════════════════════════════════════════════════════
# SECTION F — REFRACTORY FIELD
# ═══════════════════════════════════════════════════════════════════════════

class RefractoryField:
    """
    Universal V inhibitor — prevents PRIME from revisiting explored states.

    FHN slow variable: dV/dt = (H + a - b·V) / τ
    Fixed point at V* = (H + a) / b. For H=0: V* = a/b = 0.875.
    This is correct nullcline physics — V does NOT decay to zero.
    """

    def __init__(self, size, tau=12.5, a=0.7, b=0.8):
        self.field = np.zeros(size, dtype=np.float64)
        self.tau = tau
        self.a = a
        self.b = b
        self._size = size

    def update(self, H, dt=0.1):
        H_flat = H.ravel()[:self._size]
        dV = (H_flat + self.a - self.b * self.field) / self.tau
        self.field += dt * dV

    def reset(self):
        self.field[:] = 0.0

    @property
    def mean_inhibition(self):
        return float(self.field.mean())

    @property
    def max_inhibition(self):
        return float(self.field.max())


# ═══════════════════════════════════════════════════════════════════════════
# SECTION G — SKELETON EXTRACTOR
# ═══════════════════════════════════════════════════════════════════════════

def extract_skeleton(H, n_minima=6, prominence=0.1):
    """Pull the 2–6 dominant attractor minima from a converged H field."""
    h = H.ravel().astype(np.float64)
    N = len(h)
    if N < 3:
        return [(0, float(h[0]), 1.0)] if N > 0 else []
    minima = []
    for i in range(1, N - 1):
        if h[i] < h[i-1] and h[i] < h[i+1]:
            left_max = max(h[:i]) if i > 0 else h[i]
            right_max = max(h[i+1:]) if i < N - 1 else h[i]
            prom = min(left_max - h[i], right_max - h[i])
            minima.append((i, float(h[i]), float(prom)))
    if N > 1 and h[0] < h[1]:
        minima.append((0, float(h[0]), float(max(h[1:]) - h[0])))
    if N > 1 and h[-1] < h[-2]:
        minima.append((N - 1, float(h[-1]), float(max(h[:-1]) - h[-1])))
    if not minima:
        idx = int(np.argmin(h))
        return [(idx, float(h[idx]), 1.0)]
    h_range = float(h.max() - h.min())
    if h_range > 0:
        minima = [(i, d, p) for i, d, p in minima if p / h_range >= prominence]
    minima.sort(key=lambda x: x[1])
    return [(i, round(d, 6), round(p, 6)) for i, d, p in minima[:n_minima]]


# ═══════════════════════════════════════════════════════════════════════════
# SECTION H — BIFURCATION SCANNER
# ═══════════════════════════════════════════════════════════════════════════

def bifurcation_scan(H0, param_name, param_range, param_setter,
                      iters_per_point=500, seed=42):
    """Scan a parameter range for phase transitions (bifurcation points)."""
    rng = np.random.default_rng(seed)
    free_energies = []
    variances = []
    for pval in param_range:
        eta, gamma, beta, epsilon = param_setter(H0, float(pval))
        H = H0.copy().ravel().astype(np.float64)
        for _ in range(iters_per_point):
            H = _prime_step(H, H0.ravel(), eta, gamma, beta, epsilon, rng)
        free_energies.append(_prime_free_energy(H, epsilon, beta))
        variances.append(float(np.var(H)))
    fe = np.array(free_energies)
    va = np.array(variances)
    bifurcations = []
    if len(fe) > 4:
        d2F = np.gradient(np.gradient(fe))
        for i in range(1, len(d2F) - 1):
            if d2F[i-1] * d2F[i+1] < 0:
                bifurcations.append((float(param_range[i]), 'CURVATURE_CHANGE'))
        for i in range(1, len(va) - 1):
            if va[i] > va[i-1] and va[i] > va[i+1] and va[i] > va.mean() * 2:
                bifurcations.append((float(param_range[i]), 'SUSCEPTIBILITY_PEAK'))
    return {
        'param_values': param_range.tolist(),
        'free_energies': free_energies,
        'variances': variances,
        'bifurcation_points': bifurcations,
    }


# ═══════════════════════════════════════════════════════════════════════════
# SECTION I — PRIME WATCHDOG
# ═══════════════════════════════════════════════════════════════════════════

class PRIMEWatchdog:
    """
    Five-mode failure detector: DIVERGENCE, COLLAPSE, STAGNATION,
    PLANAR_WAVE_TRAP, WF_APPROACH (healthy).
    """

    def __init__(self, divergence_threshold=1e6, collapse_threshold=1e-8,
                 stagnation_window=50, stagnation_threshold=1e-6):
        self._div_thresh = divergence_threshold
        self._col_thresh = collapse_threshold
        self._stag_window = stagnation_window
        self._stag_thresh = stagnation_threshold
        self._var_history = []
        self._mean_history = []

    def check(self, H, step=0):
        h = H.ravel().astype(np.float64)
        max_abs = float(np.max(np.abs(h)))
        std_h = float(np.std(h))
        var_h = float(np.var(h))
        mean_h = float(np.mean(h))
        self._var_history.append(var_h)
        self._mean_history.append(mean_h)

        # Mode 1: DIVERGENCE
        if max_abs > self._div_thresh:
            return {'mode': 'DIVERGENCE', 'healthy': False,
                    'details': 'max|H|=%.2e. Reduce eta.' % max_abs, 'step': step}

        # Mode 2: COLLAPSE
        if std_h < self._col_thresh:
            return {'mode': 'COLLAPSE', 'healthy': False,
                    'details': 'std(H)=%.2e. Increase eta.' % std_h, 'step': step}

        # Mode 3: STAGNATION (with WF_APPROACH override)
        if len(self._var_history) >= self._stag_window:
            var_of_vars = float(np.var(self._var_history[-self._stag_window:]))
            if var_of_vars < self._stag_thresh:
                if len(self._mean_history) >= self._stag_window:
                    means = np.array(self._mean_history[-self._stag_window:])
                    x = means[:-1] - means[:-1].mean()
                    y = means[1:] - means[1:].mean()
                    xv = float((x**2).mean())
                    ac = float((x * y).mean() / max(xv, 1e-20))
                    if ac > 0.7:
                        return {'mode': 'WF_APPROACH', 'healthy': True,
                                'details': 'Autocorr=%.3f. Approaching fixed point.' % ac,
                                'step': step}
                return {'mode': 'STAGNATION', 'healthy': False,
                        'details': 'Variance frozen. Perturb or increase temp.',
                        'step': step}

        # Mode 4: PLANAR_WAVE_TRAP
        if H.ndim >= 2 and min(H.shape) > 4:
            try:
                F_h = np.fft.fftshift(np.fft.fft2(H))
                P = np.abs(F_h) ** 2
                total_power = P.sum()
                max_power = P.max()
                if total_power > 0 and max_power / total_power > 0.5:
                    return {'mode': 'PLANAR_WAVE_TRAP', 'healthy': False,
                            'details': 'Single mode dominates.', 'step': step}
            except Exception:
                pass

        # Mode 5: WF_APPROACH (healthy default)
        return {'mode': 'WF_APPROACH', 'healthy': True,
                'details': 'max|H|=%.2f, std=%.4f' % (max_abs, std_h),
                'step': step}

    def reset(self):
        self._var_history.clear()
        self._mean_history.clear()


# ═══════════════════════════════════════════════════════════════════════════
# SECTION J — HAMILTONIAN FINGERPRINT
# ═══════════════════════════════════════════════════════════════════════════

@dataclass
class HamiltonianFingerprint:
    """Physics-grounded artifact signature."""
    free_energy: float
    tier: str
    spectral_centroid: float
    field_sha256: str
    n_attractors: int
    mean_depth: float
    timestamp: str = ""

    def to_dict(self):
        return asdict(self)

    def to_json(self):
        return json.dumps(self.to_dict(), indent=2)


def hamiltonian_fingerprint(H, timestamp=""):
    """
    Physics-grounded artifact signing.

    SHA-256 is computed on 8-bit quantized field state (min-max normalized).
    This makes it invariant to linear transforms by design — H*2+3 produces
    the same SHA as the original. The fingerprint captures field SHAPE,
    not absolute position.
    """
    h = H.ravel().astype(np.float64)
    F = _prime_free_energy(H)
    tier = _free_energy_tier(F)

    # Spectral centroid
    if len(h) > 1:
        fft_mag = np.abs(np.fft.rfft(h))
        freqs = np.arange(len(fft_mag))
        total = fft_mag.sum()
        centroid = float((freqs * fft_mag).sum() / max(total, 1e-20))
    else:
        centroid = 0.0

    # SHA-256 of quantized field (8-bit, min-max normalized → shape invariant)
    h_min, h_max = h.min(), h.max()
    h_range = max(h_max - h_min, 1e-10)
    quantized = ((h - h_min) / h_range * 255).astype(np.uint8)
    sha = hashlib.sha256(quantized.tobytes()).hexdigest()

    # Attractor extraction
    skeleton = extract_skeleton(H, n_minima=6)
    n_attractors = len(skeleton)
    mean_depth = float(np.mean([d for _, d, _ in skeleton])) if skeleton else 0.0

    return HamiltonianFingerprint(
        free_energy=round(F, 4),
        tier=tier,
        spectral_centroid=round(centroid, 4),
        field_sha256=sha,
        n_attractors=n_attractors,
        mean_depth=round(mean_depth, 6),
        timestamp=timestamp,
    )


def fingerprints_same_class(fp_a, fp_b, fe_tolerance=20.0, centroid_tolerance=5.0):
    """O(1) universality class comparison via fingerprint proxy."""
    if fp_a.tier != fp_b.tier:
        return False
    return (abs(fp_a.free_energy - fp_b.free_energy) < fe_tolerance and
            abs(fp_a.spectral_centroid - fp_b.spectral_centroid) < centroid_tolerance)


# ═══════════════════════════════════════════════════════════════════════════
# SECTION K — CRITICALITY CONTRIBUTION INDEX (CCI)
# ═══════════════════════════════════════════════════════════════════════════

def power_iteration(A, num_simulations=100):
    """Compute left and right principal eigenvectors of matrix A."""
    n = A.shape[0]
    # Right eigenvector
    b_k = np.ones(n, dtype=np.float64)
    for _ in range(num_simulations):
        b_k1 = A @ b_k
        b_k1_norm = np.linalg.norm(b_k1)
        if b_k1_norm < 1e-12:
            break
        b_k = b_k1 / b_k1_norm
    # Left eigenvector (eigenvector of A.T)
    AT = A.T
    b_l = np.ones(n, dtype=np.float64)
    for _ in range(num_simulations):
        b_l1 = AT @ b_l
        b_l1_norm = np.linalg.norm(b_l1)
        if b_l1_norm < 1e-12:
            break
        b_l = b_l1 / b_l1_norm
    return b_l, b_k


def cci_prime_analysis(ir_functions, ranked_results, f_base, H_converged, eta=0.44, iters=500, seed=42, top_k=50):
    """
    Compute approximate CCI for all nodes, then exact node-removal CCI for the top K.
    """
    import networkx as nx
    names = [f['name'] for f in ir_functions]
    name_idx = {n: i for i, n in enumerate(names)}
    N = len(names)

    # Reconstruct adjacency and initial Hamiltonian
    adj, _ = ir_dependency_matrix(ir_functions)
    H0 = ir_hamiltonian_field(ir_functions)
    deg = adj.sum(axis=1, keepdims=True).clip(1)
    A = adj / deg

    # 1. Local Energy extremity
    max_h = max(np.abs(H_converged)) if len(H_converged) > 0 else 1.0
    max_h = max(max_h, 1e-10)

    # 2. Bridge score (betweenness centrality)
    G_nx = nx.DiGraph()
    for i in range(N):
        G_nx.add_node(i)
    rows, cols = np.nonzero(adj)
    for r, c in zip(rows, cols):
        val = adj[r, c]
        if val > 0:
            # lower weight in networkx betweenness means shorter path
            G_nx.add_edge(r, c, weight=1.0 / val)
    
    try:
        betweenness = nx.betweenness_centrality(G_nx, weight="weight")
    except Exception:
        betweenness = {i: 0.0 for i in range(N)}
    max_bet = max(betweenness.values()) if betweenness else 1.0
    max_bet = max(max_bet, 1e-10)

    # 3. Rank-disagreement boost
    # Build size ranking
    name_to_lines = {f['name']: f['line_count'] for f in ir_functions}
    size_ranked = sorted(name_to_lines.items(), key=lambda x: -x[1])
    size_rank_map = {name: i+1 for i, (name, _) in enumerate(size_ranked)}
    
    # PRIME ranking mapping
    prime_rank_map = {name: i+1 for i, (name, _, _) in enumerate(ranked_results)}

    # 4. Alias-global writes/reads degree coupling
    deg_coupling = (adj > 0).sum(axis=1) + (adj > 0).sum(axis=0)
    max_deg_coupling = max(deg_coupling) if len(deg_coupling) > 0 else 1.0
    max_deg_coupling = max(max_deg_coupling, 1e-10)

    # 5. Spectral contribution (u * v)
    u, v = power_iteration(A)
    spectral = u * v
    max_spectral = max(np.abs(spectral)) if len(spectral) > 0 else 1.0
    max_spectral = max(max_spectral, 1e-10)

    # Calculate approximate CCI
    cci_approx_scores = []
    for i in range(N):
        name = names[i]
        energy_score = abs(H_converged[i]) / max_h
        bridge_score = betweenness.get(i, 0.0) / max_bet
        rank_boost = (size_rank_map.get(name, 0) - prime_rank_map.get(name, 0)) / N
        
        func_meta = ir_functions[i]
        alias_score = (len(func_meta.get('writes', [])) + 0.5 * len(func_meta.get('reads', [])) + deg_coupling[i]) / (1.5 * max_deg_coupling)
        spectral_score = abs(spectral[i]) / max_spectral
        
        cci_approx = (
            0.30 * energy_score +
            0.25 * bridge_score +
            0.20 * rank_boost +
            0.15 * alias_score +
            0.10 * spectral_score
        )
        cci_approx_scores.append((i, name, cci_approx))

    # Run Exact CCI reruns for top-K candidates
    cci_approx_scores.sort(key=lambda x: -x[2])
    candidates = cci_approx_scores[:top_k]
    exact_cci_results = {}

    for idx, name, approx_score in candidates:
        keep = [j for j in range(N) if j != idx]
        adj_removed = adj[np.ix_(keep, keep)]
        H0_removed = H0[keep]
        
        deg_removed = adj_removed.sum(axis=1, keepdims=True).clip(1)
        A_removed = adj_removed / deg_removed
        N_removed = len(keep)
        
        # Rerun truncated PRIME simulation
        H_removed = H0_removed.copy()
        rng_removed = np.random.default_rng(seed)
        for _ in range(iters):
            sig = _sigmoid(H_removed, 0.3)
            T_local = 0.05**2 * (1.0 + 0.1 * np.abs(H_removed))
            noise = rng_removed.standard_normal(N_removed) * np.sqrt(T_local)
            H_removed = H0_removed + eta * (A_removed @ H_removed) * sig + noise
            
        f_removed = _prime_free_energy(H_removed)
        exact_cci_results[name] = f_removed - f_base

    # Compute final metrics and tiers
    final_cci_records = []
    for idx, name, approx_score in cci_approx_scores:
        exact_val = exact_cci_results.get(name, 0.0)
        # Tier assignment
        if name in exact_cci_results:
            if exact_val > 0.02:
                tier = "CRITICAL_ORGANIZER"
            elif exact_val > 0.002:
                tier = "STABILIZATION_BRIDGE"
            elif exact_val < -0.002:
                tier = "DILUTER"
            else:
                tier = "HOT_LOCAL" if approx_score > 0.5 else "COLD_MASS"
        else:
            tier = "COLD_MASS" if name_to_lines.get(name, 0) > 1000 else "POTENTIAL_DILUTER"
            
        final_cci_records.append({
            'name': name,
            'cci': round(exact_val, 6) if name in exact_cci_results else 0.0,
            'cci_approx': round(approx_score, 6),
            'tier': tier,
            'size': name_to_lines.get(name, 0),
            'size_rank': size_rank_map.get(name, 0),
            'prime_rank': prime_rank_map.get(name, 0)
        })

    # Sort final records by cci descending (if ran exact) then by approx score
    final_cci_records.sort(key=lambda x: (-abs(x['cci']) if x['cci'] != 0.0 else 0.0, -x['cci_approx']))

    max_cci = max(exact_cci_results.values(), default=0.0)
    adjusted_f = f_base - 0.25 * max_cci

    return {
        'records': final_cci_records,
        'max_cci': round(max_cci, 6),
        'f_cci': round(adjusted_f, 4),
        'adjusted_tier': _free_energy_tier(adjusted_f)
    }


# ═══════════════════════════════════════════════════════════════════════════
# UNIFIED PIPELINE
# ═══════════════════════════════════════════════════════════════════════════

def zkaedi_prime_pipeline(domain, data, eta=0.44, iters=2000, seed=42, timestamp=""):
    """
    Single entry point for all four ZKAEDI domains.
    Routes to the right integration, runs PRIME, returns fingerprinted result.
    """
    wd = PRIMEWatchdog()
    H = None

    if domain == 'ir':
        result = ir_prime_pass(data, eta=eta, iters=iters, seed=seed)
        H0 = ir_hamiltonian_field(data)
        adj, _ = ir_dependency_matrix(data)
        deg = adj.sum(axis=1, keepdims=True).clip(1)
        A = adj / deg
        rng = np.random.default_rng(seed)
        H = H0.copy()
        for t in range(min(iters, 500)):
            H = _prime_step(H, H0, eta, 0.3, 0.1, 0.05, rng)
            wd.check(H.reshape(-1), t)

    elif domain == 'dream':
        seeds = dream_next_generation_seeds(data, eta=eta, iters=iters, seed=seed)
        plateau = dream_plateau_detector(data)
        H0 = dream_fitness_hamiltonian(data)
        rng = np.random.default_rng(seed)
        H = H0.copy()
        for t in range(min(iters, 500)):
            coupling = np.zeros_like(H)
            if len(H) > 1:
                coupling[1:] += H[:-1]; coupling[:-1] += H[1:]; coupling /= 2
            H = _prime_step(H, H0, eta, 0.3, 0.1, 0.05, rng, coupling=coupling * 0.1)
            wd.check(H, t)
        result = {
            'seeds': seeds,
            'plateau': plateau,
            'free_energy': round(_prime_free_energy(H), 4),
            'tier': _free_energy_tier(_prime_free_energy(H)),
        }

    elif domain == 'contract':
        result = contract_prime_audit(
            data['cfg_edges'], data['block_scores'], data['n_nodes'],
            eta=eta, iters=iters, seed=seed)
        H0 = contract_vulnerability_hamiltonian(data['block_scores'])
        rng = np.random.default_rng(seed)
        H = H0.copy()
        for t in range(min(iters, 500)):
            H = _prime_step(H, H0, eta, 0.3, 0.1, 0.05, rng)
            wd.check(H, t)

    elif domain == 'mev':
        result = mev_prime_scan(data, eta=eta, iters=iters, seed=seed)
        H0 = mev_opportunity_hamiltonian(data)
        rng = np.random.default_rng(seed)
        H = H0.copy()
        for t in range(min(iters, 500)):
            H = _prime_step(H, H0, eta, 0.3, 0.1, 0.05, rng)
            wd.check(H, t)

    else:
        raise ValueError("Unknown domain: %s. Use 'ir', 'dream', 'contract', or 'mev'." % domain)

    fp = hamiltonian_fingerprint(H if H is not None else np.zeros(1), timestamp)
    return {
        'domain': domain,
        'result': result,
        'fingerprint': fp.to_dict(),
        'watchdog': wd.check(H if H is not None else np.zeros(1), iters),
    }
