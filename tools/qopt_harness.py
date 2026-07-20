import argparse
import subprocess
import sys
import numpy as np
import random
import os

try:
    from qiskit import QuantumCircuit
    from qiskit.quantum_info import Operator
    HAS_QISKIT = True
except ImportError:
    HAS_QISKIT = False
    print("Warning: qiskit not found. Please install it using 'pip install qiskit'.")

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
        pass # allow PATH resolution, but usually it's a local file
        
    try:
        input_data = "\n".join(ir_lines) + "\n"
        result = subprocess.run([binary_path], input=input_data.encode('utf-8'), capture_output=True, check=True)
        out_lines = result.stdout.decode('utf-8').strip().split('\n')
        return [line.strip() for line in out_lines if line.strip()]
    except Exception as e:
        print(f"Error running optimizer: {e}")
        sys.exit(1)

def build_qiskit_circuit(ir_lines, num_qubits=3):
    qc = QuantumCircuit(num_qubits)
    for line in ir_lines:
        parts = line.split()
        if not parts: continue
        gate = parts[0]
        if gate == "H":
            qc.h(int(parts[1]))
        elif gate == "RZ":
            qc.rz(float(parts[2]), int(parts[1]))
        elif gate == "CNOT":
            qc.cx(int(parts[1]), int(parts[2]))
        elif gate == "SWAP":
            qc.swap(int(parts[1]), int(parts[2]))
    return qc

def main():
    args = parse_args()
    random.seed(args.seed)
    
    if not os.path.exists(args.binary):
        print(f"Binary {args.binary} not found.")
        sys.exit(1)

    if not HAS_QISKIT:
        print("Cannot run verification without qiskit.")
        sys.exit(1)
        
    iterations = args.n if args.property else 1
    
    for i in range(iterations):
        ir_type = "fuzz" if args.property else "basic"
        raw_ir = generate_circuit_ir(ir_type)
        opt_ir = run_optimizer(args.binary, raw_ir)
        
        qc_raw = build_qiskit_circuit(raw_ir)
        qc_opt = build_qiskit_circuit(opt_ir)
        
        op_raw = Operator(qc_raw).data
        op_opt = Operator(qc_opt).data
        
        diff = np.linalg.norm(op_raw - op_opt, ord='fro')
        
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
