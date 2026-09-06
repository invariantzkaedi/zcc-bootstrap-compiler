#!/usr/bin/env python3
"""
tools/colab_42qubit_hypercube_runner.py
========================================================================
  🔱 ZKAEDI PRIME // 42-QUBIT HYPER-CUBE COLAB RUNNER
  4,398,046,511,104 Amplitudes (42Q — 4.40 Trillion!) • 32 Super-Slabs
========================================================================
Single-cell executable runner for Google Colab with NVIDIA A100/H100 GPU.
"""

import os
import sys
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

from quantum_42qubit_hypercube_engine import (
    QUBITS_42, TOTAL_AMPS_42, SLABS_COUNT_42, AMPS_PER_SLAB,
    print_banner, run_42qubit_hypercube_gauntlet
)

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI PRIME // 42-Qubit Hyper-Cube Colab Runner")
    parser.add_argument("--scaled", action="store_true", help="Force scaled mode (1 GiB buffer plane)")
    args = parser.parse_args()

    print("=" * 72)
    print("  🔱 INITIALIZING 42-QUBIT HYPER-CUBE COLAB RUNNER")
    print("=" * 72)

    has_cuda = torch.cuda.is_available()
    device_name = torch.cuda.get_device_name(0) if has_cuda else "CPU Host"
    vram_gb = torch.cuda.get_device_properties(0).total_memory / (1024**3) if has_cuda else 0.0

    print(f"  • Accelerator Detected : {device_name}")
    print(f"  • Dedicated VRAM       : {vram_gb:.2f} GiB")
    print(f"  • Hilbert Dimension    : 4,398,046,511,104 Amplitudes (42 Qubits)")
    print(f"  • Memory Staging       : 2,048-GiB logical state space represented by thirty-two distinct sequentially staged super-slabs")
    print("=" * 72 + "\n")

    run_42qubit_hypercube_gauntlet(scaled=args.scaled)

if __name__ == "__main__":
    main()
