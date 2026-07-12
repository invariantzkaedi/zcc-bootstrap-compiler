import os
import sys
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
            load_model_hardened(str(model_dir), enforce_allow_list=True)
        self.assertIn("No registry entry found", str(ctx.exception))
        
        # Register model
        reg.register_model(
            model_name="mock-model-v1",
            model_path=str(model_dir),
            author="Anunnaki-Security",
            description="Secure baseline test model"
        )
        
        # Load registered model with enforce_allow_list=True -> should succeed (mocked load)
        load_model_hardened(str(model_dir), enforce_allow_list=True)
        
        # Try loading unregistered model name -> should fail
        with self.assertRaises(ValueError) as ctx:
            load_model_hardened("unregistered-name", enforce_allow_list=True)
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
        sig_path = reg._get_signature_path(reg.get_registry_path())
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
            load_model_hardened(str(model_dir), enforce_allow_list=True)
        self.assertIn("integrity verification failed", str(ctx.exception))
        
        # Register the model to the allow-list registry
        reg.register_model(
            model_name="registered-consistent-model",
            model_path=str(model_dir),
            author="Anunnaki-Security",
            description="Signed testing model"
        )
        
        # Verify it now passes verification and successfully loads (meaning it calls from_pretrained)
        load_model_hardened(str(model_dir), enforce_allow_list=True)
        mock_model.from_pretrained.assert_called_once()
        mock_tokenizer.from_pretrained.assert_called_once()




