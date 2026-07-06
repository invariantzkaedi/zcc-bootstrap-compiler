import sys
import os
import numpy as np

sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from tableau_ref import cnot, cz, h, s
from test_quantum_stabilizers import I, X, Y, Z, CNOT, CZ, conj, same_pauli_up_to_sign, kron2

# Hadamard matrix
H_unitary = 1.0 / np.sqrt(2) * np.array([[1,1],[1,-1]], dtype=complex)
# Phase S matrix
S_unitary = np.array([[1,0],[0,1j]], dtype=complex)

def test_tableau_known_rules():
    # 1. CNOT X_c propagation
    x = [1,0]; z = [0,0]
    cnot(x, z, 0, 1)
    assert x == [1,1] and z == [0,0]

    # 2. CNOT Z_t propagation
    x = [0,0]; z = [0,1]
    cnot(x, z, 0, 1)
    assert x == [0,0] and z == [1,1]

    # 3. CZ X_a propagation
    x = [1,0]; z = [0,0]
    cz(x, z, 0, 1)
    assert x == [1,0] and z == [0,1]

    # 4. H X_q propagation
    x = [1,0]; z = [0,0]
    h(x, z, 0)
    assert x == [0,0] and z == [1,0]

    # 5. S X_q propagation
    x = [1,0]; z = [0,0]
    s(x, z, 0)
    assert x == [1,0] and z == [1,0]

    print("TABLEAU KNOWN RULES TEST PASSED!")

def test_tableau_vs_unitary_conjugation():
    # Verify Hadamard and Phase gate simulation matches numerical matrix representation
    # Single Qubit checks
    # X stabilizer
    x = [1]; z = [0]
    h(x, z, 0)
    assert same_pauli_up_to_sign(conj(H_unitary, X), Z)
    assert x == [0] and z == [1]

    # S gate on X
    x = [1]; z = [0]
    s(x, z, 0)
    assert same_pauli_up_to_sign(conj(S_unitary, X), Y)  # S X S^H = Y (up to sign/phase)
    assert x == [1] and z == [1]

    print("TABLEAU VS UNITARY CONJUGATION PASSED!")

if __name__ == "__main__":
    test_tableau_known_rules()
    test_tableau_vs_unitary_conjugation()
