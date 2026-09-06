#!/usr/bin/env python3
"""
tools/colab_40qubit_grover_runner.py
========================================================================
  🔱 ZKAEDI PRIME // 40-QUBIT GROVER CRYPTANALYTIC SEARCH COLAB RUNNER
  1,099,511,627,776 Amplitudes (40Q — 1.10 Trillion!) • 8 Octants
========================================================================
Single-cell executable runner for Google Colab with NVIDIA A100/H100 GPU.
"""

import os
import sys
import time
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

from quantum_40qubit_grover_engine import (
    QUBITS_40, TOTAL_AMPS_40, OCTANTS_COUNT_40, AMPS_PER_OCTANT,
    DEFAULT_TARGET_PREIMAGE, print_banner, verify_grover_theoretical_scaling,
    generate_grover_sonification, run_40qubit_grover_gauntlet,
    generate_grover_report
)

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI PRIME // 40-Qubit Grover Colab Runner")
    parser.add_argument("--scaled", action="store_true", help="Force scaled mode (1 GiB buffer plane)")
    parser.add_argument("--target", type=lambda x: int(x, 0), default=DEFAULT_TARGET_PREIMAGE, help="Target preimage hex value")
    args = parser.parse_args()

    print("=" * 72)
    print("  🔱 INITIALIZING 40-QUBIT GROVER SEARCH COLAB RUNNER")
    print("=" * 72)

    has_cuda = torch.cuda.is_available()
    device_name = torch.cuda.get_device_name(0) if has_cuda else "CPU Host"
    vram_gb = torch.cuda.get_device_properties(0).total_memory / (1024**3) if has_cuda else 0.0

    print(f"  • Accelerator Detected : {device_name}")
    print(f"  • Dedicated VRAM       : {vram_gb:.2f} GiB")
    print(f"  • Search Space         : 1,099,511,627,776 Amplitudes (40 Qubits)")
    print(f"  • Memory Staging       : 512-GiB logical state space represented by eight distinct sequentially staged octants")
    print("=" * 72 + "\n")

    run_40qubit_grover_gauntlet(target_preimage=args.target, scaled=args.scaled)

if __name__ == "__main__":
    main()
