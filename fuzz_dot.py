#!/usr/bin/env python3
"""
fuzz_dot.py — Differential fuzzer for ZCC dot_product_scalar.

Compares ZCC-compiled output against numpy reference.
Specifically designed to surface CG-IR-005 (phi edge copy drops second phi).

Expected failure signature WITH bug:
    FAIL K=4  got=0.000000  ref=30.000000  err=30.000000  <- CG-IR-005
    All results will be 0.0 — accumulator phi never updated.

Expected output AFTER fix:
    PASS K=4   got=30.000000   ref=30.000000   err=0.000000e+00
    PASS K=8   got=204.000000  ref=204.000000  err=0.000000e+00
    ...

Usage:
    # 1. Build the ZCC-compiled dot product binary first:
    #    (after wiring zcc_loop_builder.c into ZCC and emitting dot_product_scalar)
    #    ./zcc dot_test_driver.c -o dot_test
    #
    # 2. Run fuzzer:
    python3 fuzz_dot.py --binary ./dot_test --trials 1000

    # Or run in reference-only mode (validates the harness itself):
    python3 fuzz_dot.py --ref-only
"""

import argparse
import struct
import subprocess
import sys
import random
import numpy as np


MAX_ERROR = 1e-5
FIXED_SEEDS = [
    # (K, a, b, expected)    — hand-computed, used as regression gates
    (4,  [1, 2, 3, 4],       [1, 2, 3, 4],       30.0),     # 1+4+9+16
    (8,  [1,2,3,4,5,6,7,8],  [1,2,3,4,5,6,7,8],  204.0),    # sum of squares
    (1,  [3.14],             [2.0],              6.28),
    (3,  [0, 0, 0],          [1, 2, 3],          0.0),       # zero vector
    (2,  [1e-7, 1e-7],       [1e-7, 1e-7],       2e-14),     # small values
    (4,  [1e3, 1e3, 1e3, 1e3],[1e3,1e3,1e3,1e3], 4e6),       # large values
]


def ref_dot(a, b):
    """Reference implementation using numpy (f32 precision)."""
    a32 = np.array(a, dtype=np.float32)
    b32 = np.array(b, dtype=np.float32)
    return float(np.dot(a32, b32))


def pack_floats(vals):
    """Pack list of floats to f32 bytes."""
    return struct.pack(f'{len(vals)}f', *vals)


def run_binary_dot(binary, a, b):
    """
    Call the ZCC-compiled dot_product_scalar binary.

    Expected binary interface:
        argv[1] = K (int, as string)
        argv[2] = hex-encoded f32 array a (2*4*K hex chars)
        argv[3] = hex-encoded f32 array b
        stdout  = f32 result as hex string (8 hex chars)

    Alternatively, the binary can read from stdin:
        line 1: K
        line 2: space-separated f32 values of a
        line 3: space-separated f32 values of b
        stdout: f32 result

    Adjust this function to match your actual binary interface.
    """
    K = len(a)
    a_str = ' '.join(f'{v}' for v in a)
    b_str = ' '.join(f'{v}' for v in b)
    inp = f'{K}\n{a_str}\n{b_str}\n'

    try:
        result = subprocess.run(
            [binary],
            input=inp,
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode != 0:
            raise RuntimeError(f'binary exited {result.returncode}: {result.stderr}')
        return float(result.stdout.strip())
    except FileNotFoundError:
        raise RuntimeError(f'binary not found: {binary}')


def run_trial(binary, a, b, label=''):
    """Run one trial. Returns (passed, got, ref, err)."""
    K = len(a)
    expected = ref_dot(a, b)

    if binary:
        got = run_binary_dot(binary, a, b)
    else:
        got = expected  # ref-only mode: always passes

    err = abs(got - expected)
    passed = err <= MAX_ERROR

    status = 'PASS' if passed else 'FAIL'
    tag = f'[{label}]' if label else ''
    print(f'{status} K={K:4d}{tag:20s}  '
          f'got={got:14.6f}  ref={expected:14.6f}  err={err:.3e}')

    # CG-IR-005 signature detection
    if not passed and abs(got) < MAX_ERROR and abs(expected) > MAX_ERROR:
        print(f'  ^^^ CG-IR-005 SIGNATURE: result is 0.0 — float phi not updated')

    return passed, got, expected, err


def main():
    parser = argparse.ArgumentParser(description='Differential fuzzer for dot_product_scalar')
    parser.add_argument('--binary', default=None,
                        help='Path to ZCC-compiled dot_test binary')
    parser.add_argument('--ref-only', action='store_true',
                        help='Run reference implementation only (no binary)')
    parser.add_argument('--trials', type=int, default=100,
                        help='Number of random trials')
    parser.add_argument('--max-k', type=int, default=64,
                        help='Maximum K for random trials')
    parser.add_argument('--seed', type=int, default=42,
                        help='Random seed')
    args = parser.parse_args()

    binary = None if args.ref_only else args.binary
    if not args.ref_only and binary is None:
        print('ERROR: --binary required unless --ref-only', file=sys.stderr)
        sys.exit(1)

    rng = random.Random(args.seed)
    passed = 0
    failed = 0
    cgir005_hits = 0

    print('=== Fixed regression gates ===')
    for K, a, b, expected in FIXED_SEEDS:
        ok, got, ref, err = run_trial(binary, a, b, label='fixed')
        if ok:
            passed += 1
        else:
            failed += 1
            if abs(got) < MAX_ERROR and abs(ref) > MAX_ERROR:
                cgir005_hits += 1

    print(f'\n=== Random trials (n={args.trials}, max_K={args.max_k}) ===')
    for i in range(args.trials):
        K = rng.randint(1, args.max_k)
        a = [rng.uniform(-10.0, 10.0) for _ in range(K)]
        b = [rng.uniform(-10.0, 10.0) for _ in range(K)]
        ok, got, ref, err = run_trial(binary, a, b)
        if ok:
            passed += 1
        else:
            failed += 1
            if abs(got) < MAX_ERROR and abs(ref) > MAX_ERROR:
                cgir005_hits += 1

    total = passed + failed
    print(f'\n=== Summary ===')
    print(f'Passed:  {passed}/{total}')
    print(f'Failed:  {failed}/{total}')
    if cgir005_hits > 0:
        print(f'CG-IR-005 hits: {cgir005_hits}  '
              f'(result==0 when reference!=0 — phi edge copy drops float phi)')
        print(f'\nFix target: ir_asm_emit_phi_edge_copy in compiler_passes.c ~line 4570')
        print(f'  Iterate ALL phis in the target block, not just the first.')
        print(f'  Gate: dot K=4 a=b=[1,2,3,4] must return 30.0')

    sys.exit(0 if failed == 0 else 1)


if __name__ == '__main__':
    main()
