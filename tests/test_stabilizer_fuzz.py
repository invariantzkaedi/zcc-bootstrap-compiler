import sys
import os
import random
import numpy as np

# Ensure tests/ directory is in path for imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from test_quantum_stabilizers import I, X, Y, Z, CNOT, CZ, conj, same_pauli_up_to_sign, kron2

def rand_pauli_2q(rng):
    one = [I, X, Y, Z]
    return kron2(rng.choice(one), rng.choice(one))

def rand_gate_seq(rng, length=20):
    gates = []
    for _ in range(length):
        gates.append(rng.choice(["CNOT", "CZ"]))
    return gates

def unitary_for_seq(seq):
    U = np.eye(4, dtype=complex)
    for g in seq:
        U = (CNOT if g == "CNOT" else CZ) @ U
    return U

def analytic_step(P, g):
    return conj(CNOT if g == "CNOT" else CZ, P)

def analytic_for_seq(P, seq):
    out = P
    for g in seq:
        out = analytic_step(out, g)
    return out

def test_fuzz_two_qubit():
    num_seeds = int(os.environ.get("QEC_FUZZ_SEEDS", "100"))
    for seed in range(num_seeds):
        rng = random.Random(seed)
        P = rand_pauli_2q(rng)
        seq = rand_gate_seq(rng, length=15)

        U = unitary_for_seq(seq)
        gt = conj(U, P)
        an = analytic_for_seq(P, seq)

        assert same_pauli_up_to_sign(gt, an), f"mismatch seed={seed}, seq={seq}"

if __name__ == "__main__":
    test_fuzz_two_qubit()
    print("STABILIZER FUZZING TESTS PASSED SUCCESSFULLY!")
