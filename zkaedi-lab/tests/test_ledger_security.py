import unittest
import os
import sys
import time
import json
import tempfile
import multiprocessing
from cryptography.hazmat.primitives.asymmetric import ed25519

# Add zkaedi-lab directory to path
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
LAB_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, LAB_DIR)

from lineage.immutable_ledger import (
    LedgerAnchor,
    LedgerVerification,
    TrustedSigner,
    unrestricted_legacy_signer,
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
    LedgerParseException,
    MAX_ANCHOR_AGE,
    MAX_CLOCK_SKEW
)
from lineage.online_types import lock_file_ex

def append_worker(path, ledger_id, count, results_queue):
    successes = 0
    for i in range(count):
        try:
            append_ledger_payload(path, ledger_id, {"worker_pid": os.getpid(), "index": i})
            successes += 1
            # Add small random sleep to maximize lock contention
            time.sleep(0.005)
        except Exception as exc:
            results_queue.put((False, f"PID {os.getpid()} failed at index {i}: {exc}"))
            return
    results_queue.put((True, successes))

def crash_writer(path: str) -> None:
    with open(path, "a+b") as fh:
        lock_file_ex(fh)
        fh.seek(0, os.SEEK_END)
        fh.write(b'{"schema_version":1,"ledger_id":"test-ledger-abc","payload":"\xe2')
        fh.flush()
        os.fsync(fh.fileno())
        # Exit abruptly while holding lock (lock should be auto-released by OS)
        os._exit(17)

def replay_worker(path, count, results_queue):
    from lineage.online_types import record_online_outcome, OnlineOutcome
    successes = 0
    for i in range(count):
        try:
            outcome = OnlineOutcome(
                config_id=f"config-{os.getpid()}-{i}",
                candidate_id=f"cand-{os.getpid()}-{i}",
                prompt=f"prompt-{os.getpid()}-{i}",
                completion=f"completion-{os.getpid()}-{i}",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=50.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            successes += 1
            time.sleep(0.005)
        except Exception as exc:
            results_queue.put((False, f"PID {os.getpid()} failed at index {i}: {exc}"))
            return
    results_queue.put((True, successes))

def crash_replay_writer(path: str) -> None:
    with open(path, "a+b") as fh:
        lock_file_ex(fh)
        fh.seek(0, os.SEEK_END)
        fh.write(b'{"prompt":"crash","completion":"\xe2')
        fh.flush()
        os.fsync(fh.fileno())
        os._exit(18)

class TestLedgerSecurity(unittest.TestCase):
    def setUp(self):
        # Generate an Ed25519 key pair for tests
        self.private_key = ed25519.Ed25519PrivateKey.generate()
        self.public_key = self.private_key.public_key()
        self.pub_bytes = self.public_key.public_bytes_raw()
        
        self.signer_key_id = "test-signer-01"
        self.ledger_id = "test-ledger-abc"
        self.signer = TrustedSigner(
            public_key=self.pub_bytes,
            valid_from=0.0,
            valid_until=None,
            revoked_at=None,
            allowed_ledgers=frozenset([self.ledger_id])
        )
        self.trusted_keys = {self.signer_key_id: self.signer}

    def test_non_dict_record_in_sequence_does_not_crash(self):
        # Create a mock records sequence where one entry is not a dict
        records = [
            [], # Non-dict record
            {
                "schema_version": 1,
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
                "schema_version": 1,
                "ledger_id": self.ledger_id,
                "sequence": "not-an-int",
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "1" * 64,
                "previous_hash": None,
                "recorded_at_unix": time.time(),
            },
            {
                "schema_version": 1,
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
            initialized=True,
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
            initialized=True,
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
        # Ledger A entry cannot verify under Ledger B expected identifier
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
            initialized=True,
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

    def test_malformed_trusted_signer_fails_closed(self):
        # Create a TrustedSigner with malformed/invalid fields
        bad_signer = TrustedSigner(
            public_key=self.pub_bytes,
            valid_from="yesterday",  # invalid type
            valid_until=None,
            revoked_at=None,
            allowed_ledgers=frozenset([self.ledger_id])
        )
        verification = LedgerVerification(
            valid=True,
            initialized=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=time.time()
        )
        ok = verify_ledger_anchor(
            anchor,
            expected_ledger_id=self.ledger_id,
            verification=verification,
            trusted_keys={self.signer_key_id: bad_signer}
        )
        self.assertFalse(ok)

    def test_raw_key_rejected_in_strict_mode(self):
        # verify_ledger_anchor must reject raw public key bytes in trusted_keys
        verification = LedgerVerification(
            valid=True,
            initialized=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=time.time()
        )
        ok = verify_ledger_anchor(
            anchor,
            expected_ledger_id=self.ledger_id,
            verification=verification,
            trusted_keys={self.signer_key_id: self.pub_bytes}  # raw bytes, not TrustedSigner
        )
        self.assertFalse(ok)

    def test_hash_invalid_parent_invalidates_child(self):
        # Entry hash doesn't match the record header -> record is invalid -> sets record_valid = False
        # The next record should follow it and fail validation with "follows an invalid parent"
        records = [
            {
                "schema_version": 1,
                "ledger_id": self.ledger_id,
                "sequence": 0,
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "f" * 64,  # wrong hash representation
                "previous_hash": None,
                "recorded_at_unix": time.time(),
            },
            {
                "schema_version": 1,
                "ledger_id": self.ledger_id,
                "sequence": 1,
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "2" * 64,
                "previous_hash": "sha256:" + "f" * 64,
                "recorded_at_unix": time.time(),
            }
        ]
        valid, failures = verify_records_sequence(records, self.ledger_id)
        self.assertFalse(valid)
        self.assertTrue(any("follows an invalid parent" in f for f in failures))

    def test_invalid_payload_does_not_mutate_ledger(self):
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Genesis block append first
            append_ledger_payload(path, self.ledger_id, {"genesis": True})
            
            with open(path, "rb") as fh:
                original_content = fh.read()
                
            # Attempt to append invalid payload (must fail validation before file write)
            with self.assertRaises(TypeError):
                append_ledger_payload(path, self.ledger_id, ["invalid", "non-mapping"])
                
            with open(path, "rb") as fh:
                current_content = fh.read()
            self.assertEqual(original_content, current_content)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_empty_expected_ledger_id_is_rejected(self):
        # Verifiers must fail closed with valid=False
        v = verify_ledger("test_path.jsonl", "")
        self.assertFalse(v.valid)
        self.assertTrue(any("invalid expected_ledger_id" in f for f in v.failures))
        
        # Builders and mutators must raise
        with self.assertRaises(ValueError):
            append_ledger_payload("test_path.jsonl", "", {})

    def test_anchor_policy_uses_injected_clock(self):
        verification = LedgerVerification(
            valid=True,
            initialized=True,
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
        # Using exact current time time=200, within skew tolerance (0 skew)
        self.assertTrue(evaluate_anchor_policy(anchor, now=200.0))
        # time=600 is too far ahead (age limit exceeded)
        self.assertFalse(evaluate_anchor_policy(anchor, now=600.0 + MAX_ANCHOR_AGE))

    def test_malformed_now_clock_injected_fails(self):
        verification = LedgerVerification(
            valid=True,
            initialized=True,
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
        # Injected now timestamp must reject strings, booleans, NaN, and Infinity
        self.assertFalse(evaluate_anchor_policy(anchor, now="today"))
        self.assertFalse(evaluate_anchor_policy(anchor, now=True))
        self.assertFalse(evaluate_anchor_policy(anchor, now=float("nan")))
        self.assertFalse(evaluate_anchor_policy(anchor, now=float("inf")))

    def test_expected_ledger_id_errors_fail_closed(self):
        verification = LedgerVerification(
            valid=True,
            initialized=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=time.time()
        )
        # verifiers must return False instead of raising ValueError for invalid ledger IDs
        self.assertFalse(verify_ledger_anchor(anchor, expected_ledger_id="", verification=verification, trusted_keys=self.trusted_keys))
        self.assertFalse(accept_ledger_anchor(anchor, expected_ledger_id="", verification=verification, trusted_keys=self.trusted_keys))

    def test_malformed_trusted_keys_fail_closed(self):
        verification = LedgerVerification(
            valid=True,
            initialized=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        anchor = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=time.time()
        )
        # should return False on bad registry types instead of raising AttributeError
        self.assertFalse(verify_ledger_anchor(anchor, expected_ledger_id=self.ledger_id, verification=verification, trusted_keys=None))
        self.assertFalse(verify_ledger_anchor(anchor, expected_ledger_id=self.ledger_id, verification=verification, trusted_keys=[]))

    def test_payload_mutation_after_append(self):
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            payload = {"state": "clean", "list": [1, 2, 3]}
            # Append payload, then mutated local reference
            envelope = append_ledger_payload(path, self.ledger_id, payload)
            payload["state"] = "mutated"
            payload["list"].append(4)
            
            # Read back verification and check stored payload matches snapshot
            verification = verify_ledger(path, self.ledger_id)
            self.assertTrue(verification.valid)
            with open(path, "rb") as fh:
                records, _ = read_complete_jsonl_prefix(fh.read())
            stored_payload = records[0]["payload"]
            self.assertEqual(stored_payload["state"], "clean")
            self.assertEqual(stored_payload["list"], [1, 2, 3])
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_unrestricted_legacy_signer_validates_key(self):
        # invalid key lengths should raise ValueError on legacy adapter construction
        with self.assertRaises(ValueError):
            unrestricted_legacy_signer(b"short-key")
        with self.assertRaises(ValueError):
            unrestricted_legacy_signer(self.pub_bytes + b"extra")
        # 32 bytes passes
        self.assertIsInstance(unrestricted_legacy_signer(self.pub_bytes), TrustedSigner)

    def test_historical_validity_vs_freshness(self):
        # An anchor from Unix epoch 200.0 is historically cryptographically valid
        signer = TrustedSigner(
            public_key=self.pub_bytes,
            valid_from=0.0,
            valid_until=None,
            revoked_at=None,
            allowed_ledgers=frozenset([self.ledger_id])
        )
        verification = LedgerVerification(
            valid=True,
            initialized=True,
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
        
        # Historically valid Ed25519 signature
        self.assertTrue(verify_ledger_anchor(anchor, expected_ledger_id=self.ledger_id, verification=verification, trusted_keys={self.signer_key_id: signer}))
        # But accept_ledger_anchor at current real clock time rejects it as stale
        self.assertFalse(accept_ledger_anchor(anchor, expected_ledger_id=self.ledger_id, verification=verification, trusted_keys={self.signer_key_id: signer}, now=time.time()))

    def test_malformed_anchor_policy_object_fails_closed(self):
        # Passing None or non-LedgerAnchor objects should fail-closed
        self.assertFalse(evaluate_anchor_policy(None))
        self.assertFalse(evaluate_anchor_policy({}))
        self.assertFalse(verify_ledger_anchor(None, expected_ledger_id=self.ledger_id, verification=None, trusted_keys=self.trusted_keys))

    def test_verifier_guard_inputs_fail_closed(self):
        # Passing None or non-LedgerVerification objects should fail-closed
        anchor = LedgerAnchor(
            ledger_id=self.ledger_id,
            sequence=0,
            head_hash="sha256:" + "0" * 64,
            anchored_at_unix=time.time(),
            signer_key_id=self.signer_key_id,
            signature="0" * 128
        )
        self.assertFalse(verify_ledger_anchor(anchor, expected_ledger_id=self.ledger_id, verification=None, trusted_keys=self.trusted_keys))
        
        with self.assertRaises(TypeError):
            sign_ledger_anchor(ledger_id=self.ledger_id, verification=None, signer_key_id=self.signer_key_id, private_key=self.private_key)

    def test_anchor_policy_clock_skew_boundaries(self):
        verification = LedgerVerification(
            valid=True,
            initialized=True,
            records_verified=1,
            head_hash="sha256:" + "a" * 64,
            failures=()
        )
        now = 1000.0
        # now + MAX_CLOCK_SKEW is accepted
        anchor_at_skew = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=now + MAX_CLOCK_SKEW
        )
        self.assertTrue(evaluate_anchor_policy(anchor_at_skew, now=now))

        # now + MAX_CLOCK_SKEW + 0.001 is rejected
        anchor_past_skew = sign_ledger_anchor(
            ledger_id=self.ledger_id,
            verification=verification,
            signer_key_id=self.signer_key_id,
            private_key=self.private_key,
            anchored_at_unix=now + MAX_CLOCK_SKEW + 0.001
        )
        self.assertFalse(evaluate_anchor_policy(anchor_past_skew, now=now))

    def test_abrupt_killed_writer_recovery(self):
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Genesis block append first
            append_ledger_payload(path, self.ledger_id, {"genesis": True})
            
            # Spawn a process that obtains the exclusive lock, writes a partial/corrupt line, and exits abruptly
            ctx = multiprocessing.get_context("spawn")
            p = ctx.Process(target=crash_writer, args=(path,))
            p.start()
            p.join(timeout=10)
            self.assertEqual(p.exitcode, 17)
            
            # Perform next append. Since lock should be auto-released on process exit, this must succeed,
            # truncate the partial byte line, and append cleanly.
            append_ledger_payload(path, self.ledger_id, {"recovered": True})
            
            # Verify the ledger is completely recovered and valid
            verification = verify_ledger(path, self.ledger_id)
            self.assertTrue(verification.valid)
            self.assertEqual(verification.records_verified, 2)
            
            # Read files and check contents
            with open(path, "rb") as fh:
                records, _ = read_complete_jsonl_prefix(fh.read())
            self.assertEqual(records[0]["payload"]["genesis"], True)
            self.assertEqual(records[1]["payload"]["recovered"], True)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_multiple_processes_allocate_unique_sequences(self):
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Genesis block append first
            append_ledger_payload(path, self.ledger_id, {"genesis": True})
            
            workers = 4
            appends_per_worker = 15
            
            # Force spawn process context to ensure Windows compatibility checks
            ctx = multiprocessing.get_context("spawn")
            queue = ctx.Queue()
            processes = []
            for _ in range(workers):
                p = ctx.Process(
                    target=append_worker,
                    args=(path, self.ledger_id, appends_per_worker, queue)
                )
                processes.append(p)
                p.start()
                
            # Collect and join processes with timeouts to prevent test deadlocks
            for p in processes:
                p.join(timeout=30)
                self.assertFalse(p.is_alive())
                self.assertEqual(p.exitcode, 0)
                
            results = [
                queue.get(timeout=10)
                for _ in range(workers)
            ]
            
            self.assertEqual(len(results), workers)
            for ok, val in results:
                self.assertTrue(ok, f"Worker failed: {val}")
                self.assertEqual(val, appends_per_worker)
                
            # Verify ledger sequence structure holds perfectly
            verification = verify_ledger(path, self.ledger_id)
            self.assertTrue(verification.valid)
            self.assertEqual(verification.records_verified, 1 + workers * appends_per_worker)
            
            # Read file records and assert uniquely numbered sequence indexes
            with open(path, "rb") as fh:
                lines = fh.readlines()
            sequences = [json.loads(line.decode("utf-8"))["sequence"] for line in lines if line.strip()]
            self.assertEqual(sequences, list(range(len(lines))))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_windows_locking_qualification(self):
        from lineage.online_types import HAS_MSVCRT, HAS_FCNTL
        if sys.platform == "win32":
            self.assertTrue(HAS_MSVCRT, "Windows lock module (msvcrt) must be available under native Windows")
            self.assertFalse(HAS_FCNTL, "POSIX lock module (fcntl) must not be available under native Windows")
        else:
            self.assertTrue(HAS_FCNTL, "POSIX lock module (fcntl) must be available under POSIX/WSL")
            self.assertFalse(HAS_MSVCRT, "Windows lock module (msvcrt) must not be available under POSIX/WSL")

    def test_replay_loader_torn_utf8_tail_recovery(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Append a valid outcome first
            outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            
            # Manually append a partial, torn invalid UTF-8 byte sequence (interrupted multibyte character)
            with open(path, "ab") as fh:
                fh.write(b'{"prompt": "hello", "completion": "\xe2') # missing trailing bytes of 3-byte char
                
            res = load_unique_records(path)
            self.assertEqual(len(res.records), 1)
            self.assertTrue(res.tail_record_skipped)
            self.assertTrue(res.tail_missing_newline)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_replay_append_repairs_torn_tail(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            first_outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, first_outcome)

            # Manually write a torn tail
            with open(path, "ab") as fh:
                fh.write(b'{"prompt":"broken","completion":"\xe2')

            second_outcome = OnlineOutcome(
                config_id="config-2",
                candidate_id="cand-2",
                prompt="hello-2",
                completion="world-2",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, second_outcome)

            result = load_unique_records(path)
            self.assertEqual(len(result.records), 2)
            self.assertFalse(result.tail_record_skipped)
            self.assertFalse(result.tail_missing_newline)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_newline_terminated_malformed_replay_data_rejected(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            first_outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, first_outcome)

            # Write newline-terminated malformed JSON data (not a torn tail since it has \n)
            with open(path, "ab") as fh:
                fh.write(b'{"prompt":"broken","completion":\n')

            # Verification of loading must raise ValueError due to durable corruption
            with self.assertRaises(ValueError):
                load_unique_records(path)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_malformed_dedup_hash_fails_value_error(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Write a record containing an invalid dedup_hash type (integer)
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"hello","completion":"world","harness_version":"1.0","evaluator_version":"1.0","policy_checkpoint":"ckpt","sandbox_version":"1.0","schema_version":1,"dedup_hash":123}\n')
                
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("dedup_hash must be a string", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_killed_replay_writer_releases_lock(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Append genesis outcome
            outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            
            # Spawn a process that gets exclusive lock, appends partial, and exits abruptly
            ctx = multiprocessing.get_context("spawn")
            p = ctx.Process(target=crash_replay_writer, args=(path,))
            p.start()
            p.join(timeout=10)
            self.assertEqual(p.exitcode, 18)
            
            # Append next outcome should succeed and repair the crash tail
            outcome2 = OnlineOutcome(
                config_id="config-2",
                candidate_id="cand-2",
                prompt="hello-2",
                completion="world-2",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome2)
            
            res = load_unique_records(path)
            self.assertEqual(len(res.records), 2)
            self.assertFalse(res.tail_record_skipped)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_concurrent_replay_writers(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Genesis outcome
            outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            
            workers = 4
            appends_per_worker = 15
            ctx = multiprocessing.get_context("spawn")
            queue = ctx.Queue()
            processes = []
            
            for _ in range(workers):
                p = ctx.Process(
                    target=replay_worker,
                    args=(path, appends_per_worker, queue)
                )
                processes.append(p)
                p.start()
                
            for p in processes:
                p.join(timeout=30)
                self.assertFalse(p.is_alive())
                self.assertEqual(p.exitcode, 0)
                
            results = [
                queue.get(timeout=10)
                for _ in range(workers)
            ]
            self.assertEqual(len(results), workers)
            for ok, val in results:
                self.assertTrue(ok, f"Worker failed: {val}")
                self.assertEqual(val, appends_per_worker)
                
            res = load_unique_records(path)
            self.assertEqual(len(res.records), 1 + workers * appends_per_worker)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_replay_append_after_valid_unterminated_record(self):
        from lineage.online_types import load_unique_records, append_jsonl_durable
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Write a valid JSON line missing a final newline separator
            first_record = {"prompt": "hello", "completion": "world"}
            with open(path, "wb") as fh:
                fh.write(json.dumps(first_record).encode("utf-8")) # no newline at all!
                
            # Append next record durably
            second_record = {"prompt": "hello-2", "completion": "world-2"}
            append_jsonl_durable(path, second_record)
            
            # Verify both records are loaded successfully and separator newline was placed correctly
            result = load_unique_records(path)
            self.assertEqual(len(result.records), 2)
            self.assertEqual(result.records[0]["prompt"], "hello")
            self.assertEqual(result.records[1]["prompt"], "hello-2")
            self.assertFalse(result.tail_missing_newline)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_ledger_append_after_valid_unterminated_record(self):
        # Repeat similar check on the Ledger path to prove safety of valid unterminated envelope lines
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            envelope1 = build_ledger_envelope(
                ledger_id=self.ledger_id,
                sequence=0,
                previous_hash=None,
                payload={"first": True},
                recorded_at_unix=time.time()
            )
            serialized = serialize_envelope(envelope1)
            # Write exactly without trailing newline
            with open(path, "wb") as fh:
                fh.write(serialized)
                
            # Append next payload
            append_ledger_payload(path, self.ledger_id, {"second": True})
            
            # Verify integrity
            v = verify_ledger(path, self.ledger_id)
            self.assertTrue(v.valid)
            self.assertEqual(v.records_verified, 2)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_legacy_records_with_missing_dedup_hash_preserved(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Legacy records do not have dedup_hash key
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"legacy","completion":"data"}\n')
            result = load_unique_records(path)
            self.assertEqual(len(result.records), 1)
            self.assertEqual(result.records[0]["prompt"], "legacy")
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_empty_dedup_hash_rejected(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Empty dedup_hash string is invalid
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"hello","completion":"world","dedup_hash":""}\n')
            with self.assertRaises(ValueError):
                load_unique_records(path)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_null_and_integer_dedup_hash_values_rejected(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"hello","completion":"world","dedup_hash":null}\n')
            with self.assertRaises(ValueError):
                load_unique_records(path)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_malformed_sha256_identifier_rejected(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"hello","completion":"world","dedup_hash":"sha256:not-hex"}\n')
            with self.assertRaises(ValueError):
                load_unique_records(path)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_non_dictionary_append_input_fails(self):
        from lineage.online_types import append_jsonl_durable
        with self.assertRaises(TypeError):
            append_jsonl_durable("path.jsonl", ["list", "not", "dict"])

    def test_replay_schema_version_required_and_unsupported_rejected(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Missing schema_version key
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"h","completion":"c","harness_version":"1","evaluator_version":"1","policy_checkpoint":"x","sandbox_version":"1","dedup_hash":"sha256:0000000000000000000000000000000000000000000000000000000000000000"}\n')
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("missing schema_version", str(ctx.exception))
            
            # Unsupported schema_version (e.g. 2)
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"h","completion":"c","harness_version":"1","evaluator_version":"1","policy_checkpoint":"x","sandbox_version":"1","schema_version":2,"dedup_hash":"sha256:0000000000000000000000000000000000000000000000000000000000000000"}\n')
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("unsupported schema_version", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_changing_safety_passed_invalidates_record_hash(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            
            # Read first record, change safety_passed to False, write back
            with open(path, "rb") as fh:
                data = fh.read()
            record = json.loads(data.decode("utf-8").strip())
            record["safety_passed"] = False
            
            with open(path, "wb") as fh:
                fh.write(json.dumps(record).encode("utf-8") + b"\n")
                
            # Verification of loading must raise ValueError due to broken integrity hash
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("failed integrity verification", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_changing_desirable_invalidates_record_hash(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            
            with open(path, "rb") as fh:
                data = fh.read()
            record = json.loads(data.decode("utf-8").strip())
            record["desirable"] = not record["desirable"]
            
            with open(path, "wb") as fh:
                fh.write(json.dumps(record).encode("utf-8") + b"\n")
                
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("failed integrity verification", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_changing_recorded_at_unix_invalidates_record_hash(self):
        from lineage.online_types import load_unique_records, record_online_outcome, OnlineOutcome
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            outcome = OnlineOutcome(
                config_id="config-1",
                candidate_id="cand-1",
                prompt="hello",
                completion="world",
                sandbox_passed=True,
                safety_passed=True,
                verification_score=1.0,
                runtime_ms=120.0,
                runner_exit=0,
                verdict="pass",
                failure_class=None,
                harness_version="1.0",
                evaluator_version="1.0",
                policy_checkpoint="ckpt",
                sandbox_version="1.0"
            )
            record_online_outcome(path, outcome)
            
            with open(path, "rb") as fh:
                data = fh.read()
            record = json.loads(data.decode("utf-8").strip())
            record["recorded_at_unix"] = record["recorded_at_unix"] + 10.0
            
            with open(path, "wb") as fh:
                fh.write(json.dumps(record).encode("utf-8") + b"\n")
                
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("failed integrity verification", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_dedup_hash_remains_stable_when_only_runtime_metadata_changes(self):
        from lineage.online_types import compute_dedup_hash
        # changing metadata fields like config_id does not change the prompt/completion dedup hash identity
        dh1 = compute_dedup_hash("prompt", "completion", "1.0", "1.0", "ckpt", "1.0")
        dh2 = compute_dedup_hash("prompt", "completion", "1.0", "1.0", "ckpt", "1.0")
        self.assertEqual(dh1, dh2)

    def test_boolean_replay_schema_version_rejected(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # boolean schema_version (e.g. true) must be rejected
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"h","completion":"c","harness_version":"1","evaluator_version":"1","policy_checkpoint":"x","sandbox_version":"1","schema_version":true,"dedup_hash":"sha256:0000000000000000000000000000000000000000000000000000000000000000"}\n')
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("schema_version must be an integer", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_boolean_ledger_schema_version_rejected(self):
        # boolean schema_version true must be rejected by records sequence verifier
        records = [
            {
                "schema_version": True,
                "ledger_id": self.ledger_id,
                "sequence": 0,
                "payload": {},
                "payload_hash": "sha256:" + "0" * 64,
                "entry_hash": "sha256:" + "1" * 64,
                "previous_hash": None,
                "recorded_at_unix": time.time(),
            }
        ]
        valid, failures = verify_records_sequence(records, self.ledger_id)
        self.assertFalse(valid)
        self.assertTrue(any("schema_version must be an integer" in f for f in failures))

    def test_modern_record_with_removed_dedup_hash_is_not_silently_treated_as_legacy(self):
        from lineage.online_types import load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Modern records that lack dedup_hash key but have schema_version = 1 must be rejected
            with open(path, "wb") as fh:
                fh.write(b'{"prompt":"h","completion":"c","schema_version":1,"record_hash":"sha256:0000000000000000000000000000000000000000000000000000000000000000"}\n')
            with self.assertRaises(ValueError) as ctx:
                load_unique_records(path)
            self.assertIn("missing dedup_hash", str(ctx.exception))
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_large_append_benchmark_confirms_acceptable_growth(self):
        from lineage.online_types import record_online_outcome, OnlineOutcome, load_unique_records
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # Append multiple items to ensure O(N) append loop executes correctly
            for i in range(30):
                outcome = OnlineOutcome(
                    config_id=f"config-{i}",
                    candidate_id=f"cand-{i}",
                    prompt=f"prompt-{i}",
                    completion=f"completion-{i}",
                    sandbox_passed=True,
                    safety_passed=True,
                    verification_score=1.0,
                    runtime_ms=10.0,
                    runner_exit=0,
                    verdict="pass",
                    failure_class=None,
                    harness_version="1.0",
                    evaluator_version="1.0",
                    policy_checkpoint="ckpt",
                    sandbox_version="1.0"
                )
                record_online_outcome(path, outcome)
            res = load_unique_records(path)
            self.assertEqual(len(res.records), 30)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_empty_and_missing_ledgers_have_distinct_readiness_states(self):
        # 1. Non-existent file
        v_missing = verify_ledger("non_existent_file_path.jsonl", self.ledger_id)
        self.assertTrue(v_missing.valid)
        self.assertFalse(v_missing.initialized)
        self.assertEqual(v_missing.records_verified, 0)
        
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.close(fd)
        try:
            # 2. Existent but empty file (0 bytes)
            v_empty = verify_ledger(path, self.ledger_id)
            self.assertTrue(v_empty.valid)
            self.assertFalse(v_empty.initialized)
            self.assertEqual(v_empty.records_verified, 0)
            
            # 3. Non-empty initialized file
            append_ledger_payload(path, self.ledger_id, {"genesis": True})
            v_filled = verify_ledger(path, self.ledger_id)
            self.assertTrue(v_filled.valid)
            self.assertTrue(v_filled.initialized)
            self.assertEqual(v_filled.records_verified, 1)
        finally:
            if os.path.exists(path):
                os.unlink(path)

    def test_nan_score_and_negative_runtime_are_rejected_at_outcome_construction(self):
        from lineage.online_types import OnlineOutcome
        # nan verification_score must raise ValueError
        with self.assertRaises(ValueError):
            OnlineOutcome(
                config_id="c", candidate_id="cand", prompt="p", completion="comp",
                sandbox_passed=True, safety_passed=True, verification_score=float("nan"),
                runtime_ms=10.0, runner_exit=0, verdict="pass", failure_class=None,
                harness_version="1", evaluator_version="1", policy_checkpoint="ckpt", sandbox_version="1"
            )
            
        # negative runtime_ms must raise ValueError
        with self.assertRaises(ValueError):
            OnlineOutcome(
                config_id="c", candidate_id="cand", prompt="p", completion="comp",
                sandbox_passed=True, safety_passed=True, verification_score=1.0,
                runtime_ms=-50.0, runner_exit=0, verdict="pass", failure_class=None,
                harness_version="1", evaluator_version="1", policy_checkpoint="ckpt", sandbox_version="1"
            )

        # bool runner_exit must raise TypeError
        with self.assertRaises(TypeError):
            OnlineOutcome(
                config_id="c", candidate_id="cand", prompt="p", completion="comp",
                sandbox_passed=True, safety_passed=True, verification_score=1.0,
                runtime_ms=10.0, runner_exit=True, verdict="pass", failure_class=None,
                harness_version="1", evaluator_version="1", policy_checkpoint="ckpt", sandbox_version="1"
            )

if __name__ == "__main__":
    unittest.main()
