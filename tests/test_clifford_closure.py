import sys
import os
import random
import pytest

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from tableau_ref import Tableau

def random_clifford_ops(rng, n, depth):
    ops = []
    for _ in range(depth):
        g = rng.choice(["H", "S", "CNOT", "CZ"])
        if g in ("H", "S"):
            ops.append((g, rng.randrange(n)))
        else:
            # For 2-qubit gates, ensure control != target
            a = rng.randrange(n)
            b = rng.randrange(n)
            while b == a:
                b = rng.randrange(n)
            ops.append((g, a, b))
    return ops

def test_clifford_rank_preserved():
    n = 7
    num_seeds = int(os.environ.get("QEC_FUZZ_SEEDS", "50"))
    for seed in range(num_seeds):
        rng = random.Random(seed)
        t = Tableau.identity(n)
        ops = random_clifford_ops(rng, n, depth=100)
        for op in ops:
            t.apply(*op)
        assert t.is_valid_symplectic(), f"Symplectic relation violated for seed={seed}"
        assert t.stabilizer_rank() == n, f"Stabilizer rank collapsed for seed={seed}"

if __name__ == "__main__":
    print("Running Clifford Group closure tests...")
    test_clifford_rank_preserved()
    print("CLIFFORD CLOSURE TESTS PASSED SUCCESSFULLY!")
