#!/usr/bin/env python3
import argparse
import json
import os
import random
import subprocess
import sys
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(description="ZCC Quantum Peephole Optimizer Verification Harness")
    parser.add_argument("--binary", required=True, help="Path to the C optimizer binary")
    parser.add_argument("--expect-fault", action="store_true", help="Expect the equivalence or gate-count check to fail")
    parser.add_argument("--property", action="store_true", help="Run property-based fuzzing")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("-n", type=int, default=10, help="Number of fuzz iterations")
    return parser.parse_args()


FIXED_SUITE = [
    {
        "name": "Basic-4Rule-Suite",
        "num_qubits": 2,
        "ir": [
            "H 0",
            "H 0",          # Cancels
            "RZ 1 1.0",
            "RZ 1 0.5",     # Merges to RZ 1 1.5
            "CNOT 0 1",
            "CNOT 0 1",     # Cancels
            "SWAP 0 1",
            "SWAP 0 1",     # Cancels
        ],
        "expect_gates_out": 1,
    },
    {
        "name": "Fixpoint-Cascade-H-CX-CX-H",
        "num_qubits": 2,
        "ir": [
            "H 0",
            "CNOT 0 1",
            "CNOT 0 1",
            "H 0",
        ],
        "expect_gates_out": 0,
    },
    {
        "name": "Disjoint-Support-Commutation",
        "num_qubits": 2,
        "ir": [
            "H 0",
            "H 1",
            "H 0",
        ],
        "expect_gates_out": 1,
    },
    {
        "name": "Symmetric-SWAP-Cancel",
        "num_qubits": 2,
        "ir": [
            "SWAP 0 1",
            "SWAP 1 0",
            "RZ 0 0.75",
        ],
        "expect_gates_out": 1,
    },
]


def generate_fuzz_ir(num_qubits=3):
    ops = []
    for _ in range(24):
        gate = random.choice(["H", "RZ", "CNOT", "SWAP"])
        if gate == "H":
            ops.append(f"H {random.randint(0, num_qubits - 1)}")
        elif gate == "RZ":
            ops.append(f"RZ {random.randint(0, num_qubits - 1)} {random.uniform(0, 2 * np.pi):.4f}")
        else:
            q0, q1 = random.sample(range(num_qubits), 2)
            ops.append(f"{gate} {q0} {q1}")
    return ops


def run_optimizer(binary_path, ir_lines, expect_rc=0):
    input_data = "\n".join(ir_lines) + "\n"
    result = subprocess.run(
        [binary_path],
        input=input_data.encode("utf-8"),
        capture_output=True,
    )
    if result.returncode != expect_rc:
        raise RuntimeError(
            f"Optimizer exited with {result.returncode} (expected {expect_rc}): "
            f"{result.stderr.decode('utf-8', 'ignore').strip()}"
        )
    out_lines = result.stdout.decode("utf-8").strip().split("\n")
    return [line.strip() for line in out_lines if line.strip()], result.returncode


def get_base_matrices():
    I = np.array([[1, 0], [0, 1]], dtype=complex)
    H = np.array([[1, 1], [1, -1]], dtype=complex) / np.sqrt(2)

    def RZ(theta):
        return np.array(
            [[np.exp(-1j * theta / 2), 0], [0, np.exp(1j * theta / 2)]],
            dtype=complex,
        )

    return I, H, RZ


def build_dense_matrix(ir_lines, num_qubits=2):
    I, H_mat, RZ_mat = get_base_matrices()
    dim = 2 ** num_qubits
    U = np.eye(dim, dtype=complex)

    def apply_single_qubit_gate(gate_matrix, target):
        op = np.array([[1]], dtype=complex)
        for q in range(num_qubits):
            op = np.kron(op, gate_matrix if q == target else I)
        return op

    def apply_cnot(control, target):
        mat = np.eye(dim, dtype=complex)
        for i in range(dim):
            c_bit = (i >> (num_qubits - 1 - control)) & 1
            if c_bit == 1:
                j = i ^ (1 << (num_qubits - 1 - target))
                mat[i, i] = 0
                mat[i, j] = 1
        return mat

    def apply_swap(q0, q1):
        mat = np.eye(dim, dtype=complex)
        for i in range(dim):
            b0 = (i >> (num_qubits - 1 - q0)) & 1
            b1 = (i >> (num_qubits - 1 - q1)) & 1
            if b0 != b1:
                j = i ^ (1 << (num_qubits - 1 - q0)) ^ (1 << (num_qubits - 1 - q1))
                mat[i, i] = 0
                mat[i, j] = 1
        return mat

    for line in ir_lines:
        parts = line.split()
        if not parts:
            continue
        gate = parts[0]
        if gate == "H":
            op = apply_single_qubit_gate(H_mat, int(parts[1]))
        elif gate == "RZ":
            op = apply_single_qubit_gate(RZ_mat(float(parts[2])), int(parts[1]))
        elif gate == "CNOT":
            op = apply_cnot(int(parts[1]), int(parts[2]))
        elif gate == "SWAP":
            op = apply_swap(int(parts[1]), int(parts[2]))
        else:
            raise ValueError(f"Unknown gate in oracle: {gate}")
        U = op @ U

    # Unitarity post-condition check
    unitarity_err = np.linalg.norm(U.conj().T @ U - np.eye(dim), ord="fro")
    if unitarity_err > 1e-10:
        raise RuntimeError(f"Oracle matrix failed unitarity check: err={unitarity_err}")
    return U


def verify_negative_controls(binary_path):
    # Unknown gate must exit 3
    res_unk = subprocess.run([binary_path], input=b"TOFFOLI 0 1 2\n", capture_output=True)
    if res_unk.returncode != 3:
        raise RuntimeError(f"Expected exit 3 on unknown gate, got {res_unk.returncode}")

    # Invalid/OOB qubit or self-CNOT must exit 2
    res_oob = subprocess.run([binary_path], input=b"CNOT 0 0\n", capture_output=True)
    if res_oob.returncode != 2:
        raise RuntimeError(f"Expected exit 2 on invalid CNOT 0 0, got {res_oob.returncode}")


def main():
    args = parse_args()
    random.seed(args.seed)

    if not os.path.exists(args.binary):
        print(f"Binary {args.binary} not found.")
        sys.exit(1)

    if not args.property:
        verify_negative_controls(args.binary)
        for case in FIXED_SUITE:
            raw_ir = case["ir"]
            nq = case["num_qubits"]
            opt_ir, _ = run_optimizer(args.binary, raw_ir)
            U_raw = build_dense_matrix(raw_ir, num_qubits=nq)
            U_opt = build_dense_matrix(opt_ir, num_qubits=nq)
            diff = np.linalg.norm(U_raw - U_opt, ord="fro")
            gate_mismatch = len(opt_ir) != case["expect_gates_out"]

            if diff > 1e-10 or gate_mismatch:
                if args.expect_fault:
                    print(
                        f"Fault detected successfully on {case['name']} "
                        f"(diff={diff:.6f}, gates={len(opt_ir)} vs expected {case['expect_gates_out']})."
                    )
                    sys.exit(0)
                print(
                    f"Verification FAILED on {case['name']}: diff={diff}, "
                    f"gates_out={len(opt_ir)} (expected {case['expect_gates_out']})"
                )
                sys.exit(1)
    else:
        for i in range(args.n):
            raw_ir = generate_fuzz_ir(num_qubits=3)
            opt_ir, _ = run_optimizer(args.binary, raw_ir)
            U_raw = build_dense_matrix(raw_ir, num_qubits=3)
            U_opt = build_dense_matrix(opt_ir, num_qubits=3)
            diff = np.linalg.norm(U_raw - U_opt, ord="fro")
            if diff > 1e-10:
                repro_path = f"/tmp/qopt_repro_seed{args.seed}_iter{i}.json"
                try:
                    with open(repro_path, "w", encoding="utf-8") as f:
                        json.dump({"seed": args.seed, "iteration": i, "raw_ir": raw_ir, "opt_ir": opt_ir, "diff": float(diff)}, f, indent=2)
                except OSError:
                    pass
                if args.expect_fault:
                    print(f"Fault detected successfully in property mode (iter={i}, diff={diff}).")
                    sys.exit(0)
                print(f"Property verification FAILED at iter {i} (diff={diff}, repro={repro_path}).")
                sys.exit(1)

    if args.expect_fault:
        print("Verification PASSED incorrectly: expected fault but none found.")
        sys.exit(1)

    print("Verification PASSED: Optimized circuit is numerically equivalent to raw circuit.")
    sys.exit(0)


if __name__ == "__main__":
    main()
