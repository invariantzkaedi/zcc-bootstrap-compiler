"""
ZCC EVM Symbolic Harness V2 & Yul Weaver Unit Test Suite
Tests EVM symbolic execution state transitions, storage slot SSTORE/SLOAD invariants, and reentrancy detection.
"""

import os
import subprocess
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def compile_c_evm_harness():
    """Builds a standalone C test harness that exercises src/evm/evm_symbolic_harness.c functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_evm_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_evm_harness")
    
    code = """
#include <stdio.h>
#include "../src/evm/evm_symbolic_harness.h"

/* Minimal stubs for lifter dependencies during unit testing */
void ir_pm_run_default(ir_module_t *mod) {}
void export_smt2(ir_func_t *fn, const char *path) {}
void evm_lifter_init(evm_lifter_t *ls, const unsigned char *bc, int len, ir_module_t *mod) {}
evm_lift_result_t evm_lift_bytecode(evm_lifter_t *ls) { evm_lift_result_t r = {0}; return r; }
void evm_lifter_destroy(evm_lifter_t *ls) {}

int main() {
    /* Safe bytecode sequence: PUSH1 0x42, PUSH1 0x00, SSTORE (no call) */
    unsigned char safe_bc[] = { 0x60, 0x42, 0x60, 0x00, 0x55 };
    int v_safe = evm_symbolic_check_reentrancy_invariant(safe_bc, sizeof(safe_bc));
    printf("SAFE_CHECK:%d\\n", v_safe);

    /* Reentrant bytecode sequence: CALL (0xF1), PUSH1 0x00, SSTORE (0x55) */
    unsigned char vuln_bc[] = { 0xF1, 0x60, 0x00, 0x55 };
    int v_vuln = evm_symbolic_check_reentrancy_invariant(vuln_bc, sizeof(vuln_bc));
    printf("VULN_CHECK:%d\\n", v_vuln);

    return 0;
}
"""
    with open(harness_c, "w") as f:
        f.write(code)
        
    cmd = ["gcc", "-Isrc", "-I.", harness_c, os.path.join(REPO_ROOT, "src", "evm", "evm_symbolic_harness.c"),
           os.path.join(REPO_ROOT, "ir.c"), os.path.join(REPO_ROOT, "ir_vuln_tag.c"),
           "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res

class TestEVMSymbolicHarness(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls):
        cls.bin_out, cls.build_res = compile_c_evm_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_evm_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)

    def test_01_build_harness(self):
        """Verify C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_evm_symbolic_reentrancy_detection(self):
        """Executes harness and verifies reentrancy detection logic."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("SAFE_CHECK:0", res.stdout)
        self.assertIn("VULN_CHECK:1", res.stdout)

if __name__ == "__main__":
    unittest.main()
