import numpy as np

I = np.eye(2, dtype=complex)
X = np.array([[0,1],[1,0]], dtype=complex)
Y = np.array([[0,-1j],[1j,0]], dtype=complex)
Z = np.array([[1,0],[0,-1]], dtype=complex)

def kron2(a, b): return np.kron(a, b)

CNOT = np.array([
    [1,0,0,0],
    [0,1,0,0],
    [0,0,0,1],
    [0,0,1,0],
], dtype=complex)

CZ = np.diag([1,1,1,-1]).astype(complex)

def conj(U, P):
    return U @ P @ U.conj().T

def same_pauli_up_to_sign(A, B, atol=1e-9):
    return np.allclose(A, B, atol=atol) or np.allclose(A, -B, atol=atol)

def test_cnot_rules():
    Xc = kron2(X, I)
    Xt = kron2(I, X)
    Zc = kron2(Z, I)
    Zt = kron2(I, Z)

    assert same_pauli_up_to_sign(conj(CNOT, Xc), Xc @ Xt)  # Xc -> XcXt
    assert same_pauli_up_to_sign(conj(CNOT, Zt), Zc @ Zt)  # Zt -> ZcZt

def test_cz_rules():
    Xa = kron2(X, I)
    Xb = kron2(I, X)
    Za = kron2(Z, I)
    Zb = kron2(I, Z)

    assert same_pauli_up_to_sign(conj(CZ, Za), Za)         # Z unchanged
    assert same_pauli_up_to_sign(conj(CZ, Zb), Zb)
    assert same_pauli_up_to_sign(conj(CZ, Xa), Xa @ Zb)    # Xa -> XaZb
    assert same_pauli_up_to_sign(conj(CZ, Xb), Za @ Xb)    # Xb -> ZaXb

if __name__ == "__main__":
    test_cnot_rules()
    test_cz_rules()
    print("PARAMETRIC STABILIZER ORACLE VERIFIED SUCCESSFULLY!")
