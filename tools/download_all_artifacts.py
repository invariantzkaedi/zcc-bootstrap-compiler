#!/usr/bin/env python3
"""
tools/download_all_artifacts.py
========================================================================
  🔱 ZKAEDI PRIME // QUANTUM & CRYPTOGRAPHIC ARTIFACTS PACKAGER & DOWNLOADER
========================================================================
Packages the entire artifacts/ directory containing all Markdown reports,
audio sonifications (.wav), JSON cryptographic receipts, and verification
ledgers into a single zip file and triggers browser download in Colab.
"""

import os
import sys
import shutil
import time

if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    artifact_dir = os.path.join(repo_root, "artifacts")
    if not os.path.exists(artifact_dir):
        artifact_dir = "artifacts"

    if not os.path.exists(artifact_dir):
        print(f"[!] Artifacts directory '{artifact_dir}' not found.")
        return

    files_list = os.listdir(artifact_dir)
    print("=" * 72)
    print("  🔱 ZKAEDI PRIME // PACKAGING ALL QUANTUM ARTIFACTS")
    print("=" * 72)
    print(f"  • Source Directory : {artifact_dir}")
    print(f"  • Files Found      : {len(files_list)} items")
    for fname in sorted(files_list):
        fpath = os.path.join(artifact_dir, fname)
        if os.path.isfile(fpath):
            fsize = os.path.getsize(fpath)
            print(f"      - {fname:<45} ({fsize:,} bytes)")

    zip_name = "zkaedi_all_quantum_artifacts"
    zip_base = os.path.join(os.getcwd(), zip_name)
    zip_path = f"{zip_base}.zip"

    print("\n  📦 Compressing artifacts into zip bundle...")
    shutil.make_archive(zip_base, "zip", artifact_dir)
    zip_size = os.path.getsize(zip_path)
    print(f"  ✔ Bundle Created: {zip_path} ({zip_size:,} bytes / {zip_size/(1024*1024):.2f} MB)")

    try:
        from google.colab import files
        print(f"\n  ⬇ Triggering direct browser download of '{os.path.basename(zip_path)}'...")
        files.download(zip_path)
        print("  ✔ Download signal dispatched to your browser!")
    except Exception as e:
        print(f"\n  [i] Colab direct download note: {e}")
        print(f"  [i] To download manually from Colab, check the file browser on the left panel:")
        print(f"      Right-click '{os.path.basename(zip_path)}' -> Download.")

    print("=" * 72 + "\n")

if __name__ == "__main__":
    main()
