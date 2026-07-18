import unittest
from pathlib import Path
import os
import shutil
import tempfile

from zkaedi_security_utils import validate_safe_path

class TestV2Containment(unittest.TestCase):
    def setUp(self):
        self.temp_dir = Path(tempfile.mkdtemp(prefix="zkaedi_test_")).resolve()
        # Set up safe bases in temp directory
        self.safe_base = self.temp_dir / "safe"
        self.safe_base.mkdir()
        
        # Sibling prefix path (e.g. /mnt/safe_evil vs /mnt/safe)
        self.evil_base = self.temp_dir / "safe_evil"
        self.evil_base.mkdir()
        
        # A file to test base-is-file control
        self.base_file = self.temp_dir / "safe_file.txt"
        self.base_file.write_text("dummy")

    def tearDown(self):
        if self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)

    def test_c1_valid_containment(self):
        # Path inside the safe base should be admitted
        target = self.safe_base / "file.parquet"
        target.write_text("data")
        res = validate_safe_path(
            str(target),
            must_exist=True,
            authoritative_safe_bases=[str(self.safe_base)]
        )
        self.assertEqual(res, target.resolve())

    def test_c2_traversal_reject(self):
        # Escape attempt using .. should be rejected
        target = self.safe_base / "../safe_evil/exfil.parquet"
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(target),
                must_exist=False,
                authoritative_safe_bases=[str(self.safe_base)]
            )

    def test_c3_sibling_prefix_file_reject(self):
        # Sibling prefix file (looks like prefix but is sibling folder) must be rejected
        target = self.evil_base / "exfil.parquet"
        target.write_text("data")
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(target),
                must_exist=True,
                authoritative_safe_bases=[str(self.safe_base)]
            )

    def test_c4_sibling_prefix_nonexistent_reject(self):
        # Nonexistent sibling prefix path must be rejected
        target = self.evil_base / "nonexistent.parquet"
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(target),
                must_exist=False,
                authoritative_safe_bases=[str(self.safe_base)]
            )

    def test_c5_legacy_extra_bases_sibling_prefix_reject(self):
        # Using extra_safe_bases should still fail sibling prefix check
        target = self.evil_base / "exfil.parquet"
        target.write_text("data")
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(target),
                must_exist=True,
                extra_safe_bases=[str(self.safe_base)]
            )

    def test_c6_base_is_file_reject(self):
        # Base must be a directory, file base is invalid and must be rejected
        target = self.safe_base / "file.parquet"
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(target),
                must_exist=False,
                authoritative_safe_bases=[str(self.base_file)]
            )

    def test_c7_nonexistent_base_reject(self):
        nonexistent_base = self.safe_base / "ghost_dir"
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(self.safe_base / "file.txt"),
                must_exist=False,
                authoritative_safe_bases=[str(nonexistent_base)]
            )

    def test_c8_relative_base_reject(self):
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(self.safe_base / "file.txt"),
                must_exist=False,
                authoritative_safe_bases=["./relative_base"]
            )

    def test_c9_empty_base_list_reject(self):
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(self.safe_base / "file.txt"),
                must_exist=False,
                authoritative_safe_bases=[]
            )

    def test_c10_mutual_exclusion(self):
        with self.assertRaises(ValueError):
            validate_safe_path(
                str(self.safe_base / "file.txt"),
                must_exist=False,
                extra_safe_bases=[str(self.safe_base)],
                authoritative_safe_bases=[str(self.safe_base)]
            )

if __name__ == "__main__":
    unittest.main()
