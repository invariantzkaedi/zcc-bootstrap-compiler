import unittest
import os
import json
import tempfile
from pathlib import Path
from unittest.mock import MagicMock, patch

from transformers import TrainerControl
from train_hf_dpo_adamw import DPOSTripwireCallback, generate_dpo_attestation
import zkaedi_model_registry as reg

class TestDPOHardening(unittest.TestCase):
    def setUp(self):
        os.environ["ZKAEDI_SAFE_BASE"] = "/"
        self.test_dir = tempfile.mkdtemp(prefix="zcc_dpo_test_")
        self.callback = DPOSTripwireCallback()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.test_dir)

    def test_tripwire_normal_logs_pass(self):
        # Normal training metrics should pass without raising
        logs = {"loss": 0.5, "rewards/margins": 0.2, "epoch": 1.0}
        control = TrainerControl()
        self.callback.on_log(None, None, control, logs=logs)
        self.assertFalse(control.should_training_stop)

    def test_tripwire_nan_fails(self):
        # NaNs in metrics should raise ValueError and flag stop training
        logs = {"loss": float("nan"), "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("NaN/Inf in metric 'loss'", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_inf_fails(self):
        # Infs in metrics should raise ValueError and flag stop training
        logs = {"loss": float("inf"), "rewards/margins": 0.2}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("NaN/Inf in metric 'loss'", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_tripwire_margin_saturation_warning(self):
        # Margins > 10 should issue warnings but not stop unless > 15
        logs = {"loss": 0.5, "rewards/margins": 11.2}
        control = TrainerControl()
        with patch("train_hf_dpo_adamw.logger.warning") as mock_warn:
            self.callback.on_log(None, None, control, logs=logs)
            mock_warn.assert_called_once()
        self.assertFalse(control.should_training_stop)

    def test_tripwire_margin_saturation_critical_fails(self):
        # Margins > 15 should abort training
        logs = {"loss": 0.5, "rewards/margins": 16.5}
        control = TrainerControl()
        with self.assertRaises(ValueError) as ctx:
            self.callback.on_log(None, None, control, logs=logs)
        self.assertIn("preference margin saturation", str(ctx.exception))
        self.assertTrue(control.should_training_stop)

    def test_generate_dpo_attestation_and_signing(self):
        # Mock script and dataset files
        script_file = Path(self.test_dir) / "train_script.py"
        with open(script_file, "w") as f:
            f.write("DPO training code")
            
        dataset_file = Path(self.test_dir) / "dataset.parquet"
        with open(dataset_file, "w") as f:
            f.write("parquet dataset content")
            
        adapter_dir = Path(self.test_dir) / "adapter"
        adapter_dir.mkdir()
        
        # Keygen for signing
        priv_key_path = os.path.join(self.test_dir, "private_key.pem")
        pub_key_path = os.path.join(self.test_dir, "public_key.pem")
        reg.generate_ed25519_keypair(priv_key_path, pub_key_path)
        
        # Test generate attestation without signing
        generate_dpo_attestation(
            script_path=script_file,
            dataset_path=dataset_file,
            base_model="gpt2-safe",
            adapter_dir=adapter_dir,
            combined_hash="combined_adapter_hash",
            files_dict={"weights.safetensors": "weights_hash"}
        )
        
        att_json = adapter_dir / "dpo_security_attestation.json"
        self.assertTrue(att_json.exists())
        with open(att_json, "r") as f:
            att_data = json.load(f)
            self.assertEqual(att_data["base_model"], "gpt2-safe")
            self.assertEqual(att_data["adapter"]["combined_sha256"], "combined_adapter_hash")

        # Test generate attestation with signing
        generate_dpo_attestation(
            script_path=script_file,
            dataset_path=dataset_file,
            base_model="gpt2-safe",
            adapter_dir=adapter_dir,
            combined_hash="combined_adapter_hash",
            files_dict={"weights.safetensors": "weights_hash"},
            private_key_path=priv_key_path
        )
        
        sig_file = adapter_dir / "dpo_security_attestation.json.sig"
        self.assertTrue(sig_file.exists())
        with open(sig_file, "rb") as f:
            signature = f.read()
            
        with open(att_json, "r") as f:
            signed_att_data = json.load(f)
            
        # Verify signature
        valid = reg.verify_registry_signature(signed_att_data, signature, pub_key_path)
        self.assertTrue(valid)




if __name__ == "__main__":
    unittest.main()
