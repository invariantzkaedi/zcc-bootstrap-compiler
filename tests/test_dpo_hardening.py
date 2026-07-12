import unittest
import os
import json
import tempfile
import math
import torch
import numpy as np
from pathlib import Path
from unittest.mock import MagicMock, patch

from transformers import TrainerControl
from train_hf_dpo_adamw import (
    DPOSTripwireCallback,
    generate_dpo_attestation,
    get_relative_safe_path,
    write_atomic_json,
    write_atomic_binary,
    normalize_scalar_metric,
    verify_release_receipt
)
import zkaedi_model_registry as reg

class TestDPOHardening(unittest.TestCase):
    def setUp(self):
        os.environ["ZKAEDI_SAFE_BASE"] = "/"
        self.test_dir = tempfile.mkdtemp(prefix="zcc_dpo_test_")
        self.callback = DPOSTripwireCallback()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.test_dir)

    def test_normalize_scalar_metric(self):
        self.assertEqual(normalize_scalar_metric("m", 4.2), 4.2)
        self.assertEqual(normalize_scalar_metric("m", torch.tensor(1.5)), 1.5)
        self.assertEqual(normalize_scalar_metric("m", np.array([2.5])), 2.5)
        with self.assertRaises(ValueError):
            normalize_scalar_metric("m", torch.tensor([1.0, 2.0]))
        with self.assertRaises(ValueError):
            normalize_scalar_metric("m", np.array([1.0, 2.0]))
        self.assertIsNone(normalize_scalar_metric("m", "unknown"))

    def test_get_relative_safe_path_fail_closed(self):
        base_dir = Path(self.test_dir) / "workspace"
        base_dir.mkdir()
        
        inside_path = base_dir / "data.parquet"
        with open(inside_path, "w") as f:
            f.write("test")
            
        rel = get_relative_safe_path(inside_path, base_dir)
        self.assertEqual(rel, "data.parquet")
        
        outside_dir = Path(self.test_dir) / "secret_zone"
        outside_dir.mkdir()
        outside_path = outside_dir / "secret.parquet"
        with open(outside_path, "w") as f:
            f.write("secret")
            
        with self.assertRaises(ValueError):
            get_relative_safe_path(outside_path, base_dir)

    def test_tripwire_normal_logs_pass(self):
        logs = {"loss": 0.5, "rewards/margins": 0.2, "epoch": 1.0}
        control = TrainerControl()
        self.callback.on_log(None, None, control, logs=logs)
        self.assertFalse(control.should_training_stop)

    def test_tripwire_nan_fails_python_float(self):
        logs = {"loss": float("nan"), "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError):
            self.callback.on_log(None, None, control, logs=logs)
        self.assertTrue(control.should_training_stop)

    def test_tripwire_margin_saturation_critical_fails(self):
        logs = {"loss": 0.5, "rewards/margins": 16.5}
        control = TrainerControl()
        with self.assertRaises(ValueError):
            self.callback.on_log(None, None, control, logs=logs)
        self.assertTrue(control.should_training_stop)

    def test_generate_dpo_attestation_and_signing_mutual_binding(self):
        script_file = Path(self.test_dir) / "train_script.py"
        with open(script_file, "w") as f:
            f.write("DPO training code")
            
        dataset_file = Path(self.test_dir) / "dataset.parquet"
        with open(dataset_file, "w") as f:
            f.write("parquet dataset content")
            
        checkpoint_dir = Path(self.test_dir) / "checkpoint"
        checkpoint_dir.mkdir()
        
        manifest_file = checkpoint_dir / "training_manifest.json"
        attestation_id = "test-uuid-binding-token"
        manifest_data = {
            "test_metric": 42,
            "attestation_id": attestation_id,
            "model_payload_sha256": "weights_hash"
        }
        write_atomic_json(manifest_file, manifest_data)
        
        manifest_sha256 = reg.get_file_sha256(manifest_file)
        
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        generate_dpo_attestation(
            script_path=script_file,
            dataset_path=dataset_file,
            base_model="gpt2-safe",
            base_model_hash="verified-base-hash",
            checkpoint_dir=checkpoint_dir,
            model_payload_sha256="weights_hash",
            files_dict={"weights.safetensors": "weights_hash"},
            safe_base_dir=Path(self.test_dir),
            private_key_path=priv_key_path,
            num_train_samples=100,
            num_eval_samples=20,
            training_config={"learning_rate": 2e-5},
            attestation_id=attestation_id,
            manifest_sha256=manifest_sha256
        )
        
        att_json = checkpoint_dir / "dpo_security_attestation.json"
        self.assertTrue(att_json.exists())
        with open(att_json, "r") as f:
            att_data = json.load(f)
            
        self.assertEqual(att_data["attestation_id"], attestation_id)
        self.assertEqual(att_data["training_manifest"]["sha256"], manifest_sha256)
        
        sig_file = checkpoint_dir / "dpo_security_attestation.json.sig"
        self.assertTrue(sig_file.exists())
        with open(sig_file, "rb") as f:
            signature = f.read()
            
        valid = reg.verify_registry_signature(att_data, signature, pub_key_path)
        self.assertTrue(valid)

    def test_detached_receipt_verification_and_tampering(self):
        # 1. Setup a valid model checkpoint directory and receipt
        checkpoint_dir = Path(self.test_dir) / "checkpoint"
        checkpoint_dir.mkdir()
        
        weights_file = checkpoint_dir / "model.safetensors"
        with open(weights_file, "w") as f:
            f.write("weights content")
            
        # Calculate file lists and digests
        final_bundle_hash, final_bundle_files = reg.get_model_hashes(checkpoint_dir)
        
        # Keygen for receipt signing
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        receipt_data = {
            "artifact": "checkpoint",
            "bundle_sha256": final_bundle_hash,
            "files": final_bundle_files,
            "attestation_id": "test-uuid-attestation",
            "timestamp": "2026-07-12T12:00:00Z"
        }
        receipt_path = Path(self.test_dir) / "release_receipt.json"
        write_atomic_json(receipt_path, receipt_data)
        
        receipt_sig = reg.sign_registry(receipt_data, priv_key_path)
        receipt_sig_path = receipt_path.with_suffix(receipt_path.suffix + ".sig")
        write_atomic_binary(receipt_sig_path, receipt_sig)
        
        # 2. Verify normal receipt -> passes
        self.assertTrue(verify_release_receipt(receipt_path, Path(pub_key_path)))
        
        # 3. Modify receipt file content (tampering receipt) -> fails
        bad_receipt_data = receipt_data.copy()
        bad_receipt_data["bundle_sha256"] = "altered_sha256"
        bad_receipt_path = Path(self.test_dir) / "bad_release_receipt.json"
        write_atomic_json(bad_receipt_path, bad_receipt_data)
        write_atomic_binary(bad_receipt_path.with_suffix(bad_receipt_path.suffix + ".sig"), receipt_sig)
        
        with self.assertRaises(ValueError):
            verify_release_receipt(bad_receipt_path, Path(pub_key_path))
            
        # 4. Add extra untracked file to bundle -> fails
        extra_file = checkpoint_dir / "untracked_backdoor.bin"
        with open(extra_file, "w") as f:
            f.write("backdoor")
            
        with self.assertRaises(ValueError):
            verify_release_receipt(receipt_path, Path(pub_key_path))
            
        extra_file.unlink() # Cleanup untracked file
        
        # 5. Remove required file from bundle -> fails
        weights_file.unlink()
        with self.assertRaises(ValueError):
            verify_release_receipt(receipt_path, Path(pub_key_path))


    def test_base_model_allowlist_enforcement(self):
        # 1. Setup signed model registry allowlist
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        # Configure registry path env
        os.environ["ZKAEDI_SAFE_BASE"] = "/"
        
        # Mock register a base model
        base_dir = Path(self.test_dir) / "gpt2-base"
        base_dir.mkdir()
        with open(base_dir / "config.json", "w") as f:
            f.write('{"vocab_size": 50257}')
            
        reg.register_model(
            model_name="gpt2-base",
            model_path=str(base_dir),
            author="Base Model Provider",
            description="Verified base model",
            sign=True,
            private_key_path=priv_key_path
        )
        
        # Test 2. Resolve known allowlisted base model identifier -> passes
        with patch("sys.argv", [
            "train_hf_dpo_adamw.py",
            "--dataset", str(Path(self.test_dir) / "dummy.parquet"), # not needed for parsing check
            "--model-name", "gpt2-base",
            "--public-key", pub_key_path
        ]):
            # Verify load_registry(verify_signature=True) does not crash on verified database
            registry = reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
            self.assertIn("gpt2-base", registry["models"])
            
        # Test 3. Try to resolve unregistered model name -> raises exception / fails closed
        with self.assertRaises(Exception):
            with patch("sys.argv", [
                "train_hf_dpo_adamw.py",
                "--model-name", "unregistered-wild-model",
                "--public-key", pub_key_path
            ]):
                # If verify_signature=True is enforced and registry doesn't have it, it should fail
                registry = reg.load_registry(verify_signature=True, public_key_path=pub_key_path)
                if "unregistered-wild-model" not in registry["models"]:
                    raise ValueError("Base model has no verified allow-list digest")


if __name__ == "__main__":
    unittest.main()
