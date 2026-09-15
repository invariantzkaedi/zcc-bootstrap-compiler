# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // BABYBEAR STARK VERIFIER UNIT TESTS 🔱
=======================================================================================================
 Unit test suite validating:
   1. BabyBear finite field arithmetic (p = 2^31 - 2^27 + 1)
   2. SHA-256 Merkle tree authentication path integrity
   3. Complete STARK attestation & verification loop
   4. Negative controls (tampered execution trace & corrupted Merkle sibling rejection)
=======================================================================================================
"""

import unittest
from tools.zk_compiler_stark_attester import (
    BABYBEAR_P,
    BABYBEAR_G,
    OP_ADD,
    OP_MUL,
    BabyBearField,
    StarkMerkleTree,
    ZkCompilerStarkAttester,
    verify_stark_receipt
)

class TestZkCompilerStark(unittest.TestCase):
    def test_01_babybear_field_properties(self):
        """Verify BabyBear prime and field arithmetic properties."""
        p = (1 << 31) - (1 << 27) + 1
        self.assertEqual(BABYBEAR_P, p)
        # Fermat's Little Theorem check
        for base in [2, 3, 5, 7]:
            self.assertEqual(pow(base, BABYBEAR_P - 1, BABYBEAR_P), 1)

        # Inversion test
        x = 123456789
        x_inv = BabyBearField.inv(x)
        self.assertEqual(BabyBearField.mul(x, x_inv), 1)

    def test_02_merkle_tree_authenticity(self):
        """Verify binary Merkle tree root and path proofs."""
        leaves = [f"leaf_test_{i}".encode("utf-8") for i in range(16)]
        tree = StarkMerkleTree(leaves)
        root = tree.get_root()
        self.assertTrue(len(root) == 64)

        for i in range(16):
            proof = tree.get_proof(i)
            self.assertTrue(StarkMerkleTree.verify_proof(leaves[i], proof, root))

    def test_03_stark_proof_attestation_loop(self):
        """Verify complete proof generation and cryptographic verification."""
        attester = ZkCompilerStarkAttester(trace_size=256)
        source = "int compute() { return 42 * 1337; }"
        receipt = attester.generate_proof(source, num_queries=16)

        self.assertTrue(receipt["constraints_valid"])
        self.assertEqual(receipt["constraint_violations"], 0)
        self.assertEqual(receipt["num_queries"], 16)

        valid, msg = verify_stark_receipt(receipt)
        self.assertTrue(valid, msg)

    def test_04_negative_control_tampered_trace(self):
        """Negative control: mutating an execution trace value must be rejected."""
        attester = ZkCompilerStarkAttester(trace_size=128)
        source = "void fn() {}"
        receipt = attester.generate_proof(source, num_queries=8)

        # Corrupt first query's raw leaf bytes
        q0 = receipt["queries"][0]
        tampered_leaf = bytearray.fromhex(q0["raw_leaf_hex"])
        tampered_leaf[0] ^= 0xFF
        q0["raw_leaf_hex"] = tampered_leaf.hex()

        valid, msg = verify_stark_receipt(receipt)
        self.assertFalse(valid, "Tampered leaf must fail Merkle verification")
        self.assertIn("Merkle path verification failed", msg)

    def test_05_negative_control_constraint_violation(self):
        """Negative control: invalid transition polynomial must fail verification."""
        attester = ZkCompilerStarkAttester(trace_size=64)
        source = "int x = 1;"
        receipt = attester.generate_proof(source, num_queries=8)

        # Force invalid arithmetic in a query row while preserving Merkle leaf
        q = receipt["queries"][0]
        q["row"]["opcode"] = OP_ADD
        q["row"]["val_dst"] = 999999
        q["row"]["val_src1"] = 10
        q["row"]["val_src2"] = 20

        # Note: If raw_leaf doesn't match, Merkle check catches it first;
        # even if Merkle was bypassed, constraint check catches arithmetic tampering
        valid, msg = verify_stark_receipt(receipt)
        self.assertFalse(valid)

if __name__ == "__main__":
    unittest.main()
