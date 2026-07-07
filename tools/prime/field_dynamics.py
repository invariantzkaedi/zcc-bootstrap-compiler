"""
Field-dynamics verification for ZKAEDI PRIME.

Independently operationalizes and measures the five "PASS" claims from the
corrigendum docstring that had no corresponding code:

    1. recursive coupling is real          (||H* - H0||_inf on open cells)
    2. nonlinear attractor sharpening      (ridge vs basin gradient ratio)
    3. fixed-point convergence             (steps to reach float64 tolerance)
    4. bifurcation / phase transition      (critical eta where recursion diverges)
    5. bounded stochastic attractor        (steady-state step-to-step drift)

None of these methodologies were specified anywhere in the original
docstring, so there is no ground truth to match against -- these are new,
explicit, documented operationalizations. Numbers here should NOT be
expected to reproduce the original figures (4.64, x1.43/x1.13, t~50,
eta_c~1.05, 0.439+/-0.001) exactly; the point is that these numbers are now
real, reproducible, and their method is written down.
"""
import logging
from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np

from zkaedi_prime import Maze, hamiltonian_field, _WALL_ENERGY

logger = logging.getLogger("zkaedi_prime.field_dynamics")

_CLIP = 500.0
_DIVERGE_THRESHOLD = 1e8  # |H| beyond this on any open cell => "diverged"


def _open_mask(maze: Maze) -> np.ndarray:
    return np.asarray(maze.grid) == 1


def _sigmoid(z: np.ndarray) -> np.ndarray:
    out = np.empty_like(z, dtype=np.float64)
    pos = z >= 0
    out[pos] = 1.0 / (1.0 + np.exp(-z[pos]))
    ez = np.exp(z[~pos])
    out[~pos] = ez / (1.0 + ez)
    return out


def _deterministic_step(H, H0, eta, gamma):
    sig = _sigmoid(gamma * np.clip(H, -_CLIP, _CLIP))
    return H0 + eta * H * sig


# ------------------------------------------------------------------
# 1 & 3: recursive coupling magnitude + fixed-point convergence speed
# ------------------------------------------------------------------
@dataclass
class RecursionResult:
    linf_open_cells: float
    linf_all_cells: float
    mean_open_cells: float
    rms_open_cells: float
    converged_at_step: Optional[int]
    H_final: np.ndarray


def run_recursion_to_fixed_point(maze: Maze, eta: float = 0.4, gamma: float = 0.3,
                                  max_steps: int = 2000, tol: float = 1e-10,
                                  patience: int = 5) -> RecursionResult:
    """
    Iterate H_t = H0 + eta*H_{t-1}*sigmoid(gamma*H_{t-1}) from H_0 = H0
    with NO noise (eps=0), until the step-to-step Linf delta over the
    whole matrix drops below `tol` for `patience` consecutive steps, or
    max_steps is hit.

    Returns the Linf distance of the fixed point from the static baseline,
    both over open (navigable) cells only and over the full matrix
    (which includes wall cells and is dominated by them by construction).

    CAVEAT on linf_open_cells: make_maze() always forces (0,0) and
    (n-1,n-1) open, and (0,0) is also always the Euclidean-farthest point
    from a goal at (n-1,n-1) on a square grid -- so for fixed n, the
    open-cell argmax of H0 (and hence often of |H*-H0|) lands on the same
    cell regardless of interior wall layout, making linf_open_cells
    largely maze-invariant and not very diagnostic of "coupling" per se.
    mean_open_cells / rms_open_cells are distributed statistics that
    actually vary with maze structure and are the more meaningful summary.
    """
    H0 = hamiltonian_field(maze)
    mask = _open_mask(maze)
    H = H0.copy()
    converged_at = None
    quiet_run = 0
    for t in range(1, max_steps + 1):
        H_new = _deterministic_step(H, H0, eta, gamma)
        delta = float(np.max(np.abs(H_new - H)))
        H = H_new
        if delta < tol:
            quiet_run += 1
            if quiet_run >= patience and converged_at is None:
                converged_at = t - patience + 1
                break
        else:
            quiet_run = 0
    dev_open = np.abs(H[mask] - H0[mask])
    return RecursionResult(
        linf_open_cells=float(np.max(dev_open)),
        linf_all_cells=float(np.max(np.abs(H - H0))),
        mean_open_cells=float(np.mean(dev_open)),
        rms_open_cells=float(np.linalg.norm(dev_open) / np.sqrt(dev_open.size)),
        converged_at_step=converged_at,
        H_final=H,
    )


# ------------------------------------------------------------------
# 2: ridge / basin sharpening
# ------------------------------------------------------------------
def _masked_gradient_magnitude(field: np.ndarray, open_mask: np.ndarray) -> np.ndarray:
    """
    Gradient magnitude at each open cell, using only finite differences
    between open-open neighbor pairs (a difference across a wall boundary
    is not a meaningful 'field steepness' -- it's just the 1e6 wall
    energy spike -- so those directions are excluded rather than
    contributing a fake gradient).

    Cells with fewer than one valid axis of measurement get NaN and are
    excluded from downstream statistics.
    """
    n, m = field.shape
    gx = np.full((n, m), np.nan)
    gy = np.full((n, m), np.nan)
    for i in range(n):
        for j in range(m):
            if not open_mask[i, j]:
                continue
            # x-direction (rows)
            vals = []
            if i > 0 and open_mask[i - 1, j]:
                vals.append(field[i, j] - field[i - 1, j])
            if i < n - 1 and open_mask[i + 1, j]:
                vals.append(field[i + 1, j] - field[i, j])
            if vals:
                gx[i, j] = np.mean(vals)
            # y-direction (cols)
            vals = []
            if j > 0 and open_mask[i, j - 1]:
                vals.append(field[i, j] - field[i, j - 1])
            if j < m - 1 and open_mask[i, j + 1]:
                vals.append(field[i, j + 1] - field[i, j])
            if vals:
                gy[i, j] = np.mean(vals)
    gx = np.nan_to_num(gx, nan=0.0)
    gy = np.nan_to_num(gy, nan=0.0)
    mag = np.hypot(gx, gy)
    mag[~open_mask] = np.nan
    return mag


@dataclass
class SharpeningResult:
    ridge_ratio: float
    basin_ratio: float
    n_ridge_cells: int
    n_basin_cells: int


def measure_attractor_sharpening(maze: Maze, eta: float = 0.4, gamma: float = 0.3,
                                  max_steps: int = 2000, quartile: float = 0.25
                                  ) -> SharpeningResult:
    """
    Ridges := open cells in the top `quartile` of |grad(H0)| (steepest
    part of the static distance field). Basins := bottom `quartile`.

    Reports, for each group, mean(|grad(H*)|) / mean(|grad(H0)|) -- i.e.
    how much steeper (>1) or flatter (<1) the converged recursive field
    is relative to the static baseline, in that region.
    """
    mask = _open_mask(maze)
    rec = run_recursion_to_fixed_point(maze, eta=eta, gamma=gamma, max_steps=max_steps)
    grad0 = _masked_gradient_magnitude(hamiltonian_field(maze), mask)
    grad_star = _masked_gradient_magnitude(rec.H_final, mask)

    valid = mask & ~np.isnan(grad0)
    vals0 = grad0[valid]
    idx = np.argsort(vals0)
    k = max(1, int(len(vals0) * quartile))
    basin_thresh = vals0[idx[k - 1]]
    ridge_thresh = vals0[idx[-k]]

    basin_cells = valid & (grad0 <= basin_thresh)
    ridge_cells = valid & (grad0 >= ridge_thresh)

    ridge_ratio = float(np.mean(grad_star[ridge_cells]) / np.mean(grad0[ridge_cells]))
    basin_ratio = float(np.mean(grad_star[basin_cells]) / np.mean(grad0[basin_cells]))

    return SharpeningResult(
        ridge_ratio=ridge_ratio,
        basin_ratio=basin_ratio,
        n_ridge_cells=int(ridge_cells.sum()),
        n_basin_cells=int(basin_cells.sum()),
    )


# ------------------------------------------------------------------
# 4: bifurcation point eta_c
# ------------------------------------------------------------------
def _diverges(maze: Maze, eta: float, gamma: float, max_steps: int) -> bool:
    H0 = hamiltonian_field(maze)
    H = H0.copy()
    mask = _open_mask(maze)
    for _ in range(max_steps):
        H = _deterministic_step(H, H0, eta, gamma)
        if np.max(np.abs(H[mask])) > _DIVERGE_THRESHOLD:
            return True
    return False


def find_bifurcation_eta(maze: Maze, gamma: float = 0.3, lo: float = 0.80,
                          hi: float = 1.50, max_steps: int = 2000,
                          bisection_tol: float = 1e-3) -> float:
    """
    Bisection search over eta in [lo, hi] for the boundary between
    'bounded on open cells within max_steps' and 'exceeds 1e8 on some
    open cell within max_steps'. Caveat: this is a finite-step-budget
    empirical estimate, not a closed-form threshold -- eta values just
    above the true asymptotic critical point may look bounded here
    simply because they haven't diverged yet within max_steps.
    """
    if _diverges(maze, hi, gamma, max_steps):
        pass
    else:
        logger.warning("hi=%.3f did not diverge within max_steps=%d; "
                        "raising hi", hi, max_steps)
        while not _diverges(maze, hi, gamma, max_steps) and hi < 10.0:
            hi *= 1.2

    assert not _diverges(maze, lo, gamma, max_steps), \
        f"lo={lo} unexpectedly diverges -- widen search range downward"

    while hi - lo > bisection_tol:
        mid = 0.5 * (lo + hi)
        if _diverges(maze, mid, gamma, max_steps):
            hi = mid
        else:
            lo = mid
    return 0.5 * (lo + hi)


# ------------------------------------------------------------------
# 5: bounded stochastic attractor drift
# ------------------------------------------------------------------
@dataclass
class DriftResult:
    mean: float
    sem: float
    n_samples: int


def measure_stochastic_drift(maze: Maze, eta: float = 0.4, gamma: float = 0.3,
                              beta: float = 0.1, eps: float = 0.05,
                              burn_in: int = 300, sample_steps: int = 1500,
                              n_seeds: int = 5) -> DriftResult:
    """
    Runs the full noisy recursion (matching _prime_step's math) from H0,
    discards `burn_in` steps to reach the stochastic steady state, then
    records mean(|H_t - H_{t-1}|) over open cells for `sample_steps`
    further steps. 'Drift' = the average per-step field fluctuation size
    once the system is no longer systematically converging, just
    fluctuating around its attractor. Repeated across `n_seeds`
    independent RNG streams; reports mean +/- standard error of the mean
    across all (seed x step) samples.
    """
    H0 = hamiltonian_field(maze)
    mask = _open_mask(maze)
    all_samples = []
    for seed in range(n_seeds):
        rng = np.random.default_rng(seed)
        H = H0.copy()
        for _ in range(burn_in):
            sig = _sigmoid(gamma * np.clip(H, -_CLIP, _CLIP))
            noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
            H = H0 + eta * H * sig + eps * noise
        for _ in range(sample_steps):
            H_prev = H
            sig = _sigmoid(gamma * np.clip(H, -_CLIP, _CLIP))
            noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
            H = H0 + eta * H * sig + eps * noise
            all_samples.append(float(np.mean(np.abs(H[mask] - H_prev[mask]))))
    arr = np.asarray(all_samples)
    return DriftResult(mean=float(arr.mean()),
                        sem=float(arr.std(ddof=1) / np.sqrt(len(arr))),
                        n_samples=len(arr))
