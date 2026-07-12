import os
import sys
import time
import socket
import json
import shutil
import tempfile
import unittest
import unittest.mock
from pathlib import Path

# Ensure import path points to workspace root
sys.path.append(str(Path(__file__).resolve().parents[1]))

import zkaedi_model_registry as reg


class TestModelRegistry(unittest.TestCase):
    def setUp(self):
        # We need mock safe bases since temporary dir might be outside /mnt/h on standard systems.
        # Let's verify what the current safe base is or override env ZKAEDI_SAFE_BASE.
        self.test_dir = tempfile.mkdtemp(prefix="zkaedi_reg_test_")
        self.old_env = os.environ.get("ZKAEDI_SAFE_BASE")
        # Enforce the test temp dir as our safe base for validation tests
        os.environ["ZKAEDI_SAFE_BASE"] = self.test_dir
        
        # Override registry location inside the temp dir to avoid modifying real data
        self.mock_registry_file = Path(self.test_dir) / "zkaedi_model_registry.json"
        self._old_get_registry_path = reg.get_registry_path
        reg.get_registry_path = lambda: self.mock_registry_file

    def tearDown(self):
        reg.get_registry_path = self._old_get_registry_path
        if self.old_env is not None:
            os.environ["ZKAEDI_SAFE_BASE"] = self.old_env
        else:
            os.environ.pop("ZKAEDI_SAFE_BASE", None)
            
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)

    def test_registration_and_allow_list(self):
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "mock_model"
        model_dir.mkdir()
        
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        config_file = model_dir / "config.json"
        with open(config_file, "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Register model
        entry = reg.register_model(
            model_name="mock-model-v1",
            model_path=str(model_dir),
            author="Anunnaki-Security",
            description="Secure baseline test model"
        )
        
        self.assertEqual(entry["name"], "mock-model-v1")
        self.assertEqual(entry["author"], "Anunnaki-Security")
        self.assertIn("model.safetensors", entry["files"])
        self.assertIn("config.json", entry["files"])
        
        # Verify allow-list lookup
        self.assertTrue(reg.is_model_allowed("mock-model-v1"))
        self.assertTrue(reg.is_model_allowed(entry["combined_sha256"]))
        self.assertFalse(reg.is_model_allowed("unregistered-model"))
        
        # Verify integrity checks
        valid, errors = reg.verify_model_integrity(str(model_dir))
        self.assertTrue(valid)
        self.assertEqual(len(errors), 0)
        
        # Test file modification (tampering)
        with open(weights_file, "w") as f:
            f.write("malicious payload")
            
        valid, errors = reg.verify_model_integrity(str(model_dir))
        self.assertFalse(valid)
        self.assertTrue(any("Hash mismatch" in err for err in errors))
        
        # Restore weights
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        # Test missing file
        config_file.unlink()
        valid, errors = reg.verify_model_integrity(str(model_dir))
        self.assertFalse(valid)
        self.assertTrue(any("Missing file" in err for err in errors))
        
        # Recreate config
        with open(config_file, "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Test untracked file detection
        untracked = model_dir / "untracked_script.py"
        with open(untracked, "w") as f:
            f.write("exec(payload)")
            
        valid, errors = reg.verify_model_integrity(str(model_dir))
        self.assertFalse(valid)
        self.assertTrue(any("Untracked file found" in err for err in errors))
        
        # Deregister model
        self.assertTrue(reg.deregister_model("mock-model-v1"))
        self.assertFalse(reg.is_model_allowed("mock-model-v1"))

    def test_path_traversal_protection(self):
        # Path outside ZKAEDI_SAFE_BASE should raise ValueError
        outside_dir = tempfile.gettempdir()
        with self.assertRaises(ValueError):
            reg.register_model("dangerous", outside_dir)

    @unittest.mock.patch("zkaedi_security_utils.AutoModelForCausalLM")
    @unittest.mock.patch("zkaedi_security_utils.AutoTokenizer")
    def test_enforce_allow_list_integration(self, mock_tokenizer, mock_model):
        from zkaedi_security_utils import load_model_hardened
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "mock_model"
        model_dir.mkdir()
        
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        config_file = model_dir / "config.json"
        with open(config_file, "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Try loading unregistered model with enforce_allow_list=True -> should fail
        with self.assertRaises(ValueError) as ctx:
            load_model_hardened(str(model_dir), enforce_allow_list=True, allow_unsigned_registry=True)
        self.assertIn("No registry entry found", str(ctx.exception))
        
        # Register model
        reg.register_model(
            model_name="mock-model-v1",
            model_path=str(model_dir),
            author="Anunnaki-Security",
            description="Secure baseline test model"
        )
        
        # Load registered model with enforce_allow_list=True -> should succeed (mocked load)
        load_model_hardened(str(model_dir), enforce_allow_list=True, allow_unsigned_registry=True)
        
        # Try loading unregistered model name -> should fail
        with self.assertRaises(ValueError) as ctx:
            load_model_hardened("unregistered-name", enforce_allow_list=True, allow_unsigned_registry=True)
        self.assertIn("is not in the allow-list", str(ctx.exception))

    def test_signature_verification(self):
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        
        # Generate keypair
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        self.assertTrue(os.path.exists(priv_key_path))
        self.assertTrue(os.path.exists(pub_key_path))
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "mock_model"
        model_dir.mkdir(exist_ok=True)
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        # Register model with signature
        reg.register_model(
            model_name="signed-model",
            model_path=str(model_dir),
            sign=True,
            private_key_path=priv_key_path
        )
        
        # Verify loading with signature check
        registry = reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
        self.assertIn("signed-model", registry["models"])
        
        # Modify the signature file (tampering)
        db_dir = reg._get_db_dir()
        gen_id = (db_dir / "current").read_text(encoding="utf-8").strip()
        sig_path = db_dir / "generations" / f"{gen_id}.sig"
        with open(sig_path, "wb") as f:
            f.write(b"invalid signature bytes")
            
        with self.assertRaises(ValueError) as ctx:
            reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
        self.assertIn("Registry signature verification failed", str(ctx.exception))

    def test_cli_commands(self):
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        
        # Keygen via CLI main
        with unittest.mock.patch("sys.argv", ["zkaedi_model_registry.py", "keygen", "--private-key", priv_key_path, "--public-key", pub_key_path]):
            reg.main()
        self.assertTrue(os.path.exists(priv_key_path))
        
        # Create mock model
        model_dir = Path(self.test_dir) / "mock_model"
        model_dir.mkdir(exist_ok=True)
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        # Register via CLI
        with unittest.mock.patch("sys.argv", ["zkaedi_model_registry.py", "register", "--name", "cli-model", "--path", str(model_dir), "--sign", "--private-key", priv_key_path]):
            reg.main()
            
        # Verify via CLI
        with unittest.mock.patch("sys.argv", ["zkaedi_model_registry.py", "verify", "--path", str(model_dir), "--verify-sig", "--public-key", pub_key_path]):
            reg.main()

    def test_encrypted_private_key(self):
        priv_key_path = os.path.join(self.test_dir, "enc_private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "enc_public_key.pem")
        password = "secure_password"
        
        # 1. Generate encrypted keypair
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path, password=password)
        self.assertTrue(os.path.exists(priv_key_path))
        self.assertTrue(os.path.exists(pub_key_path))
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "mock_model_enc"
        model_dir.mkdir(exist_ok=True)
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        # 2. Register model with signed encrypted key (needs correct password)
        reg.register_model(
            model_name="enc-model",
            model_path=str(model_dir),
            sign=True,
            private_key_path=priv_key_path,
            password=password
        )
        
        # Verify loading with signature check
        registry = reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
        self.assertIn("enc-model", registry["models"])
        
        # 3. Trying to register/sign with wrong password should fail decryption
        with self.assertRaises(Exception):
            reg.register_model(
                model_name="another-model",
                model_path=str(model_dir),
                sign=True,
                private_key_path=priv_key_path,
                password="wrong_password"
            )

    @unittest.mock.patch("zkaedi_security_utils.AutoModelForCausalLM")
    @unittest.mock.patch("zkaedi_security_utils.AutoTokenizer")
    def test_quantize_autoregistration(self, mock_tokenizer, mock_model):
        from zkaedi_security_utils import safe_quantize_model
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "mock_model_for_quant"
        model_dir.mkdir(exist_ok=True)
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
        config_file = model_dir / "config.json"
        with open(config_file, "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Mock bitsandbytes modules
        import sys
        from unittest.mock import MagicMock
        sys.modules['bitsandbytes'] = MagicMock()
        sys.modules['bitsandbytes.nn'] = MagicMock()

        output_dir = Path(self.test_dir) / "quantized_model_out"
        
        # Call safe_quantize_model with register=True
        safe_quantize_model(
            model_path=str(model_dir),
            output_dir=str(output_dir),
            bits=8,
            device="cpu",
            register=True,
            model_name="auto-registered-quantized"
        )
        
        # Verify it was added to registry allow-list
        self.assertTrue(reg.is_model_allowed("auto-registered-quantized"))

    @unittest.mock.patch("zkaedi_security_utils.AutoModelForCausalLM")
    @unittest.mock.patch("zkaedi_security_utils.AutoTokenizer")
    def test_unregistered_model_enforce_allow_list(self, mock_tokenizer, mock_model):
        from zkaedi_security_utils import load_model_hardened
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "unregistered_consistent_model"
        model_dir.mkdir(exist_ok=True)
        weights_file = model_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
        config_file = model_dir / "config.json"
        with open(config_file, "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Since it is NOT in the registry, verify that load_model_hardened with enforce_allow_list=True fails
        with self.assertRaises(ValueError) as ctx:
            load_model_hardened(str(model_dir), enforce_allow_list=True, allow_unsigned_registry=True)
        self.assertIn("no-registry-entry", str(ctx.exception))
        
        # Register the model to the allow-list registry
        reg.register_model(
            model_name="registered-consistent-model",
            model_path=str(model_dir),
            author="Anunnaki-Security",
            description="Signed testing model"
        )
        
        # Verify it now passes verification and successfully loads (meaning it calls from_pretrained)
        load_model_hardened(str(model_dir), enforce_allow_list=True, allow_unsigned_registry=True)
        mock_model.from_pretrained.assert_called_once()
        mock_tokenizer.from_pretrained.assert_called_once()

    def test_lock_timeout_behavior(self):
        # Acquire lock once manually
        lock_path = Path(self.test_dir) / "test.lock"
        lock1 = reg.RegistryLock(lock_path)
        lock1.__enter__()
        
        # Second lock attempt should timeout
        lock2 = reg.RegistryLock(lock_path, timeout=0.1)
        with self.assertRaises(TimeoutError):
            lock2.__enter__()
            
        lock1.__exit__(None, None, None)

    def test_stale_lock_recovery_dead_pid(self):
        import json
        lock_path = Path(self.test_dir) / "stale.lock"
        # Write stale lock data with a demonstrably dead PID on the same host
        stale_data = {
            "pid": 999999, # Dead PID
            "hostname": socket.gethostname(),
            "owner_token": "deadtoken",
            "acquired_at": time.time(),
            "lease_duration": 15.0
        }
        with open(lock_path, "w", encoding="utf-8") as f:
            json.dump(stale_data, f)
            
        # Reclaiming/acquiring should succeed without TimeoutError by breaking stale lock
        lock = reg.RegistryLock(lock_path, timeout=0.5)
        lock.__enter__()
        self.assertTrue(lock.acquired)
        lock.__exit__(None, None, None)

    def test_stale_lock_recovery_expired_lease(self):
        import json
        lock_path = Path(self.test_dir) / "expired.lock"
        # Write lock data that has expired lease duration
        expired_data = {
            "pid": os.getpid(),
            "hostname": "other-host",
            "owner_token": "expiredtoken",
            "acquired_at": time.time() - 30.0, # 30 seconds ago
            "lease_duration": 15.0
        }
        with open(lock_path, "w", encoding="utf-8") as f:
            json.dump(expired_data, f)
            
        # Reclaiming/acquiring should succeed without TimeoutError
        lock = reg.RegistryLock(lock_path, timeout=0.5)
        lock.__enter__()
        self.assertTrue(lock.acquired)
        lock.__exit__(None, None, None)

    def test_lock_ownership_token_safety(self):
        import json
        lock_path = Path(self.test_dir) / "token.lock"
        lock1 = reg.RegistryLock(lock_path)
        lock1.__enter__()
        
        # Manually alter the owner_token in the lock file to simulate another process taking it
        with open(lock_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        data["owner_token"] = "different_token"
        with open(lock_path, "w", encoding="utf-8") as f:
            json.dump(data, f)
            
        # Exiting lock1 should NOT delete the lock file because token mismatch
        lock1.__exit__(None, None, None)
        self.assertTrue(lock_path.exists())
        
        # Clean up
        lock_path.unlink(missing_ok=True)

    def test_crash_atomic_recovery_fallback(self):
        # Verify that if a new generation JSON is written but current pointer is NOT updated (simulating crash),
        # readers correctly read the previous valid generation.
        db_dir = reg._get_db_dir()
        db_dir.mkdir(parents=True, exist_ok=True)
        
        # Setup initial generation 1
        data1 = {"models": {"model1": {}}, "generation": 1}
        reg._save_registry_unlocked(data1)
        
        # Stage generation 2 manually inside generations dir, but do not update "current" pointer
        generations_dir = db_dir / "generations"
        with open(generations_dir / "2.json", "w") as f:
            json.dump({"models": {"model2": {}}, "generation": 2}, f)
            
        # Load registry should still read generation 1 (fallback)
        loaded = reg.load_registry()
        self.assertEqual(loaded["generation"], 1)
        self.assertIn("model1", loaded["models"])
        self.assertNotIn("model2", loaded["models"])

    def test_concurrency_lost_updates(self):
        # Simulate two concurrent threads/processes modifying the registry simultaneously
        import threading
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "concurrent_model"
        model_dir.mkdir(exist_ok=True)
        with open(model_dir / "model.safetensors", "w") as f:
            f.write("weights")
            
        def worker(worker_id):
            reg.register_model(
                model_name=f"worker-model-{worker_id}",
                model_path=str(model_dir),
                author=f"Worker {worker_id}"
            )
            
        # Start concurrent threads to write simultaneously
        threads = []
        for i in range(5):
            t = threading.Thread(target=worker, args=(i,))
            threads.append(t)
            t.start()
            
        for t in threads:
            t.join()
            
        # Verify that all 5 models exist in the registry (no lost updates!)
        loaded = reg.load_registry()
        for i in range(5):
            self.assertIn(f"worker-model-{i}", loaded["models"])

    @unittest.mock.patch("zkaedi_security_utils.AutoModelForCausalLM")
    @unittest.mock.patch("zkaedi_security_utils.AutoTokenizer")
    def test_enforcement_rejects_unsigned_registry(self, mock_tokenizer, mock_model):
        from zkaedi_security_utils import load_model_hardened
        
        # Create public/private key paths
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        # Create a mock model directory
        model_dir = Path(self.test_dir) / "test_unsigned_model"
        model_dir.mkdir(exist_ok=True)
        with open(model_dir / "model.safetensors", "w") as f:
            f.write("weights")
        with open(model_dir / "config.json", "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Register WITHOUT signing
        reg.register_model(
            model_name="unsigned-model",
            model_path=str(model_dir),
            sign=False
        )
        
        # Call load_model_hardened with enforce_allow_list=True and verify it raises no-signature-present error
        with self.assertRaises(ValueError) as ctx:
            load_model_hardened(
                str(model_dir),
                enforce_allow_list=True,
                public_key_path=pub_key_path,
                allow_unsigned_registry=False
            )
        self.assertIn("no-signature-present", str(ctx.exception))

    @unittest.mock.patch("zkaedi_security_utils.AutoModelForCausalLM")
    @unittest.mock.patch("zkaedi_security_utils.AutoTokenizer")
    def test_enforcement_rejects_tampered_registry(self, mock_tokenizer, mock_model):
        from zkaedi_security_utils import load_model_hardened
        import json
        
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        model_dir = Path(self.test_dir) / "test_tampered_model"
        model_dir.mkdir(exist_ok=True)
        with open(model_dir / "model.safetensors", "w") as f:
            f.write("weights")
        with open(model_dir / "config.json", "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Register WITH signing
        reg.register_model(
            model_name="tampered-model",
            model_path=str(model_dir),
            sign=True,
            private_key_path=priv_key_path
        )
        
        # Now tamper the JSON file directly (leave .sig stale)
        db_dir = reg._get_db_dir()
        current_file = db_dir / "current"
        gen_id_str = current_file.read_text(encoding="utf-8").strip()
        gen_json_path = db_dir / "generations" / f"{gen_id_str}.json"
        
        with open(gen_json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        data["models"]["malicious-entry"] = {"name": "malicious"}
        with open(gen_json_path, "w", encoding="utf-8") as f:
            json.dump(data, f)
            
        # Verify it raises signature-invalid error
        with self.assertRaises(ValueError) as ctx:
            load_model_hardened(
                str(model_dir),
                enforce_allow_list=True,
                public_key_path=pub_key_path,
                allow_unsigned_registry=False
            )
        self.assertIn("signature-invalid", str(ctx.exception))

    def test_signed_write_refuses_tampered_state(self):
        import json
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        model_dir = Path(self.test_dir) / "test_launder_model"
        model_dir.mkdir(exist_ok=True)
        with open(model_dir / "model.safetensors", "w") as f:
            f.write("weights")
            
        # Register WITH signing
        reg.register_model(
            model_name="launder-model",
            model_path=str(model_dir),
            sign=True,
            private_key_path=priv_key_path
        )
        
        # Tamper the JSON file directly (leave .sig stale)
        db_dir = reg._get_db_dir()
        current_file = db_dir / "current"
        gen_id_str = current_file.read_text(encoding="utf-8").strip()
        gen_json_path = db_dir / "generations" / f"{gen_id_str}.json"
        
        with open(gen_json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        data["models"]["malicious-entry"] = {"name": "malicious"}
        with open(gen_json_path, "w", encoding="utf-8") as f:
            json.dump(data, f)
            
        # Attempt another register_model with sign=True -> must refuse to launder
        with self.assertRaises(ValueError) as ctx:
            reg.register_model(
                model_name="new-model",
                model_path=str(model_dir),
                sign=True,
                private_key_path=priv_key_path
            )
        self.assertIn("Refusing to sign over unverified registry state", str(ctx.exception))

    @unittest.mock.patch("zkaedi_security_utils.AutoModelForCausalLM")
    @unittest.mock.patch("zkaedi_security_utils.AutoTokenizer")
    def test_enforcement_passes_valid_signed_registry(self, mock_tokenizer, mock_model):
        from zkaedi_security_utils import load_model_hardened
        
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        model_dir = Path(self.test_dir) / "test_valid_signed_model"
        model_dir.mkdir(exist_ok=True)
        with open(model_dir / "model.safetensors", "w") as f:
            f.write("weights")
        with open(model_dir / "config.json", "w") as f:
            f.write('{"vocab_size": 32000}')
            
        # Register WITH signing
        reg.register_model(
            model_name="valid-signed-model",
            model_path=str(model_dir),
            sign=True,
            private_key_path=priv_key_path
        )
        
        # Verify it succeeds
        load_model_hardened(
            str(model_dir),
            enforce_allow_list=True,
            public_key_path=pub_key_path,
            allow_unsigned_registry=False
        )
        mock_model.from_pretrained.assert_called_once()
        mock_model.from_pretrained.assert_called_once()
        mock_tokenizer.from_pretrained.assert_called_once()

    def test_verified_migration_signed_legacy(self):
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        legacy_data = {"models": {"legacy-model": {"name": "legacy-model"}}, "generation": 0}
        
        # Write legacy flat json database
        with open(self.mock_registry_file, "w", encoding="utf-8") as f:
            json.dump(legacy_data, f)
            
        # Write legacy signature
        legacy_sig = reg._get_signature_path(self.mock_registry_file)
        signature = reg.sign_registry(legacy_data, priv_key_path)
        with open(legacy_sig, "wb") as f:
            f.write(signature)
            
        # Load registry with verify_signature=True
        loaded = reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
        self.assertIn("legacy-model", loaded["models"])
        self.assertEqual(loaded["generation"], 0)
        
        # Verify legacy files were cleaned up, and generations structure exists
        self.assertFalse(self.mock_registry_file.exists())
        self.assertFalse(legacy_sig.exists())
        
        db_dir = reg._get_db_dir()
        current_file = db_dir / "current"
        self.assertTrue(current_file.exists())
        self.assertEqual(current_file.read_text(encoding="utf-8").strip(), "0")
        
        gen_json = db_dir / "generations" / "0.json"
        gen_sig = db_dir / "generations" / "0.sig"
        self.assertTrue(gen_json.exists())
        self.assertTrue(gen_sig.exists())

    def test_unsigned_legacy_migration_rejection(self):
        # 1. No signature file present
        legacy_data = {"models": {"legacy-model": {"name": "legacy-model"}}, "generation": 0}
        with open(self.mock_registry_file, "w", encoding="utf-8") as f:
            json.dump(legacy_data, f)
            
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        # Generate arbitrary keys just for verification call
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        with self.assertRaises(Exception):
            reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
            
        # 2. Tampered signature file present
        legacy_sig = reg._get_signature_path(self.mock_registry_file)
        with open(legacy_sig, "wb") as f:
            f.write(b"bad signature bytes")
            
        with self.assertRaises(ValueError):
            reg.load_registry(verify_signature=True, public_key_path=pub_key_path)

    def test_live_pid_lease_expiry_not_stale(self):
        db_dir = reg._get_db_dir()
        db_dir.mkdir(parents=True, exist_ok=True)
        lock_path = db_dir / "write.lock"
        
        # Create a lock file owned by a live PID (ours) with expired lease
        lock_data = {
            "pid": os.getpid(),
            "hostname": socket.gethostname(),
            "owner_token": "some-token",
            "acquired_at": time.time() - 100.0,
            "lease_duration": 10.0
        }
        with open(lock_path, "w", encoding="utf-8") as f:
            json.dump(lock_data, f)
            
        # Try to acquire lock - should fail with TimeoutError because process is still alive
        lock = reg.RegistryLock(lock_path, timeout=0.2)
        with self.assertRaises(TimeoutError):
            with lock:
                pass
 
    def test_stale_lock_deletion_race(self):
        db_dir = reg._get_db_dir()
        db_dir.mkdir(parents=True, exist_ok=True)
        lock_path = db_dir / "write.lock"
        
        # Setup lock that looks stale (dead PID on same host)
        lock_data = {
            "pid": 999999,  # hopefully dead PID
            "hostname": socket.gethostname(),
            "owner_token": "stale-token",
            "acquired_at": time.time(),
            "lease_duration": 15.0
        }
        with open(lock_path, "w", encoding="utf-8") as f:
            json.dump(lock_data, f)
            
        # If the lock file is replaced with a different owner token, breaking it should return False
        with open(lock_path, "w", encoding="utf-8") as f:
            json.dump({"owner_token": "active-token"}, f)
        res = reg._safe_break_stale_lock(lock_path, "stale-token")
        self.assertFalse(res)
        
        # Verify the file still exists and contains the active token
        with open(lock_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        self.assertEqual(data["owner_token"], "active-token")

    def test_corrupt_lock_exit_unlink_prevention(self):
        db_dir = reg._get_db_dir()
        db_dir.mkdir(parents=True, exist_ok=True)
        lock_path = db_dir / "write.lock"
        
        # Setup lock file with bad/corrupt JSON
        with open(lock_path, "w") as f:
            f.write("{invalid_json:}")
            
        # Instantiate RegistryLock, simulate it was acquired and try to exit
        lock = reg.RegistryLock(lock_path)
        lock.acquired = True
        
        # Call __exit__ -> it should handle parse error and NOT delete the lock
        lock.__exit__(None, None, None)
        self.assertTrue(lock_path.exists())

    def test_generation_pointer_mismatch(self):
        db_dir = reg._get_db_dir()
        db_dir.mkdir(parents=True, exist_ok=True)
        
        # Write generation 2 json, but set generation inside json to 1
        generations_dir = db_dir / "generations"
        generations_dir.mkdir(parents=True, exist_ok=True)
        with open(generations_dir / "2.json", "w", encoding="utf-8") as f:
            json.dump({"models": {}, "generation": 1}, f)
            
        current_file = db_dir / "current"
        current_file.write_text("2\n")
        
        # load_registry should raise ValueError due to mismatch
        with self.assertRaises(ValueError) as ctx:
            reg.load_registry()
        self.assertIn("Registry generation does not match current pointer", str(ctx.exception))

    def test_safe_base_explicit_enforcement(self):
        from train_hf_dpo_adamw import verify_release_receipt
        
        # Define trusted base directory inside test dir
        trusted_base = Path(self.test_dir) / "safe_base"
        trusted_base.mkdir(exist_ok=True)
        
        # Create a receipt that resides OUTSIDE the trusted base
        bad_receipt = Path(self.test_dir) / "escaped_receipt.json"
        receipt_data = {
            "relative_artifact_path": "checkpoint",
            "files": {"model.safetensors": "hash"},
            "bundle_sha256": "bundlehash"
        }
        with open(bad_receipt, "w") as f:
            json.dump(receipt_data, f)
            
        pub_key = Path(self.test_dir) / "public_key.pem"
        with open(pub_key, "w") as f:
            f.write("pubkey")
            
        with open(Path(str(bad_receipt) + ".sig"), "wb") as f:
            f.write(b"sigbytes")
            
        # verify_release_receipt passing trusted_base as safe_base should raise ValueError
        with self.assertRaises(ValueError) as ctx:
            verify_release_receipt(bad_receipt, pub_key, safe_base=trusted_base)
        self.assertIn("escapes trusted safe base", str(ctx.exception))

    def test_signed_write_succeeds_on_valid_state(self):
        priv = os.path.join(self.test_dir, "pk.pem")
        pub = os.path.join(self.test_dir, "pub.pem")
        reg.generate_ed25519_keypair(priv, pub)
        model_dir = Path(self.test_dir) / "m"
        model_dir.mkdir()
        (model_dir / "model.safetensors").write_text("weights")
        reg.register_model("m1", str(model_dir), sign=True, private_key_path=priv)
        
        # Second signed write over untampered state MUST succeed
        reg.register_model("m2", str(model_dir), sign=True, private_key_path=priv)
        loaded = reg.load_registry(verify_signature=True, public_key_path=pub)
        self.assertIn("m2", loaded["models"])


if __name__ == "__main__":
    unittest.main()




