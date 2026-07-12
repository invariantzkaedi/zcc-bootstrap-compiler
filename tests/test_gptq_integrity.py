import json
import shutil
import tempfile
import unittest
import hashlib
from pathlib import Path

from zkaedi_security_utils import verify_gptq_config_integrity

class TestGPTQConfigIntegrity(unittest.TestCase):
    def setUp(self):
        self.test_dir = Path(tempfile.mkdtemp(prefix="zkaedi_test_gptq_"))

    def tearDown(self):
        if self.test_dir.exists():
            shutil.rmtree(self.test_dir)

    def write_config(self, data):
        config_path = self.test_dir / "quantize_config.json"
        with open(config_path, "w", encoding="utf-8") as f:
            json.dump(data, f)
        return config_path

    def test_valid_config(self):
        """Verify that a standard valid config passes validation."""
        self.write_config({"bits": 4, "group_size": 128})
        res = verify_gptq_config_integrity(str(self.test_dir))
        self.assertTrue(res["valid"])
        self.assertEqual(res["config"]["bits"], 4)

    def test_config_hash_matching(self):
        """Verify that expected hash validation works when hashes match."""
        config_path = self.write_config({"bits": 4, "group_size": 128})
        
        # Calculate actual hash
        h = hashlib.sha256()
        with open(config_path, "rb") as f:
            h.update(f.read())
        actual_hash = h.hexdigest()

        res = verify_gptq_config_integrity(str(self.test_dir), expected_config_hash=actual_hash)
        self.assertTrue(res["valid"])
        self.assertEqual(res["config_hash"], actual_hash)

    def test_tampered_hash_rejection(self):
        """Verify that tampering with config file triggers a rejection when matching expected hash."""
        self.write_config({"bits": 4, "group_size": 128})
        wrong_hash = "a" * 64
        
        with self.assertRaises(ValueError) as ctx:
            verify_gptq_config_integrity(str(self.test_dir), expected_config_hash=wrong_hash)
        self.assertIn("hash mismatch", str(ctx.exception))

    def test_invalid_bits_rejection(self):
        """Verify that invalid bits values are rejected (fail-closed)."""
        self.write_config({"bits": 16, "group_size": 128})
        with self.assertRaises(ValueError) as ctx:
            verify_gptq_config_integrity(str(self.test_dir))
        self.assertIn("Invalid or missing 'bits'", str(ctx.exception))

    def test_invalid_group_size_rejection(self):
        """Verify that invalid group_size values are rejected (fail-closed)."""
        self.write_config({"bits": 4, "group_size": -1})
        with self.assertRaises(ValueError) as ctx:
            verify_gptq_config_integrity(str(self.test_dir))
        self.assertIn("Invalid or missing 'group_size'", str(ctx.exception))

if __name__ == "__main__":
    unittest.main()
