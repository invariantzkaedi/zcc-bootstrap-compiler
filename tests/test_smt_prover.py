"""
ZCC SMT Invariant Prover Unit Test Suite
Tests SMT-LIB2 formula generation, bit-vector arithmetic assertions, 16-byte stack alignment formal proof generation, and SMT file output syntax.
"""

import os
import subprocess
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def compile_c_smt_harness():
    """Builds a standalone C test harness that exercises src/zcc_smt_prover.c functions."""
    harness_c = os.path.join(REPO_ROOT, "tests", "temp_smt_harness.c")
    bin_out = os.path.join(REPO_ROOT, "tests", "temp_smt_harness")
    
    code = """
#include <stdio.h>
#include "../src/zcc_smt_prover.h"

int main() {
    g_emit_smt_proofs = 1;
    snprintf(g_smt_proofs_dir, sizeof(g_smt_proofs_dir), "/tmp/zcc_smt_test");

    smt_prove_stack_alignment("test_fn", 24, 16, 101);
    smt_prove_push_pop_elision("rax", "rbx", 1, 202);
    
    printf("SMT_HARNESS_DONE:0\\n");
    return 0;
}
"""
    with open(harness_c, "w") as f:
        f.write(code)
        
    cmd = ["gcc", "-Isrc", harness_c, os.path.join(REPO_ROOT, "src", "zcc_smt_prover.c"), "-o", bin_out]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return bin_out, res

class TestSMTProver(unittest.TestCase):
    
    @classmethod
    def setUpClass(cls):
        os.makedirs("/tmp/zcc_smt_test", exist_ok=True)
        cls.bin_out, cls.build_res = compile_c_smt_harness()

    @classmethod
    def tearDownClass(cls):
        harness_c = os.path.join(REPO_ROOT, "tests", "temp_smt_harness.c")
        if os.path.exists(harness_c):
            os.remove(harness_c)
        if os.path.exists(cls.bin_out):
            os.remove(cls.bin_out)

    def test_01_build_harness(self):
        """Verify C harness builds with zero errors."""
        self.assertEqual(self.build_res.returncode, 0, f"Build failed: {self.build_res.stderr}")

    def test_02_smt_proof_generation(self):
        """Executes harness and verifies generated SMT-LIB2 formula files."""
        res = subprocess.run([self.bin_out], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(res.returncode, 0, f"Harness crashed: {res.stderr}")
        self.assertIn("SMT_HARNESS_DONE:0", res.stdout)

        # Inspect generated SMT-LIB2 stack alignment proof file
        proof_file = "/tmp/zcc_smt_test/proof_stack_align_line101.smt2"
        self.assertTrue(os.path.exists(proof_file), f"Proof file missing: {proof_file}")

        
        with open(proof_file, "r") as f:
            smt_content = f.read()

        self.assertIn("(set-logic QF_ABV)", smt_content)

        self.assertIn("(declare-const sp_0 (_ BitVec 64))", smt_content)
        self.assertIn("bvand sp_0 #x000000000000000F", smt_content)
        self.assertIn("(check-sat)", smt_content)

if __name__ == "__main__":
    unittest.main()
