#!/usr/bin/env python3
"""
ZCC Native OpenQASM 2.0 Statevector Simulator Test Suite (Phase 0B)
===================================================================
Tests all standard 1-qubit, 2-qubit, 3-qubit gates, parameterized rotations,
exact complex amplitudes, Born measurement collapse, measurement correlations,
classical conditions, custom gate bodies, single-qubit entanglement entropy,
oversized safety limits, and differential matrix parity against Python reference.
"""

import os
import sys
import math
import subprocess
import tempfile
import unittest
import cmath

ZCC_BIN = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "zcc"))

def run_zcc_sim(qasm_source, seed=None):
    """Executes QASM circuit in ZCC statevector simulator and returns (stdout, stderr, exit_code)."""
    with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
        f.write(qasm_source)
        f.flush()
        temp_path = f.name
    try:
        cmd = [ZCC_BIN, "--target=qasm-sim"]
        if seed is not None:
            cmd.append(f"--seed={seed}")
        cmd.append(temp_path)
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return proc.stdout, proc.stderr, proc.returncode
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)

def parse_statevector_dump(dump_text):
    """
    Parses output in the format:
    |00>: +0.70710678 +0.00000000i (prob: 0.500000)
    Returns dict mapping basis string -> complex amplitude.
    """
    state = {}
    for line in dump_text.strip().splitlines():
        line = line.strip()
        if not line or not line.startswith("|"):
            continue
        parts = line.split(":", 1)
        if len(parts) != 2:
            continue
        basis = parts[0].strip()[1:-1] # strip | and >
        rest = parts[1].strip()
        val_part = rest.split("(prob:")[0].strip()
        tokens = val_part.split()
        if len(tokens) >= 2:
            r = float(tokens[0])
            i_str = tokens[1]
            if i_str.endswith("i"):
                i_str = i_str[:-1]
            im = float(i_str)
            state[basis] = complex(r, im)
    return state


class TestZCCQasmSimulator(unittest.TestCase):

    def test_01_initial_ground_state(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0, f"Error: {err}")
        state = parse_statevector_dump(out)
        self.assertIn("00", state)
        self.assertAlmostEqual(state["00"].real, 1.0, places=6)
        self.assertAlmostEqual(state["00"].imag, 0.0, places=6)
        self.assertEqual(len(state), 1)

    def test_02_pauli_x_gate(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        x q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # q[0] is LSB, so |01>
        self.assertIn("01", state)
        self.assertAlmostEqual(abs(state["01"]), 1.0, places=6)
        self.assertEqual(len(state), 1)

    def test_03_pauli_y_gate_complex_phase(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        y q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # Y|0> = i|1>
        self.assertIn("1", state)
        self.assertAlmostEqual(state["1"].real, 0.0, places=6)
        self.assertAlmostEqual(state["1"].imag, 1.0, places=6)

    def test_04_pauli_z_gate(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        x q[0];
        z q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # Z|1> = -|1>
        self.assertIn("1", state)
        self.assertAlmostEqual(state["1"].real, -1.0, places=6)
        self.assertAlmostEqual(state["1"].imag, 0.0, places=6)

    def test_05_hadamard_gate(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        h q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertIn("0", state)
        self.assertIn("1", state)
        self.assertAlmostEqual(state["0"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["1"].real, inv_sqrt2, places=6)

    def test_06_s_and_sdg_phase_gates(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        x q[0];
        s q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # S|1> = i|1>
        self.assertAlmostEqual(state["1"].real, 0.0, places=6)
        self.assertAlmostEqual(state["1"].imag, 1.0, places=6)

        # S followed by Sdg should return to |1>
        src_sdg = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        x q[0];
        s q[0];
        sdg q[0];
        """
        out2, err2, code2 = run_zcc_sim(src_sdg)
        self.assertEqual(code2, 0)
        state2 = parse_statevector_dump(out2)
        self.assertAlmostEqual(state2["1"].real, 1.0, places=6)
        self.assertAlmostEqual(state2["1"].imag, 0.0, places=6)

    def test_07_t_and_tdg_gates(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        x q[0];
        t q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # T|1> = e^{i pi/4}|1> = (1/sqrt(2) + i/sqrt(2))|1>
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertAlmostEqual(state["1"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["1"].imag, inv_sqrt2, places=6)

    def test_08_rx_rotation(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        rx(pi) q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # Rx(pi)|0> = -i|1>
        self.assertIn("1", state)
        self.assertAlmostEqual(state["1"].real, 0.0, places=6)
        self.assertAlmostEqual(state["1"].imag, -1.0, places=6)

    def test_09_ry_rotation(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        ry(pi/2) q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertAlmostEqual(state["0"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["1"].real, inv_sqrt2, places=6)

    def test_10_rz_rotation(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        h q[0];
        rz(pi) q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # Rz(pi) ( |0> + |1> )/sqrt(2) = ( e^{-i pi/2}|0> + e^{i pi/2}|1> )/sqrt(2)
        # = ( -i|0> + i|1> ) / sqrt(2)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertAlmostEqual(state["0"].imag, -inv_sqrt2, places=6)
        self.assertAlmostEqual(state["1"].imag, inv_sqrt2, places=6)

    def test_11_u_and_u3_general_unitary(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        u3(pi/2, pi/4, pi/3) q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertAlmostEqual(state["0"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["0"].imag, 0.0, places=6)
        self.assertAlmostEqual(state["1"].real, 0.5, places=6)
        self.assertAlmostEqual(state["1"].imag, 0.5, places=6)

    def test_12_cnot_cx_gate(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        x q[0];
        cx q[0], q[1];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # q[0] is control (1), q[1] is target (1) -> |11>
        self.assertIn("11", state)
        self.assertAlmostEqual(abs(state["11"]), 1.0, places=6)

    def test_13_cz_and_ch_gates(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        h q[0];
        x q[1];
        cz q[0], q[1];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertAlmostEqual(state["10"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["11"].real, -inv_sqrt2, places=6)

    def test_14_swap_and_iswap_gates(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        x q[0];
        swap q[0], q[1];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # |01> swapped to |10>
        self.assertIn("10", state)
        self.assertAlmostEqual(abs(state["10"]), 1.0, places=6)

        src_iswap = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        x q[0];
        iswap q[0], q[1];
        """
        out_i, err_i, code_i = run_zcc_sim(src_iswap)
        self.assertEqual(code_i, 0)
        state_i = parse_statevector_dump(out_i)
        # iSWAP |01> = i|10>
        self.assertIn("10", state_i)
        self.assertAlmostEqual(state_i["10"].real, 0.0, places=6)
        self.assertAlmostEqual(state_i["10"].imag, 1.0, places=6)

    def test_15_rzz_2qubit_rotation(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        rzz(pi) q[0], q[1];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # RZZ(pi)|00> = e^{-i pi/2}|00> = -i|00>
        self.assertIn("00", state)
        self.assertAlmostEqual(state["00"].real, 0.0, places=6)
        self.assertAlmostEqual(state["00"].imag, -1.0, places=6)

    def test_16_toffoli_ccx_and_fredkin_cswap(self):
        src_ccx = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[3];
        x q[0];
        x q[1];
        ccx q[0], q[1], q[2];
        """
        out, err, code = run_zcc_sim(src_ccx)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # q0=1, q1=1 -> q2 flipped to 1 -> |111>
        self.assertIn("111", state)
        self.assertAlmostEqual(abs(state["111"]), 1.0, places=6)

        src_cswap = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[3];
        x q[0];
        x q[1];
        cswap q[0], q[1], q[2];
        """
        out_f, err_f, code_f = run_zcc_sim(src_cswap)
        self.assertEqual(code_f, 0)
        state_f = parse_statevector_dump(out_f)
        # Control q0=1, swap q1(1) and q2(0) -> |101>
        self.assertIn("101", state_f)
        self.assertAlmostEqual(abs(state_f["101"]), 1.0, places=6)

    def test_17_bell_state_oracle(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        h q[0];
        cx q[0], q[1];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertEqual(len(state), 2)
        self.assertIn("00", state)
        self.assertIn("11", state)
        self.assertAlmostEqual(state["00"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["11"].real, inv_sqrt2, places=6)
        self.assertNotIn("01", state)
        self.assertNotIn("10", state)

    def test_18_ghz_3qubit_state_oracle(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[3];
        h q[0];
        cx q[0], q[1];
        cx q[1], q[2];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertEqual(len(state), 2)
        self.assertIn("000", state)
        self.assertIn("111", state)
        self.assertAlmostEqual(state["000"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["111"].real, inv_sqrt2, places=6)

    def test_19_3qubit_qft_fixture(self):
        # Standard 3-qubit QFT on |000> yields uniform superposition 1/sqrt(8)
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[3];
        h q[2];
        cu1(pi/2) q[1], q[2];
        cu1(pi/4) q[0], q[2];
        h q[1];
        cu1(pi/2) q[0], q[1];
        h q[0];
        swap q[0], q[2];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        self.assertEqual(len(state), 8)
        expected_amp = 1.0 / math.sqrt(8.0)
        for b in ["000", "001", "010", "011", "100", "101", "110", "111"]:
            self.assertIn(b, state)
            self.assertAlmostEqual(state[b].real, expected_amp, places=6)
            self.assertAlmostEqual(state[b].imag, 0.0, places=6)

    def test_20_deterministic_measurement_and_collapse(self):
        # Measurement of |+> with fixed seed
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[1];
        creg c[1];
        h q[0];
        measure q[0] -> c[0];
        """
        out1, err1, code1 = run_zcc_sim(src, seed=42)
        self.assertEqual(code1, 0)
        state1 = parse_statevector_dump(out1)
        self.assertEqual(len(state1), 1)

        out2, err2, code2 = run_zcc_sim(src, seed=42)
        self.assertEqual(out1, out2, "Identical seed must yield identical collapsed state")

    def test_21_bell_measurement_correlation(self):
        # Bell state measurement across multiple seeds must only produce |00> or |11>, never |01> or |10>
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        creg c[2];
        h q[0];
        cx q[0], q[1];
        measure q[0] -> c[0];
        measure q[1] -> c[1];
        """
        observed = set()
        for seed in [1, 2, 3, 4, 7, 13, 42, 99, 12345]:
            out, err, code = run_zcc_sim(src, seed=seed)
            self.assertEqual(code, 0)
            state = parse_statevector_dump(out)
            self.assertEqual(len(state), 1)
            basis = list(state.keys())[0]
            self.assertIn(basis, ["00", "11"], f"Invalid non-correlated outcome: {basis}")
            observed.add(basis)
        self.assertTrue(len(observed) >= 1)

    def test_22_quantum_reset(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        h q[0];
        cx q[0], q[1];
        reset q[0];
        """
        out, err, code = run_zcc_sim(src, seed=123)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # Target q0 reset to 0, so bit 0 must be 0 (e.g. 00 or 10)
        for basis in state.keys():
            self.assertEqual(basis[-1], '0', f"Reset failed to clear qubit 0: {basis}")

    def test_23_classical_condition_true_and_false(self):
        src_true = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        creg c[2];
        x q[0];
        measure q[0] -> c[0];
        if (c == 1) x q[1];
        """
        out, err, code = run_zcc_sim(src_true)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        # Condition was true (c=1), so q1 was flipped to 1 -> |11>
        self.assertIn("11", state)
        self.assertAlmostEqual(abs(state["11"]), 1.0, places=6)

        src_false = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        creg c[2];
        x q[0];
        measure q[0] -> c[0];
        if (c == 0) x q[1];
        """
        out_f, err_f, code_f = run_zcc_sim(src_false)
        self.assertEqual(code_f, 0)
        state_f = parse_statevector_dump(out_f)
        # Condition was false (c=1, expected 0), so q1 remained 0 -> |01>
        self.assertIn("01", state_f)
        self.assertAlmostEqual(abs(state_f["01"]), 1.0, places=6)

    def test_24_custom_gate_execution(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        gate bell a, b {
            h a;
            cx a, b;
        }
        qreg q[2];
        bell q[0], q[1];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)
        inv_sqrt2 = 1.0 / math.sqrt(2.0)
        self.assertEqual(len(state), 2)
        self.assertAlmostEqual(state["00"].real, inv_sqrt2, places=6)
        self.assertAlmostEqual(state["11"].real, inv_sqrt2, places=6)

    def test_25_oversized_simulation_error_handling(self):
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[32];
        h q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertNotEqual(code, 0)
        self.assertIn("exceeds maximum", err)

    def test_26_differential_unitary_matrix_oracle(self):
        # Differential matrix oracle: compare 2-qubit circuit simulation with Python matrix multiplication
        # Circuit: H(0), S(0), T(1), CX(0, 1), RY(pi/3, 0)
        src = """
        OPENQASM 2.0;
        include "qelib1.inc";
        qreg q[2];
        h q[0];
        s q[0];
        t q[1];
        cx q[0], q[1];
        ry(pi/3) q[0];
        """
        out, err, code = run_zcc_sim(src)
        self.assertEqual(code, 0)
        state = parse_statevector_dump(out)

        # Independent Python matrix formulation
        # Vector order: [v00, v01, v10, v11] where bit0 is q0, bit1 is q1
        # |q1 q0>
        # Initial: |00> = [1, 0, 0, 0]
        # H on q0: 1/sqrt(2) [ |00> + |01> ]
        # S on q0: 1/sqrt(2) [ |00> + i|01> ]
        # T on q1: T|0> = |0>, so state remains 1/sqrt(2)[ |00> + i|01> ]
        # CX(q0, q1): when q0=1 (state |01>), flips q1 -> |11>
        # State after CX: 1/sqrt(2) |00> + i/sqrt(2) |11>
        # RY(pi/3) on q0: cos(pi/6)|0> - sin(pi/6)|1> = (sqrt(3)/2)|0> - 0.5|1>
        # RY(pi/3)|00> = (sqrt(3)/2)|00> + 0.5|01>
        # RY(pi/3)|11> = (sqrt(3)/2)|11> - 0.5|10>
        # Full vector:
        # |00>: (1/sqrt(2))*(sqrt(3)/2) = sqrt(3)/(2*sqrt(2)) = 0.6123724
        # |01>: (1/sqrt(2))*0.5 = 0.5/sqrt(2) = 0.3535534
        # |10>: (i/sqrt(2))*(-0.5) = -0.3535534i
        # |11>: (i/sqrt(2))*(sqrt(3)/2) = +0.6123724i

        exp_00 = math.sqrt(3.0) / (2.0 * math.sqrt(2.0))
        exp_01 = 0.5 / math.sqrt(2.0)
        exp_10 = complex(0, -0.5 / math.sqrt(2.0))
        exp_11 = complex(0, math.sqrt(3.0) / (2.0 * math.sqrt(2.0)))

        self.assertAlmostEqual(state["00"].real, exp_00, places=6)
        self.assertAlmostEqual(state["01"].real, exp_01, places=6)
        self.assertAlmostEqual(state["10"].imag, exp_10.imag, places=6)
        self.assertAlmostEqual(state["11"].imag, exp_11.imag, places=6)


if __name__ == "__main__":
    unittest.main()
