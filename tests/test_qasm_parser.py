#!/usr/bin/env python3
"""
tests/test_qasm_parser.py — ZCC OpenQASM 2.0 Parser, Validator & Canonical Emitter Test Suite

Validates Phase 0A milestone:
- OpenQASM 2.0 syntax parsing (registers, gates, math expressions, measures, resets, barriers, custom gates, ifs)
- Structural AST verification & roundtrip parse -> emit -> reparse equivalence
- Negative diagnostic tests (line/col error messages on duplicate, undeclared, out-of-bounds, arity errors)
"""

import os
import subprocess
import tempfile
import unittest

ZCC_BIN = "./zcc"

class TestZCCQasmParser(unittest.TestCase):

    def setUp(self):
        self.assertTrue(os.path.exists(ZCC_BIN), f"{ZCC_BIN} must exist before running tests")

    def run_zcc_qasm(self, qasm_content, extra_args=None):
        if extra_args is None:
            extra_args = ["--target=qasm-canonical"]
        with tempfile.NamedTemporaryFile(suffix=".qasm", mode="w", delete=False) as f:
            f.write(qasm_content)
            f_path = f.name

        cmd = [ZCC_BIN] + extra_args + [f_path]
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        os.unlink(f_path)
        return res

    def test_01_bell_state_roundtrip(self):
        """Test Bell State circuit parse -> emit -> reparse equivalence."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];
h q[0];
cx q[0], q[1];
measure q[0] -> c[0];
measure q[1] -> c[1];
"""
        res1 = self.run_zcc_qasm(qasm)
        self.assertEqual(res1.returncode, 0, f"ZCC failed on Bell state: {res1.stderr}")
        canonical_output = res1.stdout
        self.assertIn("OPENQASM 2.0;", canonical_output)
        self.assertIn("qreg q[2];", canonical_output)
        self.assertIn("creg c[2];", canonical_output)
        self.assertIn("h q[0];", canonical_output)
        self.assertIn("cx q[0], q[1];", canonical_output)
        self.assertIn("measure q[0] -> c[0];", canonical_output)
        self.assertIn("measure q[1] -> c[1];", canonical_output)

        # Reparse the canonical output
        res2 = self.run_zcc_qasm(canonical_output)
        self.assertEqual(res2.returncode, 0, f"ZCC failed on reparsing canonical output: {res2.stderr}")
        self.assertEqual(canonical_output, res2.stdout, "Reparse of canonical QASM must be byte-identical")

    def test_02_ghz_3qubit_state_roundtrip(self):
        """Test 3-Qubit GHZ state circuit with barrier and broadcast reset."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
creg c[3];
reset q[0];
h q[0];
cx q[0], q[1];
cx q[1], q[2];
barrier q[0], q[1], q[2];
measure q[0] -> c[0];
measure q[1] -> c[1];
measure q[2] -> c[2];
"""
        res1 = self.run_zcc_qasm(qasm)
        self.assertEqual(res1.returncode, 0, f"GHZ parsing failed: {res1.stderr}")
        out1 = res1.stdout

        res2 = self.run_zcc_qasm(out1)
        self.assertEqual(res2.returncode, 0, f"GHZ reparsing failed: {res2.stderr}")
        self.assertEqual(out1, res2.stdout, "GHZ roundtrip must be byte-identical")

    def test_03_parameterized_gates_and_math_expressions(self):
        """Test parameterized rotation gates with mathematical angle expressions."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];
u3(pi/2, 0, pi) q[0];
rx(pi/4 + 0.1) q[0];
ry(sin(pi/6)) q[1];
rz(2*pi/3) q[1];
crz(pi/8) q[0], q[1];
rzz(pi/4) q[0], q[1];
"""
        res1 = self.run_zcc_qasm(qasm)
        self.assertEqual(res1.returncode, 0, f"Parameterized gates failed: {res1.stderr}")
        out1 = res1.stdout
        self.assertIn("u3(", out1)
        self.assertIn("rx(", out1)
        self.assertIn("ry(", out1)
        self.assertIn("rz(", out1)
        self.assertIn("crz(", out1)
        self.assertIn("rzz(", out1)

        res2 = self.run_zcc_qasm(out1)
        self.assertEqual(res2.returncode, 0)
        self.assertEqual(out1, res2.stdout)

    def test_04_custom_gate_definition(self):
        """Test custom composite gate definition and instantiation."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
gate rxx(theta) a, b {
  h a;
  h b;
  cx a, b;
  rz(theta) b;
  cx a, b;
  h a;
  h b;
}
qreg q[2];
creg c[2];
rxx(pi/2) q[0], q[1];
"""
        res1 = self.run_zcc_qasm(qasm)
        self.assertEqual(res1.returncode, 0, f"Custom gate failed: {res1.stderr}")
        out1 = res1.stdout
        self.assertIn("gate rxx(theta) a, b", out1)
        self.assertIn("rxx(", out1)

        res2 = self.run_zcc_qasm(out1)
        self.assertEqual(res2.returncode, 0)
        self.assertEqual(out1, res2.stdout)

    def test_05_conditional_branching(self):
        """Test classical condition if (c == val) gate;"""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg c[2];
measure q[0] -> c[0];
if (c == 1) x q[1];
if (c == 3) z q[0];
"""
        res1 = self.run_zcc_qasm(qasm)
        self.assertEqual(res1.returncode, 0, f"Conditional branching failed: {res1.stderr}")
        out1 = res1.stdout
        self.assertIn("if (c == 1) x q[1];", out1)
        self.assertIn("if (c == 3) z q[0];", out1)

        res2 = self.run_zcc_qasm(out1)
        self.assertEqual(res2.returncode, 0)
        self.assertEqual(out1, res2.stdout)

    def test_06_3qubit_toffoli_and_fredkin_gates(self):
        """Test 3-qubit gates ccx (Toffoli) and cswap (Fredkin)."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
creg c[3];
ccx q[0], q[1], q[2];
cswap q[0], q[1], q[2];
"""
        res1 = self.run_zcc_qasm(qasm)
        self.assertEqual(res1.returncode, 0, f"3-qubit gates failed: {res1.stderr}")
        out1 = res1.stdout
        self.assertIn("ccx q[0], q[1], q[2];", out1)
        self.assertIn("cswap q[0], q[1], q[2];", out1)

        res2 = self.run_zcc_qasm(out1)
        self.assertEqual(res2.returncode, 0)
        self.assertEqual(out1, res2.stdout)

    # ================================================================
    # NEGATIVE DIAGNOSTIC TESTS (Semantic Validator)
    # ================================================================

    def test_07_neg_duplicate_register_name(self):
        """Reject duplicate register name declarations."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
creg q[2];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("duplicate register declaration 'q'", res.stderr)

    def test_08_neg_undeclared_register(self):
        """Reject access to undeclared register."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h non_existent[0];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("undeclared register 'non_existent'", res.stderr)

    def test_09_neg_out_of_bounds_qubit_index(self):
        """Reject qubit index out of bounds."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
h q[2];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("index 2 out of bounds for quantum register 'q' of size 2", res.stderr)

    def test_10_neg_duplicate_qubits_in_cnot(self):
        """Reject duplicate control and target qubit in 2-qubit gate."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
cx q[0], q[0];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("duplicate qubit operand 'q[0]' in 2-qubit gate 'cx'", res.stderr)

    def test_11_neg_invalid_measurement_target_type(self):
        """Reject measurement targeted at quantum register instead of classical register."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
qreg r[2];
measure q[0] -> r[0];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("target 'r' of measure must be a classical register (creg)", res.stderr)

    def test_12_neg_gate_parameter_count_mismatch(self):
        """Reject gate application with wrong number of parameters."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
u(1.0) q[0];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("gate 'u' requires 3 parameters", res.stderr)

    def test_13_neg_invalid_condition_register(self):
        """Reject 'if' condition using quantum register instead of classical register."""
        qasm = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[2];
if (q == 1) h q[0];
"""
        res = self.run_zcc_qasm(qasm)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("register 'q' in 'if' condition must be classical (creg)", res.stderr)


if __name__ == "__main__":
    unittest.main()
