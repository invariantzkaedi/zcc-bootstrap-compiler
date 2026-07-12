import unittest
import os
import tempfile
import pandas as pd
from pathlib import Path
from validate_training_health import verify_dataset_integrity

class TestDatasetIntegrity(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="zcc_dataset_test_")
        self.valid_parquet = os.path.join(self.test_dir, "valid.parquet")
        
        # Create a valid Parquet dataset
        df = pd.DataFrame({
            "system": ["sys1", "sys2"],
            "prompt": ["p1", "p2"],
            "chosen": ["c1", "c2"],
            "rejected": ["r1", "r2"]
        })
        df.to_parquet(self.valid_parquet, index=False)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.test_dir)

    def test_valid_dataset_passes(self):
        verify_dataset_integrity(self.valid_parquet)

    def test_missing_column_fails(self):
        invalid_parquet = os.path.join(self.test_dir, "missing_col.parquet")
        df = pd.DataFrame({
            "prompt": ["p1"],
            "chosen": ["c1"]
        })
        df.to_parquet(invalid_parquet, index=False)
        
        with self.assertRaises(ValueError) as ctx:
            verify_dataset_integrity(invalid_parquet)
        self.assertIn("missing required columns", str(ctx.exception))

    def test_invalid_column_type_fails(self):
        invalid_parquet = os.path.join(self.test_dir, "bad_type.parquet")
        df = pd.DataFrame({
            "prompt": ["p1"],
            "chosen": ["c1"],
            "rejected": [12345]  # Numeric column type
        })
        df.to_parquet(invalid_parquet, index=False)
        
        with self.assertRaises(TypeError) as ctx:
            verify_dataset_integrity(invalid_parquet)
        self.assertIn("must be of string/object type", str(ctx.exception))

    def test_nonexistent_file_fails(self):
        with self.assertRaises(ValueError) as ctx:
            verify_dataset_integrity(os.path.join(self.test_dir, "nonexistent.parquet"))
        self.assertIn("Failed to read Parquet dataset", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
