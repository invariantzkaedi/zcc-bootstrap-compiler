#!/usr/bin/env python3
"""
tools/colab_40qubit_hypercube_runner.py
========================================================================
  🔱 ZKAEDI PRIME // 40-QUBIT & 42-QUBIT HYPER-CUBE COLAB RUNNER
  1,099,511,627,776 Amplitudes (40Q) • 4,398,046,511,104 Amps (42Q)
========================================================================
Single-cell executable runner for Google Colab with NVIDIA A100/H100 GPU.
"""

import os
import sys
import time
import math
import json
import wave
import struct
import shutil
import hashlib
import uuid
import argparse
import numpy as np

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

# Import all core algorithms from quantum_40qubit_hypercube_engine
from quantum_40qubit_hypercube_engine import (
    QUBITS_40, TOTAL_AMPS_40, OCTANTS_COUNT_40, AMPS_PER_OCTANT,
    QUBITS_42, TOTAL_AMPS_42, FP4_LUT,
    decode_fp4_to_complex, encode_complex_to_fp4, verify_fp4_codec_lossless,
    build_unitary_byte_lut, print_banner, verify_hardware_limits,
    run_40qubit_architecture_simulation, OCTANT_CONFIGS,
    allocate_vram_buffer_plane, compute_chunk_hash, init_octant_pattern,
    apply_gpu_xk_inplace, apply_cswap_fredkin, apply_continuous_unitary_lut,
    generate_40qubit_sonification, run_40qubit_hypercube_gauntlet,
    generate_markdown_report
)

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI PRIME // 40-Qubit & 42-Qubit Hyper-Cube Colab Runner")
    parser.add_argument("--scaled", action="store_true", help="Force scaled hardware mode")
    args = parser.parse_args()

    results = run_40qubit_hypercube_gauntlet(scaled=args.scaled)

    # In-Notebook Audio Display trigger
    try:
        from IPython.display import Audio, display
        wav_path = "artifacts/quantum_sonification_40qubit.wav"
        if os.path.exists(wav_path):
            print("  [*] Rendering Colab Interactive Audio Player...")
            display(Audio(wav_path, autoplay=False))
    except Exception:
        pass

if __name__ == "__main__":
    main()
