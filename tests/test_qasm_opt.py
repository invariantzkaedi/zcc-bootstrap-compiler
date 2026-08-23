#!/usr/bin/env python3
"""
ZCC OpenQASM 2.0 Circuit Optimizer Test Suite (Phase 0C)
Tests algebraic rewrites, commutation sliding, fixed-point convergence,
and statevector equivalence oracle against Phase 0B simulator.
"""

import math
import os
import random
import subprocess
import tempfile
import unittest

ZCC_BIN = os.path.abspath("./zcc")


def run_zcc_opt(qasm_source, extra_args=None):
    with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
        f.write(qasm_source)
        f_name = f.name
    try:
        cmd = [ZCC_BIN, "--target=qasm-opt", f_name]
        if extra_args:
            cmd.extend(extra_args)
        res = subprocess.run(cmd, capture_output=True, text=True)
        return res.returncode, res.stdout, res.stderr
    finally:
        if os.path.exists(f_name):
            os.remove(f_name)


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


def assert_statevectors_equivalent_up_to_global_phase(test_case, sv1, sv2, tol=1e-10):
    all_keys = set(sv1.keys()).union(set(sv2.keys()))
    phase_ratio = None
    for k in sorted(all_keys):
        a = sv1.get(k, 0.0 + 0.0j)
        b = sv2.get(k, 0.0 + 0.0j)
        if abs(a) > tol and abs(b) > tol:
            phase_ratio = a / b
            phase_ratio = phase_ratio / abs(phase_ratio)
            break
        elif abs(a) > tol or abs(b) > tol:
            test_case.fail(f"Amplitude support mismatch at basis {k}: {a} vs {b}")

    if phase_ratio is None:
        phase_ratio = 1.0 + 0.0j

    for k in all_keys:
        a = sv1.get(k, 0.0 + 0.0j)
        b = sv2.get(k, 0.0 + 0.0j)
        b_rot = b * phase_ratio
        test_case.assertAlmostEqual(
            a.real, b_rot.real, delta=tol,
            msg=f"Real amplitude divergence at basis {k}: {a} vs rotated {b_rot}"
        )
        test_case.assertAlmostEqual(
            a.imag, b_rot.imag, delta=tol,
            msg=f"Imag amplitude divergence at basis {k}: {a} vs rotated {b_rot}"
        )


class TestZCCQasmOptimizer(unittest.TestCase):

    def test_01_h_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
h q[0];
h q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("h q[0];", out)

    def test_02_x_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
x q[0];
x q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("x q[0];", out)

    def test_03_y_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
y q[0];
y q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("y q[0];", out)

    def test_04_z_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
z q[0];
z q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("z q[0];", out)

    def test_05_cx_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cx q[0],q[1];
cx q[0],q[1];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("cx", out)

    def test_06_cz_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cz q[0],q[1];
cz q[0],q[1];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("cz", out)

    def test_07_swap_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
swap q[0],q[1];
swap q[1],q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("swap", out)

    def test_08_s_sdg_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
s q[0];
sdg q[0];
sdg q[0];
s q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("s q[0];", out)
        self.assertNotIn("sdg q[0];", out)

    def test_09_t_tdg_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
t q[0];
tdg q[0];
tdg q[0];
t q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("t q[0];", out)
        self.assertNotIn("tdg q[0];", out)

    def test_10_rx_fusion(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
rx(pi/4) q[0];
rx(pi/4) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertIn("rx(", out)
        self.assertEqual(out.count("rx("), 1)
        # Check simulator equivalence
        _, sim_orig, _ = run_zcc_sim(qasm)
        _, sim_opt, _ = run_zcc_sim(out)
        assert_statevectors_equivalent_up_to_global_phase(self, parse_statevector_dump(sim_orig), parse_statevector_dump(sim_opt))

    def test_11_ry_fusion(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
ry(pi/3) q[0];
ry(pi/6) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("ry("), 1)
        _, sim_orig, _ = run_zcc_sim(qasm)
        _, sim_opt, _ = run_zcc_sim(out)
        assert_statevectors_equivalent_up_to_global_phase(self, parse_statevector_dump(sim_orig), parse_statevector_dump(sim_opt))

    def test_12_rz_fusion(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
rz(pi) q[0];
rz(-pi) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("rz(", out)

    def test_13_p_fusion(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
p(pi/8) q[0];
p(pi/8) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("p("), 1)

    def test_14_u1_fusion(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
u1(0.2) q[0];
u1(0.3) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("u1("), 1)

    def test_15_zero_rotation_elimination(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
rx(0) q[0];
ry(0) q[0];
rz(0) q[0];
p(0) q[0];
u1(0) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("rx(", out)
        self.assertNotIn("ry(", out)
        self.assertNotIn("rz(", out)
        self.assertNotIn("p(", out)
        self.assertNotIn("u1(", out)

    def test_16_periodic_zero_rotation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
rx(2*pi) q[0];
rz(-4*pi) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("rx(", out)
        self.assertNotIn("rz(", out)

    def test_17_u3_identity_elimination(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
u3(0, 0, 0) q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("u3(", out)

    def test_18_disjoint_sliding_h_x_h(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
x q[1];
h q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("h q[0];", out)
        self.assertIn("x q[1];", out)

    def test_19_disjoint_sliding_cx_h_cx(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
cx q[0],q[1];
h q[2];
cx q[0],q[1];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("cx", out)
        self.assertIn("h q[2];", out)

    def test_20_negative_same_wire_interference(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
h q[0];
x q[0];
h q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("h q[0];"), 2)
        self.assertIn("x q[0];", out)

    def test_21_reversed_cx_non_cancellation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cx q[0],q[1];
cx q[1],q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("cx"), 2)

    def test_22_measurement_barrier(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
creg c[1];
h q[0];
measure q[0] -> c[0];
h q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("h q[0];"), 2)

    def test_23_reset_barrier(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
x q[0];
reset q[0];
x q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertEqual(out.count("x q[0];"), 2)

    def test_24_classical_condition_barrier(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[1];
x q[0];
if (c == 1) z q[1];
x q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertIn("if (c == 1) z q[1];", out)

    def test_25_global_phase_equivalence(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
h q[0];
s q[0];
t q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0)
        _, sim_orig, _ = run_zcc_sim(qasm)
        _, sim_opt, _ = run_zcc_sim(out)
        assert_statevectors_equivalent_up_to_global_phase(self, parse_statevector_dump(sim_orig), parse_statevector_dump(sim_opt))

    def test_26_bell_preservation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
x q[1];
x q[1];
cx q[0],q[1];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("x q[1];", out)
        self.assertIn("h q[0];", out)
        self.assertIn("cx q[0], q[1];", out)

        _, sim_orig, _ = run_zcc_sim(qasm)
        _, sim_opt, _ = run_zcc_sim(out)
        orig_sv = parse_statevector_dump(sim_orig)
        opt_sv = parse_statevector_dump(sim_opt)
        assert_statevectors_equivalent_up_to_global_phase(self, orig_sv, opt_sv)
        self.assertAlmostEqual(abs(opt_sv["|00>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(opt_sv["|11>"]), 1.0 / math.sqrt(2), places=6)

    def test_27_ghz_preservation(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[0];
z q[2];
z q[2];
cx q[0],q[1];
cx q[1],q[2];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("z q[2];", out)

        _, sim_orig, _ = run_zcc_sim(qasm)
        _, sim_opt, _ = run_zcc_sim(out)
        orig_sv = parse_statevector_dump(sim_orig)
        opt_sv = parse_statevector_dump(sim_opt)
        assert_statevectors_equivalent_up_to_global_phase(self, orig_sv, opt_sv)
        self.assertAlmostEqual(abs(opt_sv["|000>"]), 1.0 / math.sqrt(2), places=6)
        self.assertAlmostEqual(abs(opt_sv["|111>"]), 1.0 / math.sqrt(2), places=6)

    def test_28_randomized_differential_circuits(self):
        gate_pool_1q = ["h", "x", "y", "z", "s", "sdg", "t", "tdg"]
        gate_pool_rot = ["rx", "ry", "rz", "p"]
        gate_pool_2q = ["cx", "cz", "swap"]

        for seed in range(25):
            rng = random.Random(seed + 1000)
            num_qubits = rng.randint(2, 4)
            num_gates = rng.randint(10, 30)

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
                    angle = rng.choice([0.0, math.pi/4, math.pi/2, math.pi, -math.pi/4, -math.pi/2])
                    lines.append(f"{g}({angle:.6f}) q[{q}];")
                else:
                    g = rng.choice(gate_pool_2q)
                    q0 = rng.randint(0, num_qubits - 1)
                    q1 = rng.randint(0, num_qubits - 1)
                    while q1 == q0:
                        q1 = rng.randint(0, num_qubits - 1)
                    lines.append(f"{g} q[{q0}],q[{q1}];")

            qasm_src = "\n".join(lines) + "\n"
            rc, out, err = run_zcc_opt(qasm_src)
            self.assertEqual(rc, 0, f"Random circuit {seed} optimization failed: {err}")

            rc_orig, sim_orig, _ = run_zcc_sim(qasm_src)
            rc_opt, sim_opt, _ = run_zcc_sim(out)
            self.assertEqual(rc_orig, 0)
            self.assertEqual(rc_opt, 0)

            sv_orig = parse_statevector_dump(sim_orig)
            sv_opt = parse_statevector_dump(sim_opt)
            assert_statevectors_equivalent_up_to_global_phase(self, sv_orig, sv_opt)

    def test_29_optimizer_idempotence(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
h q[0];
rx(pi/4) q[1];
rx(pi/4) q[1];
cx q[0],q[1];
"""
        rc1, out1, _ = run_zcc_opt(qasm)
        self.assertEqual(rc1, 0)
        rc2, out2, _ = run_zcc_opt(out1)
        self.assertEqual(rc2, 0)
        self.assertEqual(out1.strip(), out2.strip())

    def test_30_deterministic_output(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[0];
x q[2];
h q[0];
rx(pi/3) q[1];
rx(pi/6) q[1];
cx q[0],q[1];
cx q[0],q[1];
"""
        runs = []
        for _ in range(5):
            rc, out, _ = run_zcc_opt(qasm)
            self.assertEqual(rc, 0)
            runs.append(out)
        for out in runs[1:]:
            self.assertEqual(runs[0], out)

    def test_31_gate_count_monotonicity(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
x q[1];
h q[0];
rx(pi/4) q[0];
rx(pi/4) q[0];
"""
        rc, out, _ = run_zcc_opt(qasm)
        self.assertEqual(rc, 0)
        orig_count = qasm.count(";") - 2  # exclude header & qreg
        opt_count = out.count(";") - 2
        self.assertLessEqual(opt_count, orig_count)

    def test_32_max_iteration_convergence(self):
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
h q[0];
h q[0];
x q[0];
x q[0];
"""
        rc, out, err = run_zcc_opt(qasm)
        self.assertEqual(rc, 0, f"Opt failed: {err}")
        self.assertNotIn("h q[0];", out)
        self.assertNotIn("x q[0];", out)


if __name__ == "__main__":
    unittest.main()
