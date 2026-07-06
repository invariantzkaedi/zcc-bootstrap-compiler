import os
import random
import pytest

from tests.tableau_ref import Tableau

SEEDS = int(os.environ.get("QEC_FUZZ_SEEDS", "100"))

def random_ops(rng, n, depth):
    ops = []
    for _ in range(depth):
        g = rng.choice(["H", "S", "CNOT", "CZ"])
        if g in ("H", "S"):
            ops.append((g, rng.randrange(n)))
        else:
            a = rng.randrange(n)
            b = rng.randrange(n)
            while b == a:
                b = rng.randrange(n)
            ops.append((g, a, b))
    return ops

@pytest.mark.parametrize("n", [2, 3, 5, 7])
def test_symplectic_validity_and_rank(n):
    for seed in range(SEEDS):
        rng = random.Random(seed)
        t = Tableau.identity(n)
        for op in random_ops(rng, n, depth=50):
            t.apply(*op)
        assert t.is_valid_symplectic()
        assert t.stabilizer_rank() == n

@pytest.mark.parametrize("n", [2, 4, 7])
def test_commutation_structure_preserved(n):
    for seed in range(SEEDS):
        rng = random.Random(seed)
        t = Tableau.identity(n)
        for op in random_ops(rng, n, depth=30):
            t.apply(*op)
        assert t.check_canonical_commutation()
