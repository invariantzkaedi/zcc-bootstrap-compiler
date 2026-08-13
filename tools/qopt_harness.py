import argparse
import subprocess
import sys
import numpy as np
import random
import os

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, help="Path to the C optimizer binary")
    parser.add_argument("--expect-fault", action="store_true", help="Expect the equivalence check to fail")
    parser.add_argument("--property", action="store_true", help="Run property-based fuzzing")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("-n", type=int, default=10, help="Number of fuzz iterations")
    return parser.parse_args()

def generate_circuit_ir(circuit_type="basic", num_qubits=2):
    if circuit_type == "basic":
        return [
            "H 0",
            "H 0",          # Should cancel
            "RZ 1 1.0",
            "RZ 1 0.5",     # Should merge to RZ 1 1.5
            "CNOT 0 1",
            "CNOT 0 1",     # Should cancel
            "SWAP 0 1",
            "SWAP 0 1"      # Should cancel
        ]
    elif circuit_type == "fuzz":
        ops = []
        for _ in range(20):
            gate = random.choice(["H", "RZ", "CNOT", "SWAP"])
            if gate == "H":
                ops.append(f"H {random.randint(0, num_qubits-1)}")
            elif gate == "RZ":
                ops.append(f"RZ {random.randint(0, num_qubits-1)} {random.uniform(0, 2*np.pi):.4f}")
            elif gate in ["CNOT", "SWAP"]:
                q0, q1 = random.sample(range(num_qubits), 2)
                ops.append(f"{gate} {q0} {q1}")
        return ops

def run_optimizer(binary_path, ir_lines):
    if not os.path.exists(binary_path) and not binary_path.startswith("./"):
        pass
        
    try:
        input_data = "\n".join(ir_lines) + "\n"
        result = subprocess.run([binary_path], input=input_data.encode('utf-8'), capture_output=True, check=True)
        out_lines = result.stdout.decode('utf-8').strip().split('\n')
        return [line.strip() for line in out_lines if line.strip()]
    except Exception as e:
        print(f"Error running optimizer: {e}")
        sys.exit(1)

def get_base_matrices():
    I = np.array([[1, 0], [0, 1]], dtype=complex)
    H = np.array([[1, 1], [1, -1]], dtype=complex) / np.sqrt(2)
    def RZ(theta):
        return np.array([[np.exp(-1j * theta / 2), 0],
                         [0, np.exp(1j * theta / 2)]], dtype=complex)
    return I, H, RZ

def build_dense_matrix(ir_lines, num_qubits=2):
    I, H_mat, RZ_mat = get_base_matrices()
    
    # Initialize state operator as Identity
    U = np.eye(2**num_qubits, dtype=complex)
    
    def apply_single_qubit_gate(gate_matrix, target):
        op = np.array([[1]], dtype=complex)
        for q in range(num_qubits):
            if q == target:
                op = np.kron(op, gate_matrix)
            else:
                op = np.kron(op, I)
        return op
        
    def apply_cnot(control, target):
        mat = np.eye(2**num_qubits, dtype=complex)
        for i in range(2**num_qubits):
            # Check if control bit is 1
            # In our convention, qubit 0 is the most significant bit for kron, or least?
            # Let's use qubit 0 as least significant bit for simplicity, but let's just do it directly.
            # Qubit indexing: q = 0 is left-most in tensor product.
            c_bit = (i >> (num_qubits - 1 - control)) & 1
            if c_bit == 1:
                # toggle target bit
                j = i ^ (1 << (num_qubits - 1 - target))
                mat[i, i] = 0
                mat[i, j] = 1
        return mat
        
    def apply_swap(q0, q1):
        mat = np.eye(2**num_qubits, dtype=complex)
        for i in range(2**num_qubits):
            b0 = (i >> (num_qubits - 1 - q0)) & 1
            b1 = (i >> (num_qubits - 1 - q1)) & 1
            if b0 != b1:
                j = i ^ (1 << (num_qubits - 1 - q0))
                j = j ^ (1 << (num_qubits - 1 - q1))
                mat[i, i] = 0
                mat[i, j] = 1
        return mat

    for line in ir_lines:
        parts = line.split()
        if not parts: continue
        gate = parts[0]
        
        if gate == "H":
            op = apply_single_qubit_gate(H_mat, int(parts[1]))
            U = op @ U
        elif gate == "RZ":
            op = apply_single_qubit_gate(RZ_mat(float(parts[2])), int(parts[1]))
            U = op @ U
        elif gate == "CNOT":
            op = apply_cnot(int(parts[1]), int(parts[2]))
            U = op @ U
        elif gate == "SWAP":
            op = apply_swap(int(parts[1]), int(parts[2]))
            U = op @ U
            
    return U

def main():
    args = parse_args()
    random.seed(args.seed)
    
    if not os.path.exists(args.binary):
        print(f"Binary {args.binary} not found.")
        sys.exit(1)
        
    iterations = args.n if args.property else 1
    
    for i in range(iterations):
        ir_type = "fuzz" if args.property else "basic"
        raw_ir = generate_circuit_ir(ir_type, num_qubits=2)
        opt_ir = run_optimizer(args.binary, raw_ir)
        
        U_raw = build_dense_matrix(raw_ir, num_qubits=2)
        U_opt = build_dense_matrix(opt_ir, num_qubits=2)
        
        diff = np.linalg.norm(U_raw - U_opt, ord='fro')
        
        if diff > 1e-10:
            if args.expect_fault:
                print(f"Fault detected successfully (diff={diff}).")
                sys.exit(0)
            else:
                print(f"Verification FAILED. Frobenius diff: {diff}")
                sys.exit(1)
    
    if args.expect_fault:
        print("Verification PASSED incorrectly: expected fault but none found.")
        sys.exit(1)
        
    print("Verification PASSED: Optimized circuit is numerically equivalent to raw circuit.")
    sys.exit(0)

if __name__ == "__main__":
    main()
