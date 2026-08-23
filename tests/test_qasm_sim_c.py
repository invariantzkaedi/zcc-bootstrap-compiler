#!/usr/bin/env python3
"""
ZCC OpenQASM 2.0 Standalone C Code Generator Differential Test Suite (Phase 0D)
Tests standalone C code generation, GCC/Clang compilation, and exact statevector /
measurement parity against the Phase 0B in-memory simulator.
"""

import math
import os
import random
import shutil
import subprocess
import tempfile
import unittest

ZCC_BIN = os.path.abspath("./zcc")
GCC_BIN = shutil.which("gcc") or "/usr/bin/gcc"
CLANG_BIN = shutil.which("clang")


def run_zcc_sim(qasm_source, seed=0x12345678):
    with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
        f.write(qasm_source)
        f_name = f.name
    try:
        cmd = [ZCC_BIN, "--target=qasm-sim", f"--seed={seed}", f_name]
        res = subprocess.run(cmd, capture_output=True, text=True)
        return res.returncode, res.stdout, res.stderr
    finally:
        if os.path.exists(f_name):
            os.remove(f_name)


def run_zcc_emit_c(qasm_source):
    with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
        f.write(qasm_source)
        f_name = f.name
    try:
        cmd = [ZCC_BIN, "--target=qasm-sim-c", f_name]
        res = subprocess.run(cmd, capture_output=True, text=True)
        return res.returncode, res.stdout, res.stderr
    finally:
        if os.path.exists(f_name):
            os.remove(f_name)


def compile_and_run_generated_c(c_source, seed=0x12345678, compiler=GCC_BIN, extra_args=None):
    with tempfile.NamedTemporaryFile(suffix=".c", mode="w", delete=False) as f:
        f.write(c_source)
        c_path = f.name
    bin_path = c_path + ".bin"
    try:
        compile_cmd = [compiler, "-O3", "-w", c_path, "-o", bin_path, "-lm"]
        c_res = subprocess.run(compile_cmd, capture_output=True, text=True)
        if c_res.returncode != 0:
            return c_res.returncode, "", f"Compilation error ({compiler}): {c_res.stderr}"

        exec_cmd = [bin_path, f"--seed={seed}"]
        if extra_args:
            exec_cmd.extend(extra_args)
        e_res = subprocess.run(exec_cmd, capture_output=True, text=True)
        return e_res.returncode, e_res.stdout, e_res.stderr
    finally:
        if os.path.exists(c_path):
            os.remove(c_path)
        if os.path.exists(bin_path):
            os.remove(bin_path)


def parse_statevector_dump(dump_text):
    amplitudes = {}
    for line in dump_text.strip().splitlines():
        line = line.strip()
        if not line or not line.startswith("|"):
            continue
        parts = line.split(":", 1)
        basis = parts[0].strip()
        comp_str = parts[1].split("(")[0].strip()
        c_parts = comp_str.split()
        re = float(c_parts[0])
        im_str = c_parts[1].replace("i", "")
        im = float(im_str)
        amplitudes[basis] = complex(re, im)
    return amplitudes


def assert_statevectors_match(test_case, sim_sv, c_sv, tol=1e-9):
    all_keys = set(sim_sv.keys()).union(set(c_sv.keys()))
    for k in sorted(all_keys):
        a = sim_sv.get(k, 0.0 + 0.0j)
        b = c_sv.get(k, 0.0 + 0.0j)
        test_case.assertAlmostEqual(
            a.real, b.real, delta=tol,
            msg=f"Real amplitude divergence at basis {k}: sim={a} vs emitted_c={b}"
        )
        test_case.assertAlmostEqual(
            a.imag, b.imag, delta=tol,
            msg=f"Imag amplitude divergence at basis {k}: sim={a} vs emitted_c={b}"
        )


class TestZCCQasmSimCEmitter(unittest.TestCase):

    def _verify_circuit_differential(self, qasm_source, seed=0x12345678, tol=1e-9):
        # 1. Run Phase 0B simulator
        sim_rc, sim_out, sim_err = run_zcc_sim(qasm_source, seed=seed)
        self.assertEqual(sim_rc, 0, f"Phase 0B simulator failed: {sim_err}")

        # 2. Emit standalone C code
        emit_rc, c_source, emit_err = run_zcc_emit_c(qasm_source)
        self.assertEqual(emit_rc, 0, f"C code emission failed: {emit_err}")
        self.assertIn("void run_quantum_circuit(ZCCQasmState *state)", c_source)

        # 3. Compile and execute generated C
        c_rc, c_out, c_err = compile_and_run_generated_c(c_source, seed=seed)
        self.assertEqual(c_rc, 0, f"Generated C execution failed: {c_err}")

        # 4. Compare statevectors
        sim_sv = parse_statevector_dump(sim_out)
        c_sv = parse_statevector_dump(c_out)
        assert_statevectors_match(self, sim_sv, c_sv, tol=tol)
        return sim_sv, c_sv

    def test_01_hadamard_single_qubit(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
h q[0];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|0>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(c_sv["|1>"]), 1.0 / math.sqrt(2), places=6)

    def test_02_pauli_gates(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
x q[0];
y q[1];
z q[2];
"""
        self._verify_circuit_differential(qasm)

    def test_03_phase_gates(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
s q[0];
t q[0];
h q[1];
sdg q[1];
tdg q[1];
"""
        self._verify_circuit_differential(qasm)

    def test_04_rotations_rx_ry_rz(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
rx(pi/3) q[0];
ry(pi/4) q[1];
rz(pi/6) q[2];
"""
        self._verify_circuit_differential(qasm)

    def test_05_u_gates(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
u1(0.5) q[0];
u2(0.3, 0.7) q[1];
u3(0.2, 0.4, 0.6) q[2];
"""
        self._verify_circuit_differential(qasm)

    def test_06_cnot_bell_state(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
cx q[0], q[1];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|00>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(c_sv["|11>"]), 1.0 / math.sqrt(2), places=6)

    def test_07_cz_and_swap(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
x q[1];
cz q[0], q[1];
swap q[0], q[1];
"""
        self._verify_circuit_differential(qasm)

    def test_08_iswap_and_rzz(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
h q[1];
iswap q[0], q[1];
rzz(pi/4) q[0], q[1];
"""
        self._verify_circuit_differential(qasm)

    def test_09_toffoli_ccx(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
x q[0];
x q[1];
ccx q[0], q[1], q[2];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|111>"]), 1.0, places=6)

    def test_10_cswap_fredkin(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
x q[0];
x q[1];
cswap q[0], q[1], q[2];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|101>"]), 1.0, places=6)

    def test_11_ghz_3qubit(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[0];
cx q[0], q[1];
cx q[1], q[2];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|000>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(c_sv["|111>"]), 1.0 / math.sqrt(2), places=6)

    def test_12_w_state_3qubit(self):
        # 3-qubit W-state preparation
        theta1 = 2.0 * math.acos(1.0 / math.sqrt(3.0))
        theta2 = 2.0 * math.acos(1.0 / math.sqrt(2.0))
        qasm = f"""OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
ry({theta1:.8f}) q[0];
x q[1];
cx q[0], q[1];
ch q[1], q[2];
cx q[2], q[0];
cx q[0], q[1];
"""
        self._verify_circuit_differential(qasm)

    def test_13_quantum_fourier_transform_3q(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
x q[0];
h q[0];
cu1(pi/2) q[1], q[0];
cu1(pi/4) q[2], q[0];
h q[1];
cu1(pi/2) q[2], q[1];
h q[2];
swap q[0], q[2];
"""
        self._verify_circuit_differential(qasm)

    def test_14_projective_measurement_determinism(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];
h q[0];
cx q[0], q[1];
measure q[0] -> c[0];
measure q[1] -> c[1];
"""
        # Test 5 deterministic seeds
        for seed in [111, 222, 333, 444, 555]:
            self._verify_circuit_differential(qasm, seed=seed)

    def test_15_reset_operator(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
x q[0];
reset q[0];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|0>"]), 1.0, places=6)

    def test_16_classical_feedback_loop(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[1];
x q[0];
measure q[0] -> c[0];
if (c == 1) x q[1];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|11>"]), 1.0, places=6)

    def test_17_multi_register_layout(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg a[2];
qreg b[1];
creg m[2];
h a[0];
cx a[0], b[0];
h a[1];
"""
        self._verify_circuit_differential(qasm)

    def test_18_full_optimization_and_emission_pipeline(self):
        # Unoptimized circuit with redundant gates
        qasm_unopt = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
h q[0];
rx(pi/4) q[1];
rx(pi/4) q[1];
cx q[0], q[1];
cx q[0], q[1];
"""
        # 1. Run optimizer via CLI
        with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
            f.write(qasm_unopt)
            unopt_path = f.name
        try:
            opt_res = subprocess.run([ZCC_BIN, "--target=qasm-opt", unopt_path], capture_output=True, text=True)
            self.assertEqual(opt_res.returncode, 0)
            opt_qasm = opt_res.stdout

            # Verify optimized circuit matches standalone C compilation
            self._verify_circuit_differential(opt_qasm)
        finally:
            if os.path.exists(unopt_path):
                os.remove(unopt_path)

    @unittest.skipIf(CLANG_BIN is None, "Clang not installed on host")
    def test_19_clang_compilation_parity(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
cx q[0], q[1];
"""
        _, c_source, _ = run_zcc_emit_c(qasm)
        c_rc, c_out, c_err = compile_and_run_generated_c(c_source, compiler=CLANG_BIN)
        self.assertEqual(c_rc, 0, f"Clang compilation failed: {c_err}")
        c_sv = parse_statevector_dump(c_out)
        self.assertAlmostEqual(abs(c_sv["|00>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(c_sv["|11>"]), 1.0 / math.sqrt(2), places=6)

    def test_20_entropy_calculation_parity(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
cx q[0], q[1];
"""
        _, c_source, _ = run_zcc_emit_c(qasm)
        c_rc, c_out, c_err = compile_and_run_generated_c(c_source, extra_args=["--entropy"])
        self.assertEqual(c_rc, 0, f"Entropy execution failed: {c_err}")
        self.assertIn("S(q0) = 1.000000 bits", c_out)
        self.assertIn("S(q1) = 1.000000 bits", c_out)

    def test_21_randomized_differential_fuzzing_20_circuits(self):
        gate_pool_1q = ["h", "x", "y", "z", "s", "sdg", "t", "tdg"]
        gate_pool_rot = ["rx", "ry", "rz", "p"]
        gate_pool_2q = ["cx", "cz", "swap", "iswap"]

        for seed in range(20):
            rng = random.Random(seed + 2026)
            num_qubits = rng.randint(2, 4)
            num_gates = rng.randint(8, 20)

            lines = [
                'OPENQASM 2.0;',
                'include "qelib1.inc";',
                f'qreg q[{num_qubits}];'
            ]

            for _ in range(num_gates):
                r = rng.random()
                if r < 0.4:
                    g = rng.choice(gate_pool_1q)
                    q = rng.randint(0, num_qubits - 1)
                    lines.append(f"{g} q[{q}];")
                elif r < 0.7:
                    g = rng.choice(gate_pool_rot)
                    q = rng.randint(0, num_qubits - 1)
                    angle = rng.choice([math.pi/4, math.pi/2, math.pi/3, -math.pi/4])
                    lines.append(f"{g}({angle:.6f}) q[{q}];")
                else:
                    g = rng.choice(gate_pool_2q)
                    q0 = rng.randint(0, num_qubits - 1)
                    q1 = rng.randint(0, num_qubits - 1)
                    while q1 == q0:
                        q1 = rng.randint(0, num_qubits - 1)
                    lines.append(f"{g} q[{q0}], q[{q1}];")

            qasm_src = "\n".join(lines) + "\n"
            self._verify_circuit_differential(qasm_src, seed=seed + 100)

    def test_22_file_output_option_dash_o(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
cx q[0], q[1];
"""
        with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f_qasm:
            f_qasm.write(qasm)
            qasm_path = f_qasm.name
        c_out_path = qasm_path + ".c"
        try:
            res = subprocess.run([ZCC_BIN, "--target=qasm-sim-c", qasm_path, "-o", c_out_path], capture_output=True, text=True)
            self.assertEqual(res.returncode, 0, f"zcc -o failed: {res.stderr}")
            self.assertTrue(os.path.exists(c_out_path), "output C file created by -o")
            with open(c_out_path, "r") as f:
                c_content = f.read()
            self.assertIn("void run_quantum_circuit", c_content)

            # Compile and run
            c_rc, c_out, c_err = compile_and_run_generated_c(c_content)
            self.assertEqual(c_rc, 0)
            sv = parse_statevector_dump(c_out)
            self.assertAlmostEqual(abs(sv["|00>"]), 1.0 / math.sqrt(2), places=6)
            self.assertAlmostEqual(abs(sv["|11>"]), 1.0 / math.sqrt(2), places=6)
        finally:
            if os.path.exists(qasm_path):
                os.remove(qasm_path)
            if os.path.exists(c_out_path):
                os.remove(c_out_path)

    def test_23_complex_parameter_expressions(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
rx(pi / 4 + pi / 4) q[0];
ry((pi * 2) / 3 - pi / 6) q[1];
rz(sin(pi/2) * pi / 3) q[0];
"""
        self._verify_circuit_differential(qasm)

    def test_24_ghz_5qubit(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[5];
h q[0];
cx q[0], q[1];
cx q[1], q[2];
cx q[2], q[3];
cx q[3], q[4];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|00000>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(c_sv["|11111>"]), 1.0 / math.sqrt(2), places=6)

    def test_25_deutsch_jozsa_oracle(self):
        # Balanced oracle f(x0, x1) = x0 ^ x1
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
qreg anc[1];
creg c[2];
x anc[0];
h q[0];
h q[1];
h anc[0];
cx q[0], anc[0];
cx q[1], anc[0];
h q[0];
h q[1];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|011>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(c_sv["|111>"]), 1.0 / math.sqrt(2), places=6)

    def test_26_grover_iteration_2qubit(self):
        # Grover's search for |11>
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
h q[1];
cz q[0], q[1];
h q[0];
h q[1];
x q[0];
x q[1];
cz q[0], q[1];
x q[0];
x q[1];
h q[0];
h q[1];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|11>"]), 1.0, places=6)

    def test_27_nested_custom_gates(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
gate bell a, b {
    h a;
    cx a, b;
}
gate double_bell a, b, c, d {
    bell a, b;
    bell c, d;
}
qreg q[4];
double_bell q[0], q[1], q[2], q[3];
"""
        sim_sv, c_sv = self._verify_circuit_differential(qasm)
        self.assertAlmostEqual(abs(c_sv["|0000>"]), 0.5, places=6)
        self.assertAlmostEqual(abs(c_sv["|0011>"]), 0.5, places=6)
        self.assertAlmostEqual(abs(c_sv["|1100>"]), 0.5, places=6)
        self.assertAlmostEqual(abs(c_sv["|1111>"]), 0.5, places=6)

    def test_28_controlled_rotation_gates(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
crx(pi/3) q[0], q[1];
cry(pi/4) q[0], q[1];
crz(pi/6) q[0], q[1];
cu1(0.7) q[0], q[1];
cu3(0.2, 0.4, 0.6) q[0], q[1];
"""
        self._verify_circuit_differential(qasm)

    def test_29_sparse_vs_dense_sampling(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[0];
cx q[0], q[1];
"""
        _, c_source, _ = run_zcc_emit_c(qasm)
        c_rc, c_out, c_err = compile_and_run_generated_c(c_source, extra_args=["--threshold=0.4"])
        self.assertEqual(c_rc, 0)
        lines = [l for l in c_out.strip().splitlines() if l.startswith("|")]
        self.assertEqual(len(lines), 2)
        self.assertIn("|000>", lines[0])
        self.assertIn("|011>", lines[1])

    def test_30_no_dump_flag_and_cli_args(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
h q[0];
"""
        _, c_source, _ = run_zcc_emit_c(qasm)
        c_rc, c_out, c_err = compile_and_run_generated_c(c_source, extra_args=["--no-dump"])
        self.assertEqual(c_rc, 0)
        self.assertEqual(c_out.strip(), "")


if __name__ == "__main__":
    unittest.main()

