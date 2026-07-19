import unittest
import os
import sys
import time
import math
import json
from cryptography.hazmat.primitives.asymmetric import ed25519

# Add zkaedi-lab directory to path
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
LAB_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, LAB_DIR)

from lineage.immutable_ledger import (
    LedgerEnvelope,
    LedgerAnchor,
    LedgerVerification,
    TrustedSigner,
    build_ledger_envelope,
    serialize_envelope,
    verify_records_sequence,
    read_complete_jsonl_prefix,
    verify_ledger,
    append_ledger_payload,
    sign_ledger_anchor,
    evaluate_anchor_policy,
    verify_ledger_anchor,
    accept_ledger_anchor,
    LedgerParseException
)

class TestLedgerSecurity(unittest.TestCase):
    def setUp(self):
        # Generate an Ed25519 key pair for tests
        self.private_key = ed25519.Ed25519PrivateKey.generate()
        self.public_key = self.private_key.public_key()
        self.pub_bytes = self.public_key.public_bytes_raw()
        
        self.signer_key_id = "test-signer-01"
        self.ledger_id = "test-ledger-abc"
        self.trusted_keys = {self.signer_key_id: self.pub_bytes}

    def test_non_dict_record_in_sequence_does_not_crash(self):
        # Create a mock records sequence where one entry is not a dict
        records = [
            [], # Non-dict record
            {
                "ledger_id": self.ledger_id,
                "sequence": 1,
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "1" * 64,
                "previous_hash": "sha256:" + "0" * 64,
                "recorded_at_unix": time.time(),
            }
        ]
        
        valid, failures = verify_records_sequence(records, self.ledger_id)
        self.assertFalse(valid)
        self.assertIn("Record index 0 is not a JSON object", failures)
        self.assertIn("Record sequence 1 follows an invalid parent", failures)

    def test_malformed_previous_record_produces_invalid_parent(self):
        # Sequence index 0 is invalid dict sequence type, index 1 is valid but points to it
        records = [
            {
                "ledger_id": self.ledger_id,
                "sequence": "not-an-int",
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "1" * 64,
                "previous_hash": None,
                "recorded_at_unix": time.time(),
            },
            {
                "ledger_id": self.ledger_id,
                "sequence": 1,
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "2" * 64,
                "previous_hash": "sha256:" + "1" * 64,
                "recorded_at_unix": time.time(),
            }
        ]
        valid, failures = verify_records_sequence(records, self.ledger_id)
        self.assertFalse(valid)
        self.assertIn("Record index 0 sequence field is not an integer", failures)
        self.assertIn("Record sequence 1 follows an invalid parent", failures)

    def test_read_complete_jsonl_prefix_rejects_non_objects(self):
        non_obj_data = b'{"sequence": 0}\n[]\n{"sequence": 1}\n'
        with self.assertRaises(LedgerParseException) as context:
            read_complete_jsonl_prefix(non_obj_data)
        self.assertIn("must be a JSON object", str(context.exception))
        self.assertEqual(context.exception.records_verified, 1)

    def test_standalone_anchor_policy_evaluation_rejects_malformed_timestamps(self):
        # Bad timestamp in evaluate_anchor_policy should fail-closed instead of raising
        anchor = LedgerAnchor(
            ledger_id=self.ledger_id,
            sequence=0,
            head_hash="sha256:" + "0" * 64,
            anchored_at_unix="not-a-timestamp",
            signer_key_id=self.signer_key_id,
            signature="0" * 128
        )
        ok = evaluate_anchor_policy(anchor)
        self.assertFalse(ok)

    def test_signer_revoked_before_anchor_timestamp_is_rejected(self):
        # Signer was revoked at t=100, anchor is created at t=200
        signer = TrustedSigner(
            public_key=self.pub_bytes,
            valid_from=0.0,
            valid_until=None,
            revoked_at=100.0,
            allowed_ledgers=frozenset([self.ledger_id])
        )
        
        verification = LedgerVerification(
            valid=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        
        anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=200.0
        )
        
        # Verify rejects because revocation time is in the past relative to the anchor
        ok = verify_ledger_anchor(
            anchor,
            expected_ledger_id=self.ledger_id,
            verification=verification,
            trusted_keys={self.signer_key_id: signer}
        )
        self.assertFalse(ok)

    def test_signer_revoked_after_anchor_timestamp_is_accepted(self):
        # Signer is revoked at t=300, anchor was created in the past at t=200
        signer = TrustedSigner(
            public_key=self.pub_bytes,
            valid_from=0.0,
            valid_until=None,
            revoked_at=300.0,
            allowed_ledgers=frozenset([self.ledger_id])
        )
        
        verification = LedgerVerification(
            valid=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        
        anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=200.0
        )
        
        # Verify accepts because revocation time is in the future relative to the anchor
        ok = verify_ledger_anchor(
            anchor,
            expected_ledger_id=self.ledger_id,
            verification=verification,
            trusted_keys={self.signer_key_id: signer}
        )
        self.assertTrue(ok)

    def test_chain_from_ledger_a_cannot_be_accepted_as_ledger_b(self):
        # Ledger A entry cannot verify under Ledger Bexpected identifier
        envelope = build_ledger_envelope(
            ledger_id="ledger-a",
            sequence=0,
            previous_hash=None,
            payload={"test": "data"},
            recorded_at_unix=time.time()
        )
        
        # Verifying with expected_ledger_id="ledger-b" must fail
        records = [json.loads(serialize_envelope(envelope).decode("utf-8"))]
        valid, failures = verify_records_sequence(records, "ledger-b")
        self.assertFalse(valid)
        self.assertTrue(any("ledger_id mismatch" in f for f in failures))

    def test_combined_anchor_acceptance_enforces_freshness(self):
        verification = LedgerVerification(
            valid=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        
        # Anchor in the future beyond max skew
        future_anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=time.time() + 1000.0
        )
        
        ok = accept_ledger_anchor(
            future_anchor,
            expected_ledger_id=self.ledger_id,
            verification=verification,
            trusted_keys=self.trusted_keys
        )
        self.assertFalse(ok)

    def test_builder_invariants(self):
        # Negative sequence check
        with self.assertRaises(ValueError):
            build_ledger_envelope(self.ledger_id, -1, None, {}, time.time())
            
        # Boolean sequence check
        with self.assertRaises(TypeError):
            build_ledger_envelope(self.ledger_id, True, None, {}, time.time())

        # Non-finite timestamp check
        with self.assertRaises(ValueError):
            build_ledger_envelope(self.ledger_id, 0, None, {}, float('nan'))

        # Non-genesis previous_hash check
        with self.assertRaises(ValueError):
            build_ledger_envelope(self.ledger_id, 1, None, {}, time.time())

if __name__ == "__main__":
    unittest.main()
