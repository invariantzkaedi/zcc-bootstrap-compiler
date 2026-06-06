#!/usr/bin/env python3
"""
fuzz_phi_gates.py — Differential fuzzer for ZCC PHI emission gates.

Four kernels, four gates, must pass in order.

GATE 1  CG-IR-005  dot_product_scalar    — multiple PHIs per block
GATE 2  CG-IR-006  swap_recurrence       — parallel PHI move correctness
GATE 3  CG-IR-007  guarded_sum           — critical-edge PHI loss
GATE 4  CG-IR-008  float_accumulate_cast — float PHI register-class

Usage:
    python3 fuzz_phi_gates.py --binary ./zcc_phi_test --trials 500

    # Reference-only mode (validates harness, no ZCC binary needed):
    python3 fuzz_phi_gates.py --ref-only

    # Run only specific gates:
    python3 fuzz_phi_gates.py --binary ./zcc_phi_test --gates 1,2

Expected failure signatures:
    CG-IR-005:  dot result == 0.0 when reference != 0.0
    CG-IR-006:  swap result != 3 (usually 2 or 4 for odd N)
    CG-IR-007:  guarded_sum != reference (threshold branch wrong)
    CG-IR-008:  |float_accumulate - 0.0| > 1e-5 (XMM/GPR class confusion)
"""

import argparse
import random
import subprocess
import sys
import struct
import numpy as np


F32_TOL = 1e-5
I64_TOL = 0          # integer results must be exact


# ---------------------------------------------------------------------------
# Reference implementations
# ---------------------------------------------------------------------------

def ref_dot(a, b):
    a32 = np.array(a, dtype=np.float32)
    b32 = np.array(b, dtype=np.float32)
    return float(np.dot(a32, b32))


def ref_swap_recurrence(N):
    """x,y = 1,2; swap N times; return x+y.  Always 3."""
    x, y = 1, 2
    for _ in range(N):
        x, y = y, x
    return x + y


def ref_guarded_sum(arr, threshold):
    """Sum elements > threshold (f32 precision)."""
    result = np.float32(0.0)
    for v in arr:
        vf = np.float32(v)
        if vf > np.float32(threshold):
            result = result + vf
    return float(result)


def ref_float_accumulate_cast(arr):
    """
    acc_int = sum(arr)
    acc_f32 = sum(float(v) for v in arr)
    return acc_f32 - float(acc_int)
    Should be 0.0 for integer inputs.
    """
    acc_i = sum(arr)
    acc_f = np.float32(0.0)
    for v in arr:
        acc_f = acc_f + np.float32(int(v))
    return float(acc_f - np.float32(acc_i))


# ---------------------------------------------------------------------------
# Binary interface
# ---------------------------------------------------------------------------
# The ZCC-compiled test binary reads from stdin:
#
#   GATE1: kernel=dot K=<n> a=<f32...> b=<f32...>
#   GATE2: kernel=swap N=<n>
#   GATE3: kernel=guarded N=<n> arr=<i32...> threshold=<f32>
#   GATE4: kernel=cast N=<n> arr=<i32...>
#
# Output: single number on stdout (f32 or i64)
# ---------------------------------------------------------------------------

def run_binary(binary, line, timeout=5):
    try:
        r = subprocess.run(
            [binary],
            input=line + '\n',
            capture_output=True,
            text=True,
            timeout=timeout
        )
        if r.returncode != 0:
            raise RuntimeError(f'exit {r.returncode}: {r.stderr.strip()[:120]}')
        return r.stdout.strip()
    except FileNotFoundError:
        raise RuntimeError(f'binary not found: {binary}')
    except subprocess.TimeoutExpired:
        raise RuntimeError(f'timeout: {line[:60]}')


# ---------------------------------------------------------------------------
# Gate 1: CG-IR-005  dot_product_scalar
# ---------------------------------------------------------------------------

GATE1_FIXED = [
    # (K, a, b, expected)
    (4,  [1, 2, 3, 4],       [1, 2, 3, 4],       30.0),
    (8,  [1,2,3,4,5,6,7,8],  [1,2,3,4,5,6,7,8],  204.0),
    (1,  [3.0],              [2.0],              6.0),
    (3,  [0, 0, 0],          [1, 2, 3],          0.0),
    (2,  [0.5, 0.5],         [0.5, 0.5],         0.5),
]


def run_gate1(binary, a, b, label=''):
    expected = ref_dot(a, b)
    K = len(a)
    if binary:
        a_str = ' '.join(str(v) for v in a)
        b_str = ' '.join(str(v) for v in b)
        raw = run_binary(binary, f'kernel=dot K={K} a={a_str} b={b_str}')
        got = float(raw)
    else:
        got = expected

    err = abs(got - expected)
    passed = err <= F32_TOL

    tag = f'[{label}]' if label else ''
    status = 'PASS' if passed else 'FAIL'
    print(f'G1 {status} K={K:4d}{tag:16s}  got={got:14.6f}  ref={expected:14.6f}  err={err:.3e}')

    sig = ''
    if not passed and abs(got) < F32_TOL and abs(expected) > F32_TOL:
        sig = 'CG-IR-005: float phi not updated (result==0.0)'
        print(f'    ^^^ {sig}')

    return passed, sig


# ---------------------------------------------------------------------------
# Gate 2: CG-IR-006  swap_recurrence
# ---------------------------------------------------------------------------

GATE2_FIXED = [0, 1, 2, 3, 4, 5, 10, 11, 99, 100]


def run_gate2(binary, N, label=''):
    expected = ref_swap_recurrence(N)   # always 3
    if binary:
        raw = run_binary(binary, f'kernel=swap N={N}')
        got = int(raw)
    else:
        got = expected

    passed = (got == expected)
    tag = f'[{label}]' if label else ''
    status = 'PASS' if passed else 'FAIL'
    print(f'G2 {status} N={N:4d}{tag:16s}  got={got:6d}  ref={expected:6d}', end='')

    sig = ''
    if not passed:
        if got in (2, 4):
            sig = f'CG-IR-006: sequential phi copy clobbered one value (got {got}, expected 3)'
        else:
            sig = f'CG-IR-006: unexpected value {got}'
        print(f'\n    ^^^ {sig}', end='')
    print()

    return passed, sig


# ---------------------------------------------------------------------------
# Gate 3: CG-IR-007  guarded_sum  (critical-edge PHI)
# ---------------------------------------------------------------------------

GATE3_FIXED = [
    # (arr, threshold, expected)
    ([1, 5, 2, 8, 3], 4.0, 13.0),       # 5 + 8
    ([10, 1, 10, 1],  5.0, 20.0),
    ([1, 2, 3],       10.0, 0.0),        # nothing passes threshold
    ([5, 5, 5],       4.0, 15.0),        # all pass
    ([-1, -2, 1, 2],  0.0, 3.0),         # negative values below threshold
]


def run_gate3(binary, arr, threshold, label=''):
    expected = ref_guarded_sum(arr, threshold)
    N = len(arr)
    if binary:
        arr_str = ' '.join(str(v) for v in arr)
        raw = run_binary(binary, f'kernel=guarded N={N} arr={arr_str} threshold={threshold}')
        got = float(raw)
    else:
        got = expected

    err = abs(got - expected)
    passed = err <= F32_TOL
    tag = f'[{label}]' if label else ''
    status = 'PASS' if passed else 'FAIL'
    print(f'G3 {status} N={N:3d} thr={threshold:5.1f}{tag:10s}  '
          f'got={got:10.4f}  ref={expected:10.4f}  err={err:.3e}')

    sig = ''
    if not passed:
        sig = 'CG-IR-007: critical-edge phi loss (if/else merge branch wrong)'
        print(f'    ^^^ {sig}')

    return passed, sig


# ---------------------------------------------------------------------------
# Gate 4: CG-IR-008  float_accumulate_cast  (register-class preservation)
# ---------------------------------------------------------------------------

GATE4_FIXED = [
    [1, 2, 3, 4, 5],
    [10, 20, 30],
    [0, 0, 0, 0],
    [127, 1, 1],
    [-5, -3, 8],
]


def run_gate4(binary, arr, label=''):
    expected = ref_float_accumulate_cast(arr)
    N = len(arr)
    if binary:
        arr_str = ' '.join(str(v) for v in arr)
        raw = run_binary(binary, f'kernel=cast N={N} arr={arr_str}')
        got = float(raw)
    else:
        got = expected

    err = abs(got - expected)
    # For gate 4, reference should be ~0.0 for integer inputs;
    # but compare got vs reference regardless (tolerance on the difference)
    passed = err <= F32_TOL
    tag = f'[{label}]' if label else ''
    status = 'PASS' if passed else 'FAIL'
    print(f'G4 {status} N={N:3d}{tag:16s}  got={got:12.6f}  ref={expected:12.6f}  err={err:.3e}')

    sig = ''
    if not passed:
        sig = 'CG-IR-008: float phi routed through integer move (XMM/GPR class confusion)'
        print(f'    ^^^ {sig}')

    return passed, sig


# ---------------------------------------------------------------------------
# Random trial generators
# ---------------------------------------------------------------------------

def random_gate1(rng, max_k=64):
    K = rng.randint(1, max_k)
    a = [rng.uniform(-10.0, 10.0) for _ in range(K)]
    b = [rng.uniform(-10.0, 10.0) for _ in range(K)]
    return a, b


def random_gate2(rng, max_n=200):
    return rng.randint(0, max_n)


def random_gate3(rng, max_n=32):
    N = rng.randint(1, max_n)
    arr = [rng.randint(-20, 20) for _ in range(N)]
    threshold = rng.uniform(-10.0, 10.0)
    return arr, threshold


def random_gate4(rng, max_n=32):
    N = rng.randint(1, max_n)
    # Integer values only — ref should return 0.0
    return [rng.randint(-100, 100) for _ in range(N)]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', default=None)
    parser.add_argument('--ref-only', action='store_true')
    parser.add_argument('--trials', type=int, default=200)
    parser.add_argument('--gates', default='1,2,3,4',
                        help='Comma-separated list of gates to run (1-4)')
    parser.add_argument('--seed', type=int, default=42)
    args = parser.parse_args()

    binary = None if args.ref_only else args.binary
    if not args.ref_only and binary is None:
        print('ERROR: --binary required unless --ref-only', file=sys.stderr)
        sys.exit(1)

    active_gates = set(int(g) for g in args.gates.split(','))
    rng = random.Random(args.seed)

    results = {1: [], 2: [], 3: [], 4: []}
    sigs    = {1: set(), 2: set(), 3: set(), 4: set()}

    def record(gate, passed, sig):
        results[gate].append(passed)
        if sig:
            sigs[gate].add(sig)

    # --- Gate 1 ---
    if 1 in active_gates:
        print('\n=== GATE 1: CG-IR-005  dot_product_scalar ===')
        for K, a, b, _ in GATE1_FIXED:
            ok, sig = run_gate1(binary, a, b, 'fixed')
            record(1, ok, sig)
        for _ in range(args.trials):
            a, b = random_gate1(rng)
            ok, sig = run_gate1(binary, a, b)
            record(1, ok, sig)

    # --- Gate 2 ---
    if 2 in active_gates:
        print('\n=== GATE 2: CG-IR-006  swap_recurrence ===')
        for N in GATE2_FIXED:
            ok, sig = run_gate2(binary, N, 'fixed')
            record(2, ok, sig)
        for _ in range(args.trials):
            N = random_gate2(rng)
            ok, sig = run_gate2(binary, N)
            record(2, ok, sig)

    # --- Gate 3 ---
    if 3 in active_gates:
        print('\n=== GATE 3: CG-IR-007  guarded_sum ===')
        for arr, thr, _ in GATE3_FIXED:
            ok, sig = run_gate3(binary, arr, thr, 'fixed')
            record(3, ok, sig)
        for _ in range(args.trials):
            arr, thr = random_gate3(rng)
            ok, sig = run_gate3(binary, arr, thr)
            record(3, ok, sig)

    # --- Gate 4 ---
    if 4 in active_gates:
        print('\n=== GATE 4: CG-IR-008  float_accumulate_cast ===')
        for arr in GATE4_FIXED:
            ok, sig = run_gate4(binary, arr, 'fixed')
            record(4, ok, sig)
        for _ in range(args.trials):
            arr = random_gate4(rng)
            ok, sig = run_gate4(binary, arr)
            record(4, ok, sig)

    # --- Summary ---
    print('\n' + '='*60)
    print('GATE LADDER SUMMARY')
    print('='*60)

    all_green = True
    gate_names = {
        1: 'CG-IR-005  multiple PHIs per block',
        2: 'CG-IR-006  parallel PHI move (swap cycle)',
        3: 'CG-IR-007  critical-edge PHI loss',
        4: 'CG-IR-008  float PHI register-class',
    }
    fix_targets = {
        1: 'ir_asm_emit_phi_edge_copy: iterate ALL phis in block',
        2: 'phi parallel move resolver: detect cycles, use scratch reg',
        3: 'critical-edge splitter: insert split block for multi-succ edges',
        4: 'emit_mov in phi copy: dispatch movss vs movq by IR type',
    }

    for gate in sorted(active_gates):
        rs = results[gate]
        if not rs:
            continue
        p = sum(rs)
        t = len(rs)
        green = (p == t)
        if not green:
            all_green = False
        icon = '✓' if green else '✗'
        print(f'  {icon}  Gate {gate}  {gate_names[gate]}')
        print(f'        {p}/{t} passed', end='')
        if not green:
            print(f'  → fix: {fix_targets[gate]}')
            for s in sorted(sigs[gate]):
                print(f'        signature: {s}')
        else:
            print()

    print()
    if all_green:
        print('ALL GATES GREEN — MatMul lowering unblocked')
        sys.exit(0)
    else:
        # Identify first failing gate
        for gate in sorted(active_gates):
            rs = results.get(gate, [])
            if rs and not all(rs):
                print(f'BLOCKED at Gate {gate} — fix CG-IR-{4 + gate:03d} before proceeding')
                break
        sys.exit(1)


if __name__ == '__main__':
    main()
