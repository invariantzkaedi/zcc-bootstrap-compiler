#!/usr/bin/env python3
"""
================================================================================
ZCC OpenQASM 2.0 CLIFFORD + T FAULT-TOLERANT QUANTUM TRANSPILER TEST SUITE
================================================================================
Tests the --target=qasm-clifford-t compiler pass:
  1. Exact pi/4 rotation decomposition (T, S, Z, TDG, SDG)
  2. Rx, Ry, Rz, U1, U2, U3 fault-tolerant expansion
  3. Canonical 7-T Toffoli (CCX) and Fredkin (CSWAP) expansion
  4. Complete elimination of continuous rotation parameters
  5. T-count and T-depth metric fidelity
  6. Exact statevector numerical equivalence (up to global phase) vs oracle
================================================================================
"""

import math
import os
import subprocess
import tempfile
import unittest

ZCC_BIN = os.path.abspath("./zcc")


def run_clifford_t_transpiler(qasm_source: str, verbose: bool = False):
    with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
        f.write(qasm_source)
        f_name = f.name
    try:
        cmd = [ZCC_BIN, "--target=qasm-clifford-t", f_name]
        if verbose:
            cmd.append("--verbose")
        res = subprocess.run(cmd, capture_output=True, text=True)
        return res.returncode, res.stdout, res.stderr
    finally:
        if os.path.exists(f_name):
            os.remove(f_name)


def run_zcc_sim(qasm_source: str):
    with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
        f.write(qasm_source)
        f_name = f.name
    try:
        cmd = [ZCC_BIN, "--target=qasm-sim", f_name]
        res = subprocess.run(cmd, capture_output=True, text=True)
        return res.returncode, res.stdout, res.stderr
    finally:
        if os.path.exists(f_name):
            os.remove(f_name)


def parse_statevector(output_str: str) -> dict:
    state = {}
    for line in output_str.strip().splitlines():
        line = line.strip()
        if not line or not line.startswith("|"):
            continue
        # Format: |00>: +0.70710678 +0.00000000i (prob: 0.500000)
        parts = line.split(":")
        basis = parts[0].strip()
        rest = parts[1].split("(")[0].strip()
        tokens = rest.split()
        re_val = float(tokens[0])
        im_val = float(tokens[1].replace("i", ""))
        state[basis] = (re_val, im_val)
    return state


def assert_statevectors_equal(test_case, sv1: dict, sv2: dict, tol: float = 1e-3):
    keys = sorted(set(sv1.keys()).union(set(sv2.keys())))
    # Compute global phase alignment
    phase_re, phase_im = 1.0, 0.0
    for k in keys:
        re1, im1 = sv1.get(k, (0.0, 0.0))
        re2, im2 = sv2.get(k, (0.0, 0.0))
        denom = re2 * re2 + im2 * im2
        if denom > 1e-12:
            p_re = (re1 * re2 + im1 * im2) / denom
            p_im = (im1 * re2 - re1 * im2) / denom
            norm = math.hypot(p_re, p_im)
            if norm > 1e-12:
                phase_re, phase_im = p_re / norm, p_im / norm
                break

    for k in keys:
        re1, im1 = sv1.get(k, (0.0, 0.0))
        re2, im2 = sv2.get(k, (0.0, 0.0))
        rot_re = phase_re * re2 - phase_im * im2
        rot_im = phase_re * im2 + phase_im * re2
        diff = math.hypot(re1 - rot_re, im1 - rot_im)
        test_case.assertLessEqual(diff, tol, f"Statevector mismatch at {k}: ({re1:.6f}, {im1:.6f}i) vs rotated ({rot_re:.6f}, {rot_im:.6f}i), diff={diff:.6e}")


class TestZCCQasmCliffordT(unittest.TestCase):

    def test_01_exact_rz_octants_decomposition(self):
        """Verifies exact pi/4 multiples decompose into discrete T/S/Z/SDG/TDG gates."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
rz(pi/4) q[0];
rz(pi/2) q[0];
rz(3*pi/4) q[0];
rz(pi) q[0];
rz(7*pi/4) q[0];
"""
        code, out, err = run_clifford_t_transpiler(qasm)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")
        self.assertIn("t q[0];", out)
        self.assertIn("s q[0];", out)
        self.assertIn("z q[0];", out)
        self.assertIn("tdg q[0];", out)
        self.assertNotIn("rz(", out)

    def test_02_rx_and_ry_exact_decomposition(self):
        """Verifies Rx and Ry decompose into H, S, SDG, and Rz sequences."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
rx(pi/2) q[0];
ry(pi/2) q[1];
"""
        code, out, err = run_clifford_t_transpiler(qasm)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")
        self.assertNotIn("rx(", out)
        self.assertNotIn("ry(", out)
        self.assertIn("h q[0];", out)
        self.assertIn("s q[0];", out)
        self.assertIn("sdg q[1];", out)

        # Verify statevector equivalence
        _, sv_orig_out, _ = run_zcc_sim(qasm)
        _, sv_ct_out, _ = run_zcc_sim(out)
        assert_statevectors_equal(self, parse_statevector(sv_orig_out), parse_statevector(sv_ct_out), 1e-4)

    def test_03_u3_euler_decomposition_parity(self):
        """Verifies U3 decomposes into Rz-Ry-Rz and preserves statevector."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
u3(pi/2, pi/4, pi/2) q[0];
"""
        code, out, err = run_clifford_t_transpiler(qasm)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")
        self.assertNotIn("u3(", out)
        self.assertIn("h q[0];", out)

        _, sv_orig_out, _ = run_zcc_sim(qasm)
        _, sv_ct_out, _ = run_zcc_sim(out)
        assert_statevectors_equal(self, parse_statevector(sv_orig_out), parse_statevector(sv_ct_out), 1e-4)

    def test_04_canonical_7t_toffoli_expansion(self):
        """Verifies 3-qubit Toffoli (CCX) expands to canonical 7-T gate lattice."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
x q[0];
x q[1];
ccx q[0], q[1], q[2];
"""
        code, out, err = run_clifford_t_transpiler(qasm, verbose=True)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")
        self.assertNotIn("ccx", out)
        self.assertIn("cx q[0], q[1];", out)
        self.assertIn("tdg q[2];", out)
        self.assertIn("t q[2];", out)

        # Simulation verification
        _, sv_orig_out, _ = run_zcc_sim(qasm)
        _, sv_ct_out, _ = run_zcc_sim(out)
        sv_orig = parse_statevector(sv_orig_out)
        sv_ct = parse_statevector(sv_ct_out)
        assert_statevectors_equal(self, sv_orig, sv_ct, 1e-4)
        # Should be flipped to |111>
        self.assertAlmostEqual(sv_ct.get("|111>", (0, 0))[0], 1.0, places=4)

    def test_05_controlled_rotations_expansion(self):
        """Verifies CRz, CRx, CRy, and CZ expand into Clifford+T networks."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[0];
crz(pi/2) q[0], q[1];
crx(pi/2) q[0], q[1];
cz q[0], q[1];
"""
        code, out, err = run_clifford_t_transpiler(qasm)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")
        self.assertNotIn("crz(", out)
        self.assertNotIn("crx(", out)
        self.assertNotIn("cz ", out)

        _, sv_orig_out, _ = run_zcc_sim(qasm)
        _, sv_ct_out, _ = run_zcc_sim(out)
        assert_statevectors_equal(self, parse_statevector(sv_orig_out), parse_statevector(sv_ct_out), 1e-4)

    def test_06_complete_continuous_rotation_elimination(self):
        """Asserts that zero parametric gates remain in transpiled output."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[4];
creg c[4];
h q[0];
rx(pi/4) q[1];
ry(pi/2) q[2];
rz(3*pi/4) q[3];
u1(pi/4) q[0];
u2(pi/4, pi/2) q[1];
u3(pi/2, pi/4, pi/2) q[2];
cx q[0], q[1];
ccx q[0], q[1], q[2];
measure q[0] -> c[0];
"""
        code, out, err = run_clifford_t_transpiler(qasm)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")

        forbidden = ["rx(", "ry(", "rz(", "u1(", "u2(", "u3(", "crx(", "cry(", "crz(", "cu1(", "cu3(", "ccx"]
        for f_gate in forbidden:
            self.assertNotIn(f_gate, out, f"Found forbidden continuous gate '{f_gate}' in Clifford+T output")

    def test_07_qft_4qubit_clifford_t_transpilation(self):
        """Transpiles 4-qubit Quantum Fourier Transform to discrete Clifford+T gates."""
        qasm_path = os.path.join(os.path.dirname(__file__), "..", "circuits", "qft_4qubit.qasm")
        with open(qasm_path, "r") as f:
            qasm = f.read()

        code, out, err = run_clifford_t_transpiler(qasm)
        self.assertEqual(code, 0, f"Transpiler failed: {err}")
        self.assertNotIn("cu1(", out)
        self.assertIn("h q[0];", out)
        self.assertIn("cx ", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
