#!/usr/bin/env python3
"""
tools/colab_surface_qec_runner.py
========================================================================
  🔱 ZKAEDI PRIME // SURFACE-17 TWO-PATCH LATTICE SURGERY COLAB RUNNER
  38 Physical Qubits • 2x Distance-3 Rotated Patches • Logical CNOT
========================================================================
Single-cell executable runner for Google Colab with NVIDIA A100/H100 GPU.
Packages and triggers browser download of all artifacts automatically.
"""

import os
import sys
import shutil
import argparse

# Force unbuffered streaming output in Colab
if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(line_buffering=True, encoding='utf-8')
    except Exception:
        pass

# Enable expandable segments to eliminate CUDA allocator fragmentation
os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"

try:
    import torch
except ImportError:
    print("[*] Installing PyTorch and NumPy in Colab environment...")
    os.system("pip install torch numpy")
    import torch

# Add tools directory to sys.path for robust Colab invocation
script_dir = os.path.dirname(os.path.abspath(__file__))
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

from quantum_surface_qec_lattice_surgery import (
    TOTAL_PHYSICAL_QUBITS, PATCH1_BASE, PATCH2_BASE, BOUNDARY_BASE,
    print_banner, run_surface_qec_gauntlet
)

def trigger_colab_download(zip_name: str = "zkaedi_surface_qec_artifacts"):
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    artifact_dir = os.path.join(repo_root, "artifacts")
    if not os.path.exists(artifact_dir):
        artifact_dir = "artifacts"

    zip_base = os.path.join(os.getcwd(), zip_name)
    zip_path = f"{zip_base}.zip"
    print(f"\n  📦 Packaging artifacts into '{zip_path}'...")
    shutil.make_archive(zip_base, "zip", artifact_dir)
    if os.path.exists(zip_path):
        print(f"  ✔ Archive created ({os.path.getsize(zip_path):,} bytes).")

    try:
        from google.colab import files
        print(f"  ⬇ Triggering direct browser download of '{os.path.basename(zip_path)}'...")
        files.download(zip_path)
        print(f"  ✔ Browser download triggered successfully!")
    except Exception as e:
        print(f"  [i] Colab direct download: {e}")
        print(f"  [i] Archive saved on disk at: {zip_path}")

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI PRIME // Surface-17 Two-Patch Lattice Surgery Colab Runner")
    parser.add_argument("--scaled", action="store_true", help="Force CPU scaled execution")
    args = parser.parse_args()

    print("=" * 72)
    print("  🔱 INITIALIZING SURFACE-17 TWO-PATCH LATTICE SURGERY COLAB RUNNER")
    print("=" * 72)

    has_cuda = torch.cuda.is_available()
    device_name = torch.cuda.get_device_name(0) if has_cuda else "CPU Host"
    vram_gb = torch.cuda.get_device_properties(0).total_memory / (1024**3) if has_cuda else 0.0

    print(f"  • Accelerator Detected : {device_name}")
    print(f"  • Dedicated VRAM       : {vram_gb:.2f} GiB")
    print(f"  • Physical Architecture: 38 Physical Qubits (2x 17Q Rotated Patches + 4 Bridge Ancillas)")
    print(f"  • Logical State        : Fault-Tolerant Bell State |Phi+>_L Synthesis via M_ZZ Lattice Surgery")
    print(f"  • Memory Staging       : 512-GiB logical state space represented by eight distinct sequentially staged octants")
    print("=" * 72 + "\n")

    run_surface_qec_gauntlet(scaled=args.scaled)
    trigger_colab_download("zkaedi_surface_qec_artifacts")

if __name__ == "__main__":
    main()
