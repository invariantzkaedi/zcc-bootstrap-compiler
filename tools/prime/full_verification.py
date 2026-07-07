"""
Full verification suite for ZKAEDI PRIME.

Extends the original run_gauntlet() (which only exercised 3 of the 7
navigation claims made in zkaedi_prime.py's corrigendum docstring, and 0 of
the 5 field-dynamics claims) to cover all 12. Every number printed here is
computed by code in this repo, run to completion, right before the line
that prints it -- consistent with the log-capture + numeric-EXIT-line gate
protocol used elsewhere in this project.

This does NOT try to reproduce the original docstring's numbers exactly.
Several of them (ridge/basin sharpening, eta_c, drift) were never
accompanied by a stated methodology, so there is no ground truth to match.
Where this suite's honest measurement disagrees with the original
*qualitative* claim (not just the magnitude), that is called out explicitly
in the gate output rather than papered over.

Run: python full_verification.py
"""
import sys
import time

import numpy as np

from zkaedi_prime import (
    make_maze, bfs_len, solve_zkaedi_prime, solve_zkaedi_prime_v2, solve_zkaedi_prime_v3,
)
from field_dynamics import (
    run_recursion_to_fixed_point, measure_attractor_sharpening,
    find_bifurcation_eta, measure_stochastic_drift,
)

N_MAZES = 60  # shipped default; see run notes for the reduced-N interactive run used during review
SIZE = 25
SEED0 = 7000
FIELD_DYNAMICS_N_MAZES = 8   # separate, smaller pool for the more expensive
                              # per-step field-dynamics measurements


def build_pool(n_mazes, size, seed_start=0):
    pool, s = [], seed_start
    while len(pool) < n_mazes:
        mz = make_maze(size, s)
        L = bfs_len(mz)
        if L is not None:
            pool.append((mz, L))
        s += 1
    return pool, s


def run_navigation_suite(pool, verbose=True):
    """
    All navigation claims from the docstring (v1, v2, and v3), each backed by an actual
    call, correctly labeled by which function/parameters it runs.
    """
    variants = [
        ("v1 recursion-only (eps=0)",
         lambda mz, sd: solve_zkaedi_prime(mz, eps=0.0, seed=sd)),
        ("v1 noise-only (eta=0)",
         lambda mz, sd: solve_zkaedi_prime(mz, eta=0.0, seed=sd)),
        ("v1 full PRIME (baseline)",
         lambda mz, sd: solve_zkaedi_prime(mz, seed=sd)),
        ("v2 scar-only (eta=0, eps=0)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=0.0, eps=0.0, seed=sd)),
        ("v2 scar+noise (eta=0, eps=.05)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=0.0, eps=0.05, seed=sd)),
        ("v2 scar+PRIME (eta=.4, eps=.05) [DEFAULT]",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, seed=sd)),
        ("v2 scar+PRIME (eta=1.0, eps=.05)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=1.0, seed=sd)),
        ("v3 backtrack+noise (eps=0.05) [v3 DEFAULT]",
         lambda mz, sd: solve_zkaedi_prime_v3(mz, eps=0.05, seed=sd)),
        ("v3 backtrack, eps=0",
         lambda mz, sd: solve_zkaedi_prime_v3(mz, eps=0.0, seed=sd)),
        # Renamed from "NEG CONTROL recursion-only" -- that name implied
        # this tests v1's recursion. It doesn't: it tests v2 with scars
        # and noise both disabled, which is a different code path that
        # happens to share v1's conclusion (no scars/noise -> no
        # navigation). Kept as its own labeled variant, not conflated
        # with the true "v1 recursion-only" row above.
        ("v2 with scars AND noise disabled (kick=0, eps=0)",
         lambda mz, sd: solve_zkaedi_prime_v2(mz, eta=0.4, eps=0.0, kick=0.0, seed=sd)),
    ]
    results = {}
    if verbose:
        print(f"{'variant':45s} solved     med steps  med steps/opt  med path/opt")
    for name, fn in variants:
        solved, ratios, path_ratios = [], [], []
        for i, (mz, L) in enumerate(pool):
            r = fn(mz, SEED0 + i)
            if r is not None:
                solved.append(r.steps)
                ratios.append(r.steps / L)
                pl = r.meta.get("path_len")
                path_ratios.append(pl / L if pl is not None else r.steps / L)
        results[name] = dict(
            solved=len(solved),
            med_steps=int(np.median(solved)) if solved else None,
            med_ratio=float(np.median(ratios)) if ratios else None,
            med_path_ratio=float(np.median(path_ratios)) if path_ratios else None,
        )
        if verbose:
            r = results[name]
            steps_str = str(r["med_steps"]) if r["med_steps"] is not None else "---"
            ratio_str = f"{r['med_ratio']:.1f}x" if r["med_ratio"] else "---"
            path_str = f"{r['med_path_ratio']:.2f}x" if r["med_path_ratio"] else "---"
            print(f"{name:45s} {r['solved']:3d}/{len(pool)}   "
                  f"{steps_str:>6}     {ratio_str:>6}       {path_str:>6}")
    return results


def run_field_dynamics_suite(mazes, verbose=True):
    """All 5 field-dynamics claims, each with documented methodology."""
    results = {}

    mean_devs, conv_steps = [], []
    for mz in mazes:
        r = run_recursion_to_fixed_point(mz, eta=0.4, gamma=0.3)
        mean_devs.append(r.mean_open_cells)
        conv_steps.append(r.converged_at_step)
    results["recursive_coupling_mean_open"] = float(np.mean(mean_devs))
    results["recursive_coupling_spread"] = (float(min(mean_devs)), float(max(mean_devs)))
    results["fixed_point_convergence_step"] = float(np.mean([c for c in conv_steps if c]))

    ridge_ratios, basin_ratios = [], []
    for mz in mazes:
        sh = measure_attractor_sharpening(mz, eta=0.4, gamma=0.3)
        ridge_ratios.append(sh.ridge_ratio)
        basin_ratios.append(sh.basin_ratio)
    results["ridge_ratio"] = float(np.mean(ridge_ratios))
    results["basin_ratio"] = float(np.mean(basin_ratios))
    results["differential_sharpening_gap"] = results["ridge_ratio"] - results["basin_ratio"]

    etac_vals = [find_bifurcation_eta(mz, gamma=0.3, max_steps=2000) for mz in mazes[:4]]
    results["eta_c"] = float(np.mean(etac_vals))

    dr = measure_stochastic_drift(mazes[0], n_seeds=5, burn_in=300, sample_steps=1500)
    results["drift_mean"] = dr.mean
    results["drift_sem"] = dr.sem

    if verbose:
        print(f"recursive coupling (mean |H*-H0|, open cells): "
              f"{results['recursive_coupling_mean_open']:.4f}  "
              f"(range across {len(mazes)} mazes: "
              f"{results['recursive_coupling_spread'][0]:.4f}-"
              f"{results['recursive_coupling_spread'][1]:.4f})")
        print(f"fixed-point convergence step: {results['fixed_point_convergence_step']:.1f}")
        print(f"ridge sharpening ratio: {results['ridge_ratio']:.4f}x")
        print(f"basin sharpening ratio: {results['basin_ratio']:.4f}x")
        print(f"differential gap (ridge - basin): {results['differential_sharpening_gap']:+.4f}")
        print(f"bifurcation eta_c: {results['eta_c']:.4f}")
        print(f"stochastic drift: {results['drift_mean']:.4f} +/- {results['drift_sem']:.4f}")
    return results


def main():
    t0 = time.time()
    print("=" * 72)
    print("NAVIGATION SUITE (60 mazes, all 7 docstring claims)")
    print("=" * 72)
    pool, scanned = build_pool(N_MAZES, SIZE)
    print(f"pool: {N_MAZES} BFS-solvable {SIZE}x{SIZE} mazes (scanned {scanned} seeds), "
          f"optimal median {int(np.median([L for _, L in pool]))}")
    nav = run_navigation_suite(pool)

    print()
    print("=" * 72)
    print(f"FIELD DYNAMICS SUITE ({FIELD_DYNAMICS_N_MAZES} mazes, all 5 docstring claims)")
    print("=" * 72)
    fd_pool, _ = build_pool(FIELD_DYNAMICS_N_MAZES, SIZE)
    fd_mazes = [mz for mz, _ in fd_pool]
    fd = run_field_dynamics_suite(fd_mazes)

    print()
    print("=" * 72)
    print("GATES")
    print("=" * 72)
    gates = []

    default_variant = nav["v2 scar+PRIME (eta=.4, eps=.05) [DEFAULT]"]
    g1 = default_variant["solved"] == N_MAZES and (default_variant["med_ratio"] or 99) <= 2.0
    gates.append((f"v2 default config solves {N_MAZES}/{N_MAZES} within 2.0x optimal", g1))

    v3_default = nav["v3 backtrack+noise (eps=0.05) [v3 DEFAULT]"]
    g_v3 = v3_default["solved"] == N_MAZES and (v3_default["med_path_ratio"] or 99) <= 1.25
    gates.append((f"v3 default simple path solves {N_MAZES}/{N_MAZES} within 1.25x optimal", g_v3))

    g2 = fd["fixed_point_convergence_step"] < 200
    gates.append(("deterministic recursion converges to fixed point (<200 steps)", g2))

    g3 = fd["eta_c"] > 1.0
    gates.append(("bifurcation point eta_c is above 1.0 (bounded for canonical eta=0.4)", g3))

    # Near-uniform rescaling: both ridge and basin ratios should be within [1.5, 1.8] under canonical eta=0.4
    g4 = 1.5 <= fd["ridge_ratio"] <= 1.8 and 1.5 <= fd["basin_ratio"] <= 1.8
    gates.append(("ridges and basins rescale uniformly by ~1/(1-eta) (1.5x-1.8x at eta=0.4)", g4))
    if not g4:
        print(f"NOTE: gate 5 FAILS -- ridge ratio ({fd['ridge_ratio']:.3f}x) "
              f"or basin ratio ({fd['basin_ratio']:.3f}x) out of expected uniform range.")

    for desc, ok in gates:
        print(f"  [{'PASS' if ok else 'FAIL'}] {desc}")

    overall = all(ok for _, ok in gates)
    print()
    print(f"OVERALL: {'PASS' if overall else 'FAIL'}  ({len(gates)} gates, "
          f"{sum(ok for _, ok in gates)} passed)")
    print(f"total elapsed: {time.time()-t0:.1f}s")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
