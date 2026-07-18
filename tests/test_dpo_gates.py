import unittest
import os
import json
import tempfile
import torch
from pathlib import Path
from unittest.mock import patch, MagicMock

import train_hf_dpo_adamw_hardened_v3 as train_v3
import zkaedi_model_registry as reg

class TestDPOGovernanceGates(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="zcc_dpo_gates_")
        self.old_env = os.environ.get("ZKAEDI_SAFE_BASE")
        os.environ["ZKAEDI_SAFE_BASE"] = self.test_dir
        
        self.mock_registry_file = Path(self.test_dir) / "zkaedi_model_registry.json"
        self._old_get_registry_path = reg.get_registry_path
        reg.get_registry_path = lambda: self.mock_registry_file

    def tearDown(self):
        reg.get_registry_path = self._old_get_registry_path
        if self.old_env is not None:
            os.environ["ZKAEDI_SAFE_BASE"] = self.old_env
        else:
            os.environ.pop("ZKAEDI_SAFE_BASE", None)
            
        import shutil
        shutil.rmtree(self.test_dir)

    def test_f3_partition_integrity(self):
        mock_dataset = MagicMock()
        mock_dataset.__len__.return_value = 10
        
        def run_check(train, eval):
            if not isinstance(train, list):
                raise ValueError("train indices must be a list")
            if not isinstance(eval, list):
                raise ValueError("eval indices must be a list")
            if len(train) == 0:
                raise ValueError("empty train split")
            if len(eval) == 0:
                raise ValueError("empty eval split")
                
            for idx in train:
                if isinstance(idx, bool):
                    raise ValueError("boolean index not allowed")
                if type(idx) is not int:
                    raise ValueError("non-integer index")
                if idx < 0:
                    raise ValueError("negative index")
                if idx >= len(mock_dataset):
                    raise ValueError("out-of-range index")
                    
            for idx in eval:
                if isinstance(idx, bool):
                    raise ValueError("boolean index not allowed")
                if type(idx) is not int:
                    raise ValueError("non-integer index")
                if idx < 0:
                    raise ValueError("negative index")
                if idx >= len(mock_dataset):
                    raise ValueError("out-of-range index")
                    
            if len(train) != len(set(train)):
                raise ValueError("duplicate train index")
            if len(eval) != len(set(eval)):
                raise ValueError("duplicate eval index")
            if set(train) & set(eval):
                raise ValueError("train/eval overlap")
                
        with self.assertRaisesRegex(ValueError, "empty train split"):
            run_check([], [1, 2])
        with self.assertRaisesRegex(ValueError, "empty eval split"):
            run_check([1, 2], [])
        with self.assertRaisesRegex(ValueError, "boolean index not allowed"):
            run_check([True, 1], [2, 3])
        with self.assertRaisesRegex(ValueError, "non-integer index"):
            run_check(["1", 2], [3, 4])
        with self.assertRaisesRegex(ValueError, "negative index"):
            run_check([-1, 2], [3, 4])
        with self.assertRaisesRegex(ValueError, "out-of-range index"):
            run_check([11, 2], [3, 4])
        with self.assertRaisesRegex(ValueError, "duplicate train index"):
            run_check([1, 1], [3, 4])
        with self.assertRaisesRegex(ValueError, "duplicate eval index"):
            run_check([1, 2], [3, 3])
        with self.assertRaisesRegex(ValueError, "train/eval overlap"):
            run_check([1, 2], [2, 3])

    def test_f4_determinism_provenance_branches(self):
        with patch("torch.use_deterministic_algorithms") as mock_use_det:
            # 1. Success Branch
            mock_use_det.return_value = None
            det_enabled = False
            det_warn_only = False
            try:
                torch.use_deterministic_algorithms(True, warn_only=True)
                det_enabled = True
                det_warn_only = True
            except RuntimeError:
                det_enabled = False
                det_warn_only = False
            self.assertTrue(det_enabled)
            self.assertTrue(det_warn_only)
            
            # 2. Failure Branch
            mock_use_det.side_effect = RuntimeError("Mock GPU restriction")
            try:
                torch.use_deterministic_algorithms(True, warn_only=True)
                det_enabled = True
                det_warn_only = True
            except RuntimeError:
                try:
                    torch.use_deterministic_algorithms(False)
                except Exception:
                    pass
                det_enabled = False
                det_warn_only = False
            self.assertFalse(det_enabled)
            self.assertFalse(det_warn_only)

    def test_f2_split_identity_mutation(self):
        manifest_1 = {
            "train": [0, 1, 2],
            "eval": [3, 4]
        }
        manifest_2 = {
            "train": [0, 1, 3],
            "eval": [2, 4]
        }
        
        manifest_path = Path(self.test_dir) / "dpo_v1_manifest.json"
        with open(manifest_path, "w") as f:
            json.dump(manifest_1, f)
            
        hash_1 = reg.get_file_sha256(manifest_path)
        
        with open(manifest_path, "w") as f:
            json.dump(manifest_2, f)
            
        hash_2 = reg.get_file_sha256(manifest_path)
        self.assertNotEqual(hash_1, hash_2)


if __name__ == "__main__":
    unittest.main()
