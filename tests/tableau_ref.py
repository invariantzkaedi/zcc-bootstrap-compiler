import numpy as np

class Tableau:
    def __init__(self, n):
        self.n = n
        # Represented as 2n rows. Each row is of length 2n (first n bits are X, next n are Z)
        # We represent it as a 2D numpy array of uint8 for fast GF(2) operations
        self.matrix = np.zeros((2 * n, 2 * n), dtype=np.uint8)
        
    @classmethod
    def identity(cls, n):
        t = cls(n)
        for i in range(2 * n):
            t.matrix[i, i] = 1
        return t
        
    def copy(self):
        t = Tableau(self.n)
        t.matrix = np.copy(self.matrix)
        return t
        
    def apply(self, gate, *args):
        if gate == "H":
            self.apply_h(*args)
        elif gate == "S":
            self.apply_s(*args)
        elif gate == "CNOT":
            self.apply_cnot(*args)
        elif gate == "CZ":
            self.apply_cz(*args)
            
    def apply_h(self, q):
        # Swap X and Z for qubit q in all rows
        x_col = self.matrix[:, q].copy()
        z_col = self.matrix[:, q + self.n].copy()
        self.matrix[:, q] = z_col
        self.matrix[:, q + self.n] = x_col
        
    def apply_s(self, q):
        # Z_q <- Z_q ^ X_q
        self.matrix[:, q + self.n] ^= self.matrix[:, q]
        
    def apply_cnot(self, c, t):
        # X_t <- X_t ^ X_c
        # Z_c <- Z_c ^ Z_t
        self.matrix[:, t] ^= self.matrix[:, c]
        self.matrix[:, c + self.n] ^= self.matrix[:, t + self.n]
        
    def apply_cz(self, a, b):
        # Z_b <- Z_b ^ X_a
        # Z_a <- Z_a ^ X_b
        self.matrix[:, b + self.n] ^= self.matrix[:, a]
        self.matrix[:, a + self.n] ^= self.matrix[:, b]
        
    def is_valid_symplectic(self):
        # Check symplectic inner products between all row pairs
        # Inner product [r_i, r_j] = sum_k (x_ik * z_jk ^ z_ik * x_jk)
        n = self.n
        for i in range(2 * n):
            for j in range(2 * n):
                x_i = self.matrix[i, :n]
                z_i = self.matrix[i, n:]
                x_j = self.matrix[j, :n]
                z_j = self.matrix[j, n:]
                
                prod = np.sum(x_i * z_j ^ z_i * x_j) % 2
                
                # Destabilizer/destabilizer and stabilizer/stabilizer commute
                # Destabilizer i and stabilizer j inner product must be delta_ij
                if i < n and j < n:
                    if prod != 0: return False
                elif i >= n and j >= n:
                    if prod != 0: return False
                elif i < n and j >= n:
                    expected = 1 if (j - n == i) else 0
                    if prod != expected: return False
        return True

    def check_canonical_commutation(self):
        return self.is_valid_symplectic()
        
    def stabilizer_rank(self):
        # Compute GF(2) rank of the bottom n rows (the stabilizers)
        n = self.n
        stabilizers = self.matrix[n:, :].copy()
        
        # Gaussian elimination over GF(2)
        rank = 0
        for col in range(2 * n):
            # Find pivot
            pivot = -1
            for row in range(rank, n):
                if stabilizers[row, col] == 1:
                    pivot = row
                    break
            if pivot != -1:
                # Swap rows
                stabilizers[[rank, pivot]] = stabilizers[[pivot, rank]]
                # Eliminate below
                for row in range(rank + 1, n):
                    if stabilizers[row, col] == 1:
                        stabilizers[row] ^= stabilizers[rank]
                rank += 1
        return rank

# Symplectic Bit-Vector functions for single vectors (backward compatibility)
def cnot(x, z, c, t):
    x[t] ^= x[c]
    z[c] ^= z[t]

def cz(x, z, a, b):
    z[b] ^= x[a]
    z[a] ^= x[b]

def h(x, z, q):
    x[q], z[q] = z[q], x[q]

def s(x, z, q):
    z[q] ^= x[q]
