"""
ZKAEDI PRIME — Recursively Coupled Hamiltonian Maze Solver
===========================================================
    H_t = H0 + eta * H_{t-1} * sigmoid(gamma * H_{t-1})
             + eps * N(0, 1 + beta * |H_{t-1}|)

CORRIGENDUM (verified 2026-07-03, 60 BFS-solvable 25x25 mazes, wall density 0.32)
---------------------------------------------------------------------------------
The field dynamics are everything they claim to be AS A DYNAMICAL SYSTEM
(recursive coupling, attractor sharpening, fixed-point convergence, a
bifurcation near eta_c ~ 1.05, a bounded stochastic attractor). But the
NAVIGATION causality was backwards:

  v1 recursion-only (eps=0)  :  0/60 solved   <- recursion navigates NOTHING
  v1 noise-only     (eta=0)  : 11/60 solved   <- the noise was doing the walking
  v1 full PRIME              :  9/60 solved
  v2 scar-only               : 60/60, median 90 steps
  v2 scar + noise            : 60/60, median 79 steps, 1.6x optimal

The SCAR term (visited-cell memory) is the navigation algorithm. eps noise
is a consistent ~15% step reduction (tie-breaking). eta is a FIELD-SHAPING
operator with zero navigational lift. One equation, two regimes: use eta
where the field is the product (worldgen, H0 scoring); use scars + eps
where a walker must move.

v3 ADDENDUM (verified 2026-07-07, same 60-maze pool)
----------------------------------------------------
v3 gives the scarred walker an explicit path stack + dead-end sealing
(backtracking). Measured:

  v3 backtrack + noise : 60/60, median 75 total moves (1.5x),
                         final SIMPLE path 1.12x optimal
  v3 backtrack, eps=0  : 60/60, median 92 total moves (1.8x), path 1.14x

The near-optimal SIMPLE path (meta['path_len']) is the real v3 win: the
returned route carries no dead-end detours. Locomotion effort (Solution.steps,
total forward+backtrack moves) is only marginally better than v2 because the
body must physically retrace. eps still earns its ~15% via tie-breaking; eta
still buys nothing for navigation (default 0.0 in v3).

v3 SWEEP (9-cell parameter sweep: 3 sizes x 3 densities, 40 mazes each)
------------------------------------------------------------------------
| size | density | v2 solved | v2 moves | v3 solved | v3 moves | v3 path  |
|------|---------|-----------|----------|-----------|----------|----------|
| 15   | 0.25    | 40/40     | 1.14x    | 40/40     | 1.14x    | 1.07x    |
| 15   | 0.35    | 40/40     | 1.50x    | 40/40     | 1.80x    | 1.07x    |
| 15   | 0.45    | 40/40     | 1.64x    | 40/40     | 1.61x    | 1.07x    |
| 25   | 0.25    | 40/40     | 1.25x    | 40/40     | 1.25x    | 1.13x    |
| 25   | 0.35    | 40/40     | 2.22x    | 40/40     | 2.55x    | 1.15x    |
| 25   | 0.45    | 40/40     | 2.09x    | 40/40     | 2.25x    | 1.06x    |
| 35   | 0.25    | 40/40     | 1.34x    | 40/40     | 1.35x    | 1.17x    |
| 35   | 0.35    | 40/40     | 2.33x    | 40/40     | 2.76x    | 1.19x    |
| 35   | 0.45    | 40/40     | 2.35x    | 40/40     | 2.46x    | 1.08x    |

Run the gauntlet:  python zkaedi_prime.py --gauntlet
Run the sweep:     python zkaedi_prime.py --sweep
"""

from __future__ import annotations
import argparse
import time
import warnings
from collections import deque
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

import numpy as np

__version__ = "3.0.0"
__all__ = [
    "Maze", "Solution", "hamiltonian_field", "make_maze", "bfs_len",
    "solve_zkaedi_prime", "solve_zkaedi_prime_v2", "solve_zkaedi_prime_v3",
    "run_gauntlet", "run_sweep",
]

_MOVES = ((0, 1), (1, 0), (0, -1), (-1, 0))
_WALL_ENERGY = 1e6


# ----------------------------------------------------------------------
# minimal maze / solution containers (duck-type compatible)
# ----------------------------------------------------------------------
@dataclass
class Maze:
    grid: np.ndarray
    start: Tuple[int, int]
    end: Tuple[int, int]

    @property
    def size(self) -> Tuple[int, int]:
        return self.grid.shape


@dataclass
class Solution:
    path: List[Tuple[int, int]]
    steps: int
    time_taken: float
    optimal: bool
    algorithm: str
    meta: dict = field(default_factory=dict)


def make_maze(n: int = 25, seed: int = 0, wall_density: float = 0.32) -> Maze:
    r = np.random.default_rng(seed)
    g = (r.random((n, n)) > wall_density).astype(int)
    g[0, 0] = g[n - 1, n - 1] = 1
    return Maze(grid=g, start=(0, 0), end=(n - 1, n - 1))


def bfs_len(maze: Maze) -> Optional[int]:
    """Optimal path length, or None if unsolvable. Use to build FAIR pools."""
    n, m = maze.size
    dist = {maze.start: 0}
    q = deque([maze.start])
    while q:
        x, y = q.popleft()
        if (x, y) == maze.end:
            return dist[(x, y)]
        for dx, dy in _MOVES:
            nx, ny = x + dx, y + dy
            if 0 <= nx < n and 0 <= ny < m and maze.grid[nx][ny] == 1 \
                    and (nx, ny) not in dist:
                dist[(nx, ny)] = dist[(x, y)] + 1
                q.append((nx, ny))
    return None


def hamiltonian_field(maze: Maze) -> np.ndarray:
    """H0: Euclidean distance-to-goal potential; walls at +1e6."""
    n, m = maze.size
    ii, jj = np.meshgrid(np.arange(n), np.arange(m), indexing="ij")
    H = np.hypot(ii - maze.end[0], jj - maze.end[1]).astype(float)
    H[np.asarray(maze.grid) == 0] = _WALL_ENERGY
    return H


def _prime_step(H, H_base, eta, gamma, beta, eps, rng):
    """One PRIME field update with overflow guards (walls sit at 1e6)."""
    sig = 1.0 / (1.0 + np.exp(-gamma * np.clip(H, -500.0, 500.0)))
    H = H_base + eta * H * sig
    if eps:
        H = H + eps * rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
    return H


def _resolve_eps(eps, sigma, default):
    if sigma is not None:            # deprecated alias from the v1 spec
        warnings.warn("`sigma` is deprecated (it collides with the sigmoid's "
                      "role in the same equation); use `eps`.",
                      DeprecationWarning, stacklevel=3)
        return sigma
    return default if eps is None else eps


# ----------------------------------------------------------------------
# v1 — preserved for A/B. Measured: 9/60 on the standard gauntlet.
# ----------------------------------------------------------------------
def solve_zkaedi_prime(self_or_maze, maze=None, eta=0.4, gamma=0.3, beta=0.1,
                       eps=None, sigma=None, seed=None, max_steps=50000):
    """ZKAEDI PRIME v1 — recursively coupled Hamiltonian solver (original).

    HONEST BEHAVIOR: the recursive eta term converges the field to a static
    fixed point in ~50 iterations; after that the walker is memoryless greedy
    descent on a frozen potential and traps in two-cell oscillations. All
    solves are attributable to the eps noise term. 9/60. Prefer v2/v3.
    """
    if maze is None:
        maze = self_or_maze
    eps = _resolve_eps(eps, sigma, 0.05)
    start_time = time.time()
    rng = np.random.default_rng(seed)
    H_base = hamiltonian_field(maze)
    H = H_base.copy()
    path = [maze.start]
    x, y = maze.start
    goal = maze.end
    n, m = maze.size
    for _t in range(max_steps):
        if (x, y) == goal:
            return Solution(path=path, steps=len(path) - 1,
                            time_taken=time.time() - start_time,
                            optimal=False, algorithm="ZKAEDI_PRIME",
                            meta={"eta": eta, "eps": eps})
        H = _prime_step(H, H_base, eta, gamma, beta, eps, rng)
        best = None
        for dx, dy in _MOVES:
            nx, ny = x + dx, y + dy
            if 0 <= nx < n and 0 <= ny < m and maze.grid[nx][ny] == 1:
                if best is None or H[nx, ny] < best[0]:
                    best = (H[nx, ny], (nx, ny))
        if best is None:
            break
        x, y = best[1]
        path.append((x, y))
    return None


# ----------------------------------------------------------------------
# v2 — scarred field. Measured: 60/60, median 79 steps, 1.6x optimal.
# ----------------------------------------------------------------------
def solve_zkaedi_prime_v2(self_or_maze, maze=None, eta=0.4, gamma=0.3, beta=0.1,
                          eps=None, sigma=None, kick=2.0, decay=1.0,
                          seed=None, max_steps=50000):
    """ZKAEDI PRIME v2 — scarred-field corrigendum.

    Reactive walker: greedy descent on a static field plus a decaying scar
    layer that records visited cells (repulsive via positive `kick`). The
    scar term is the actual navigation mechanism. 60/60, ~1.6x optimal.
    """
    if maze is None:
        maze = self_or_maze
    eps = _resolve_eps(eps, sigma, 0.05)
    start_time = time.time()
    rng = np.random.default_rng(seed)
    for name, val in (("eta", eta), ("gamma", gamma), ("beta", beta),
                      ("eps", eps), ("kick", kick)):
        if not isinstance(val, (int, float)) or isinstance(val, bool):
            raise TypeError(f"{name} must be numeric, got {type(val).__name__}")
        if val < 0:
            raise ValueError(f"{name} must be non-negative, got {val}")
    if not isinstance(decay, (int, float)) or isinstance(decay, bool) or not (0.0 < decay <= 1.0):
        raise ValueError(f"decay must be in (0, 1], got {decay!r}")
    if not isinstance(max_steps, int) or isinstance(max_steps, bool) or max_steps <= 0:
        raise ValueError(f"max_steps must be a positive int, got {max_steps!r}")
    try:
        n, m = maze.size
    except (AttributeError, TypeError, ValueError) as exc:
        raise ValueError(f"maze.size must unpack to (n, m): {exc}")
    H_static = hamiltonian_field(maze).copy()
    if H_static.shape != (n, m):
        raise ValueError(f"field shape {H_static.shape} != maze.size {(n, m)}")
    try:
        x, y = maze.start
        goal = maze.end
    except (AttributeError, TypeError, ValueError) as exc:
        raise ValueError(f"maze.start/maze.end must be (row, col) pairs: {exc}")
    scars = np.zeros_like(H_static)
    H = H_static.copy()
    path = [(x, y)]
    for _t in range(max_steps):
        if (x, y) == goal:
            return Solution(path=path, steps=len(path) - 1,
                            time_taken=time.time() - start_time,
                            optimal=False, algorithm="ZKAEDI_PRIME_V2",
                            meta={"eta": eta, "eps": eps, "kick": kick,
                                  "decay": decay})
        if decay != 1.0:
            scars *= decay
        scars[x, y] += kick
        H_base = H_static + scars
        clipped_H = np.clip(H, -500.0, 500.0)
        sigmoid = 1.0 / (1.0 + np.exp(-gamma * clipped_H))
        noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
        H = H_base + eta * H * sigmoid + eps * noise
        best_move = None
        for dx, dy in _MOVES:
            nx, ny = x + dx, y + dy
            if 0 <= nx < n and 0 <= ny < m and maze.grid[nx][ny] == 1:
                if best_move is None or H[nx, ny] < best_move[0]:
                    best_move = (H[nx, ny], (nx, ny))
        if best_move is None:
            break
        x, y = best_move[1]
        path.append((x, y))
    return None


# ----------------------------------------------------------------------
# v3 — scarred walker + backtracking. Measured: 60/60, path 1.12x optimal.
# ----------------------------------------------------------------------
def solve_zkaedi_prime_v3(self_or_maze, maze=None, eta=0.0, gamma=0.3, beta=0.1,
                          eps=None, sigma=None, kick=2.0, decay=1.0,
                          seal=1e4, seed=None, max_steps=50000):
    """ZKAEDI PRIME v3 — scarred walker with backtracking + dead-end sealing.

    Same scar-navigated field as v2, but the walker keeps an explicit path
    stack. When every open neighbor is already on the path or sealed, the
    current cell is a dead end: it is sealed (never re-entered), given a large
    scar, and the walker retraces one cell. This produces a near-optimal
    SIMPLE path (meta['path_len'] ~ 1.1x optimal on the standard gauntlet).

    Solution.steps counts TOTAL locomotion (forward + backtrack moves), for
    apples-to-apples comparison with v1/v2; the final simple-path length is in
    meta['path_len']. eta defaults to 0.0 (no navigational lift; see
    corrigendum). eps still earns ~15% via frontier tie-breaking.
    """
    if maze is None:
        maze = self_or_maze
    eps = _resolve_eps(eps, sigma, 0.05)
    for name, val in (("eta", eta), ("gamma", gamma), ("beta", beta),
                      ("eps", eps), ("kick", kick), ("seal", seal)):
        if not isinstance(val, (int, float)) or isinstance(val, bool):
            raise TypeError(f"{name} must be numeric, got {type(val).__name__}")
        if val < 0:
            raise ValueError(f"{name} must be non-negative, got {val}")
    if not isinstance(decay, (int, float)) or isinstance(decay, bool) or not (0.0 < decay <= 1.0):
        raise ValueError(f"decay must be in (0, 1], got {decay!r}")
    if not isinstance(max_steps, int) or isinstance(max_steps, bool) or max_steps <= 0:
        raise ValueError(f"max_steps must be a positive int, got {max_steps!r}")
    t0 = time.time()
    rng = np.random.default_rng(seed)
    try:
        n, m = maze.size
    except (AttributeError, TypeError, ValueError) as exc:
        raise ValueError(f"maze.size must unpack to (n, m): {exc}")
    H_static = hamiltonian_field(maze).copy()
    if H_static.shape != (n, m):
        raise ValueError(f"field shape {H_static.shape} != maze.size {(n, m)}")
    try:
        x, y = maze.start
        goal = maze.end
    except (AttributeError, TypeError, ValueError) as exc:
        raise ValueError(f"maze.start/maze.end must be (row, col) pairs: {exc}")

    scars = np.zeros_like(H_static)
    H = H_static.copy()
    stack = [(x, y)]
    on_path = {(x, y)}
    sealed = set()
    trajectory = [(x, y)]
    total_moves = 0

    for _t in range(max_steps):
        if (x, y) == goal:
            return Solution(path=trajectory, steps=total_moves,
                            time_taken=time.time() - t0, optimal=False,
                            algorithm="ZKAEDI_PRIME_V3",
                            meta={"path_len": len(stack) - 1, "eta": eta,
                                  "eps": eps, "kick": kick, "decay": decay,
                                  "seal": seal})
        if decay != 1.0:
            scars *= decay
        scars[x, y] += kick
        H_base = H_static + scars
        clipped_H = np.clip(H, -500.0, 500.0)
        sigmoid = 1.0 / (1.0 + np.exp(-gamma * clipped_H))
        noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
        H = H_base + eta * H * sigmoid + eps * noise

        best = None
        for dx, dy in _MOVES:
            nx, ny = x + dx, y + dy
            if 0 <= nx < n and 0 <= ny < m and maze.grid[nx][ny] == 1 \
                    and (nx, ny) not in on_path and (nx, ny) not in sealed:
                if best is None or H[nx, ny] < best[0]:
                    best = (H[nx, ny], (nx, ny))

        if best is not None:
            x, y = best[1]
            stack.append((x, y))
            on_path.add((x, y))
            trajectory.append((x, y))
            total_moves += 1
        else:
            sealed.add((x, y))
            scars[x, y] += seal
            on_path.discard((x, y))
            stack.pop()
            if not stack:
                return None            # start fully boxed in
            x, y = stack[-1]
            trajectory.append((x, y))
            total_moves += 1
    return None


# ----------------------------------------------------------------------
# gauntlet — reproduces the corrigendum ledger (+ v3 rows)
# ----------------------------------------------------------------------
def run_gauntlet(n_mazes=60, size=25, seed0=7000, verbose=True):
    pool, s = [], 0
    while len(pool) < n_mazes:
        mz = make_maze(size, s)
        L = bfs_len(mz)
        if L is not None:
            pool.append((mz, L))
        s += 1
    variants = [
        ("v2 scar-only   (eta=0, eps=0)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=0.0, eps=0.0, seed=sd)),
        ("v2 scar+noise  (eta=0, eps=.05)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=0.0, eps=0.05, seed=sd)),
        ("v2 scar+PRIME  (eta=.4, eps=.05)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, seed=sd)),
        ("v3 backtrack   (eps=.05)",
         lambda mz, sd: solve_zkaedi_prime_v3(mz, eps=0.05, seed=sd)),
        ("v1 full PRIME  (baseline)",
         lambda mz, sd: solve_zkaedi_prime(mz, seed=sd)),
        ("NEG CONTROL recursion-only",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=0.4, eps=0.0, kick=0.0,
                                              seed=sd)),
    ]
    results = {}
    if verbose:
        print(f"pool: {n_mazes} BFS-solvable {size}x{size} mazes "
              f"(scanned {s} seeds), optimal median "
              f"{int(np.median([L for _, L in pool]))}")
        print(f"{'variant':36s} solved   med steps  med steps/opt  med path/opt")
    for name, fn in variants:
        solved, ratios, path_ratios = [], [], []
        for i, (mz, L) in enumerate(pool):
            r = fn(mz, seed0 + i)
            if r is not None:
                solved.append(r.steps)
                ratios.append(r.steps / L)
                pl = r.meta.get("path_len")
                path_ratios.append((pl / L) if pl is not None else (r.steps / L))
        results[name] = (len(solved),
                         int(np.median(solved)) if solved else None,
                         float(np.median(ratios)) if ratios else None,
                         float(np.median(path_ratios)) if path_ratios else None)
        if verbose:
            k, ms, ro, pr = results[name]
            print(f"{name:36s} {k:3d}/{n_mazes}   "
                  f"{ms if ms is not None else '---':>6}     "
                  f"{f'{ro:.1f}x' if ro else '---':>6}     "
                  f"{f'{pr:.2f}x' if pr else '---':>6}")
    v2 = results["v2 scar+noise  (eta=0, eps=.05)"]
    v3 = results["v3 backtrack   (eps=.05)"]
    neg = results["NEG CONTROL recursion-only"]
    ok = (v2[0] == n_mazes and v2[2] <= 2.0
          and v3[0] == n_mazes and v3[3] <= 1.3      # v3 simple path near-optimal
          and neg[0] <= 2)
    if verbose:
        print("GATES:", "PASS" if ok else "FAIL",
              "(v2 60/60 & <=2.0x; v3 60/60 & simple path <=1.3x optimal; "
              "neg control <=2/60 — the LOW control number is EXPECTED)")
    results["_gates_pass"] = ok
    return results


def run_sweep(n_mazes=40, seed0=7000, verbose=True):
    """Run a 9-cell parameter sweep across sizes (15, 25, 35) and densities (0.25, 0.35, 0.45)."""
    sizes = [15, 25, 35]
    densities = [0.25, 0.35, 0.45]
    if verbose:
        print(f"Running sweep: {len(sizes)} sizes x {len(densities)} densities, {n_mazes} mazes each (seed0={seed0})")
        print(f"| {'size':<4s} | {'density':<7s} | {'v2 solved':<9s} | {'v2 moves':<8s} | {'v3 solved':<9s} | {'v3 moves':<8s} | {'v3 path':<8s} |")
        print(f"|{'-'*6}|{'-'*9}|{'-'*11}|{'-'*10}|{'-'*11}|{'-'*10}|{'-'*10}|")
    
    table = []
    for size in sizes:
        for density in densities:
            pool = []
            s = 0
            while len(pool) < n_mazes:
                mz = make_maze(size, s, wall_density=density)
                L = bfs_len(mz)
                if L is not None:
                    pool.append((mz, L))
                s += 1
            
            # v2 scar+noise (eta=0, eps=0.05)
            v2_solved, v2_ratios = [], []
            for i, (mz, L) in enumerate(pool):
                sol = solve_zkaedi_prime_v2(mz, eta=0.0, eps=0.05, seed=seed0 + i)
                if sol is not None:
                    v2_solved.append(sol.steps)
                    v2_ratios.append(sol.steps / L)
            
            # v3 backtrack (eps=0.05)
            v3_solved, v3_ratios, v3_path_ratios = [], [], []
            for i, (mz, L) in enumerate(pool):
                sol = solve_zkaedi_prime_v3(mz, eps=0.05, seed=seed0 + i)
                if sol is not None:
                    v3_solved.append(sol.steps)
                    v3_ratios.append(sol.steps / L)
                    pl = sol.meta.get("path_len")
                    v3_path_ratios.append(pl / L if pl is not None else (sol.steps / L))
            
            v2_pct = len(v2_solved) / n_mazes
            v3_pct = len(v3_solved) / n_mazes
            v2_moves_opt = np.median(v2_ratios) if v2_ratios else 0.0
            v3_moves_opt = np.median(v3_ratios) if v3_ratios else 0.0
            v3_path_opt = np.median(v3_path_ratios) if v3_path_ratios else 0.0
            
            if verbose:
                print(f"| {size:<4d} | {density:<7.2f} | "
                      f"{len(v2_solved):2d}/{n_mazes} ({v2_pct*100:3.0f}%) | "
                      f"{v2_moves_opt:.2f}x      | "
                      f"{len(v3_solved):2d}/{n_mazes} ({v3_pct*100:3.0f}%) | "
                      f"{v3_moves_opt:.2f}x      | "
                      f"{v3_path_opt:.2f}x      |")
            
            table.append({
                "size": size,
                "density": density,
                "v2_pct": v2_pct,
                "v2_moves_opt": v2_moves_opt,
                "v3_pct": v3_pct,
                "v3_moves_opt": v3_moves_opt,
                "v3_path_opt": v3_path_opt
            })
    return table


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="ZKAEDI PRIME solver + gauntlet")
    ap.add_argument("--gauntlet", action="store_true")
    ap.add_argument("--sweep", action="store_true")
    ap.add_argument("--n", type=int, default=60)
    ap.add_argument("--size", type=int, default=25)
    ap.add_argument("--seed", type=int, default=7000)
    ap.add_argument("--solver", choices=["v1", "v2", "v3"], default="v3")
    a = ap.parse_args()
    if a.gauntlet:
        r = run_gauntlet(a.n, a.size, a.seed)
        raise SystemExit(0 if r["_gates_pass"] else 1)
    if a.sweep:
        run_sweep(n_mazes=40, seed0=a.seed)
        raise SystemExit(0)
    mz = make_maze(a.size, 0)
    fn = {"v1": solve_zkaedi_prime, "v2": solve_zkaedi_prime_v2,
          "v3": solve_zkaedi_prime_v3}[a.solver]
    sol = fn(mz, seed=a.seed)
    if sol:
        extra = f", simple path {sol.meta['path_len']}" if "path_len" in sol.meta else ""
        print(f"solved: {sol.steps} moves in {sol.time_taken:.3f}s{extra}")
    else:
        print("unsolved")
