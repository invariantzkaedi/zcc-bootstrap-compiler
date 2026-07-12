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
    normalize_scalar_metric
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
        # 1. Normal numbers
        self.assertEqual(normalize_scalar_metric("m", 4.2), 4.2)
        # 2. Single element tensor
        self.assertEqual(normalize_scalar_metric("m", torch.tensor(1.5)), 1.5)
        # 3. Single element numpy array
        self.assertEqual(normalize_scalar_metric("m", np.array([2.5])), 2.5)
        # 4. Multi element tensor should raise ValueError
        with self.assertRaises(ValueError):
            normalize_scalar_metric("m", torch.tensor([1.0, 2.0]))
        # 5. Multi element numpy array should raise ValueError
        with self.assertRaises(ValueError):
            normalize_scalar_metric("m", np.array([1.0, 2.0]))
        # 6. Arbitrary objects should return None
        self.assertIsNone(normalize_scalar_metric("m", "unknown"))

    def test_get_relative_safe_path_fail_closed(self):
        base_dir = Path(self.test_dir) / "workspace"
        base_dir.mkdir()
        
        # Inside workspace
        inside_path = base_dir / "data.parquet"
        with open(inside_path, "w") as f:
            f.write("test")
            
        rel = get_relative_safe_path(inside_path, base_dir)
        self.assertEqual(rel, "data.parquet")
        
        # Outside workspace
        outside_dir = Path(self.test_dir) / "secret_zone"
        outside_dir.mkdir()
        outside_path = outside_dir / "secret.parquet"
        with open(outside_path, "w") as f:
            f.write("secret")
            
        with self.assertRaises(ValueError) as ctx:
            get_relative_safe_path(outside_path, base_dir)
        self.assertIn("Path is outside the declared safe workspace", str(ctx.exception))

    def test_tripwire_normal_logs_pass(self):
        logs = {"loss": 0.5, "rewards/margins": 0.2, "epoch": 1.0}
        control = TrainerControl()
        self.callback.on_log(None, None, control, logs=logs)
        self.assertFalse(control.should_training_stop)

    def test_tripwire_nan_fails_python_float(self):
        logs = {"loss": float("nan"), "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("NaN/Inf in metric 'loss'", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_inf_fails_python_float(self):
        logs = {"loss": float("inf"), "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("NaN/Inf in metric 'loss'", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_nan_fails_numpy_float(self):
        logs = {"loss": np.nan, "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("NaN/Inf in metric 'loss'", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_nan_fails_tensor(self):
        logs = {"loss": torch.tensor(float("nan")), "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("NaN/Inf in metric 'loss'", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_nonnumeric_metric_ignored(self):
        logs = {"loss": 0.5, "unrelated_str": "some_value"}
        control = TrainerControl()
        self.callback.on_log(None, None, control, logs=logs)
        self.assertFalse(control.should_training_stop)

    def test_tripwire_margin_saturation_warning(self):
        logs = {"loss": 0.5, "rewards/margins": 11.2}
        control = TrainerControl()
        with patch("train_hf_dpo_adamw.logger.warning") as mock_warn:
            self.callback.on_log(None, None, control, logs=logs)
            mock_warn.assert_called_once()
        self.assertFalse(control.should_training_stop)

    def test_tripwire_margin_saturation_critical_fails(self):
        logs = {"loss": 0.5, "rewards/margins": 16.5}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("preference margin saturation", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_negative_margin_saturation_fails(self):
        logs = {"loss": 0.5, "rewards/margins": -16.5}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("preference margin saturation", str(ctx.exception))
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
        
        # Create a mock training manifest file in the checkpoint directory
        manifest_file = checkpoint_dir / "training_manifest.json"
        attestation_id = "test-uuid-binding-token"
        manifest_data = {
            "test_metric": 42,
            "attestation_id": attestation_id,
            "model_payload_sha256": "weights_hash"
        }
        write_atomic_json(manifest_file, manifest_data)
        
        # Calculate manifest hash
        manifest_sha256 = reg.get_file_sha256(manifest_file)
        
        # Keygen for signing
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        # Test generate attestation with signing and relative paths
        generate_dpo_attestation(
            script_path=script_file,
            dataset_path=dataset_file,
            base_model="gpt2-safe",
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
            
        # Verify mutual bindings and relative paths
        self.assertEqual(att_data["attestation_id"], attestation_id)
        self.assertEqual(att_data["training_manifest"]["sha256"], manifest_sha256)
        self.assertEqual(att_data["dataset"]["path"], "dataset.parquet")
        self.assertEqual(att_data["checkpoint"]["path"], "checkpoint")
        
        # Verify signatures exist
        sig_file = checkpoint_dir / "dpo_security_attestation.json.sig"
        self.assertTrue(sig_file.exists())
        with open(sig_file, "rb") as f:
            signature = f.read()
            
        valid = reg.verify_registry_signature(att_data, signature, pub_key_path)
        self.assertTrue(valid)

        manifest_sig_file = manifest_file.with_suffix(manifest_file.suffix + ".sig")
        self.assertTrue(manifest_sig_file.exists())
        with open(manifest_sig_file, "rb") as f:
            m_signature = f.read()
        m_valid = reg.verify_registry_signature(manifest_data, m_signature, pub_key_path)
        self.assertTrue(m_valid)


if __name__ == "__main__":
    unittest.main()
