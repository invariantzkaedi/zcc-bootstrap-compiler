#!/usr/bin/env python3
"""
scripts/package_40q_zip.py
========================================================================
Robust, cross-platform artifact packager for ZKAEDI PRIME 40-Qubit Engine.
Operates seamlessly on Linux (Colab/WSL) and Windows.
Guarantees non-empty archive (never 22-byte empty zip).
========================================================================
"""

import os
import sys
import zipfile
import shutil

# Configure UTF-8 stdout for Windows consoles
if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

# Resolve repository root dynamically (works in CLI scripts or interactive Colab/Jupyter cells)
if "__file__" in globals():
    SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
    REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
else:
    # Running directly inside an interactive Google Colab / Jupyter notebook cell
    REPO_ROOT = os.getcwd()
    if not os.path.exists(os.path.join(REPO_ROOT, "artifacts")):
        parent = os.path.abspath(os.path.join(REPO_ROOT, ".."))
        if os.path.exists(os.path.join(parent, "artifacts")):
            REPO_ROOT = parent
    SCRIPT_DIR = os.path.join(REPO_ROOT, "scripts")

# Primary output path inside the repo artifacts directory
PRIMARY_ZIP = os.path.join(REPO_ROOT, "artifacts", "zkaedi_prime_40qubit_hypercube_artifacts.zip")

# Optional mirror target for host Windows environments
WINDOWS_ABSTRACT_DIR = r"E:\__GROUPED_IMAGES\ABSTRACT"
MIRROR_ZIP = os.path.join(WINDOWS_ABSTRACT_DIR, "zkaedi_prime_40qubit_hypercube_artifacts.zip") if os.path.exists(WINDOWS_ABSTRACT_DIR) else None

# Rel-paths of all essential 40Q & Chemistry artifacts
RELATIVE_ARTIFACTS = [
    os.path.join("artifacts", "40QUBIT_EXPLORATION_REPORT.md"),
    os.path.join("artifacts", "quantum_40qubit_metrics.json"),
    os.path.join("artifacts", "quantum_40qubit_hypercube_observatory.html"),
    os.path.join("artifacts", "QUANTUM_CHEMISTRY_VQE_REPORT.md"),
    os.path.join("artifacts", "quantum_chemistry_metrics.json"),
    os.path.join("artifacts", "quantum_chemistry_observatory.html"),
    os.path.join("artifacts", "quantum_sonification_40qubit.wav"),
    os.path.join("artifacts", "quantum_chemistry_vqe_sonification.wav"),
    os.path.join("notebooks", "zkaedi_prime_40qubit_hypercube.ipynb"),
    os.path.join("notebooks", "zkaedi_prime_quantum_chemistry_vqe.ipynb"),
    os.path.join("tools", "quantum_40qubit_hypercube_engine.py"),
    os.path.join("tools", "quantum_chemistry_hyperslab_engine.py")
]

def main():
    print("=" * 72)
    print("[*] ZKAEDI PRIME // 40-QUBIT ARTIFACT PACKAGER")
    print(f"  • Repo Root  : {REPO_ROOT}")
    print(f"  • Target ZIP : {PRIMARY_ZIP}")
    print("=" * 72)

    os.makedirs(os.path.dirname(PRIMARY_ZIP), exist_ok=True)

    # Collect existing files and package
    added_count = 0
    with zipfile.ZipFile(PRIMARY_ZIP, "w", compression=zipfile.ZIP_DEFLATED) as zout:
        # 1. Package key artifacts
        for rel_path in RELATIVE_ARTIFACTS:
            abs_path = os.path.join(REPO_ROOT, rel_path)
            if os.path.exists(abs_path):
                arcname = os.path.basename(abs_path)
                zout.write(abs_path, arcname)
                sz = os.path.getsize(abs_path)
                print(f"  [+] Added: {arcname:<45} ({sz:>10,} bytes)")
                added_count += 1
            else:
                print(f"  [-] Notice: {rel_path} not found on disk, skipping.")

        # 2. Also package all report markdowns and json telemetry in artifacts/
        artifacts_dir = os.path.join(REPO_ROOT, "artifacts")
        if os.path.exists(artifacts_dir):
            for fname in os.listdir(artifacts_dir):
                if fname.endswith((".md", ".json", ".html")) and fname not in [os.path.basename(r) for r in RELATIVE_ARTIFACTS]:
                    full_p = os.path.join(artifacts_dir, fname)
                    if os.path.isfile(full_p):
                        zout.write(full_p, fname)
                        added_count += 1

    final_size = os.path.getsize(PRIMARY_ZIP)
    if final_size <= 22 or added_count == 0:
        raise RuntimeError(f"FATAL ERROR: ZIP archive is empty ({final_size} bytes). No files were packed!")

    print("=" * 72)
    print(f"  [+] Primary Archive Finalized: {PRIMARY_ZIP}")
    print(f"  [+] Total Packed Size        : {final_size:,} bytes ({added_count} files)")

    # Mirror to Windows E: drive if available
    if MIRROR_ZIP:
        try:
            shutil.copy2(PRIMARY_ZIP, MIRROR_ZIP)
            print(f"  [+] Mirrored to Host E: Drive: {MIRROR_ZIP} ({os.path.getsize(MIRROR_ZIP):,} bytes)")
            
            # Also mirror to sanitized name in case user browser downloads look for it
            sanitized_mirror = os.path.join(WINDOWS_ABSTRACT_DIR, "E____GROUPED_IMAGES_ABSTRACT_zkaedi_prime_40qubit_hypercube_artifacts.zip")
            shutil.copy2(PRIMARY_ZIP, sanitized_mirror)
            print(f"  [+] Mirrored to Sanitized Name: {sanitized_mirror} ({os.path.getsize(sanitized_mirror):,} bytes)")
        except Exception as e:
            print(f"  [-] Mirror copy notice: {e}")

    # If running in Google Colab, trigger browser download
    try:
        from google.colab import files
        print("  [*] Triggering Colab Browser Download...")
        files.download(PRIMARY_ZIP)
    except Exception:
        pass

    print("=" * 72)

if __name__ == "__main__":
    main()
