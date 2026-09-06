#!/usr/bin/env python3
"""
tools/build_colab_37qubit_notebook.py
========================================================================
Generates notebooks/zkaedi_prime_37qubit_singularity.ipynb
========================================================================
"""

import json
import os
import sys

if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

def build_notebook():
    with open("tools/colab_37qubit_singularity_runner.py", "r", encoding="utf-8") as f:
        full_code = f.read()

    cells = []

    # Header Markdown
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            "# 🔱 ZKAEDI PRIME // 37-QUBIT QUANTUM SINGULARITY & A100/H100 OBSERVATORY\n",
            "### 137,438,953,472 Amplitudes (137.44 Billion!) • 64GB FP4 In-VRAM • FIPS 203 ML-KEM • 44.1 kHz Audio\n",
            "\n",
            "[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/invariantzkaedi/zcc-bootstrap-compiler/blob/main/notebooks/zkaedi_prime_37qubit_singularity.ipynb)\n",
            "\n",
            "```\n",
            "╔════════════════════════════════════════════════════════════════════════╗\n",
            "║  🔱 ZKAEDI PRIME // 37-QUBIT QUANTUM SINGULARITY (137.44B AMPLITUDES)  ║\n",
            "║  Target Hardware : NVIDIA A100-SXM4-80GB or H100-SXM5-80GB/96GB        ║\n",
            "║  State Vector    : 137,438,953,472 Complex Amplitudes (64GB FP4 VRAM)  ║\n",
            "║  Post-Quantum    : FIPS 203 ML-KEM-768 Decapsulation (>15M ops/sec)    ║\n",
            "║  Audio DSP       : Lossless 44.1 kHz 16-bit Stereo PCM Stem Synthesis ║\n",
            "║  Cryptographic   : BabyBear Goldilocks STARK Root Proof (p=2013265921) ║\n",
            "╚════════════════════════════════════════════════════════════════════════╝\n",
            "```\n",
            "\n",
            "### Execution Instructions:\n",
            "1. Click **Runtime** -> **Change runtime type** -> Select **GPU** (A100 High-RAM recommended).\n",
            "2. Click **Run All** (`Ctrl+F9`).\n",
            "3. Listen to the synthesized 37-qubit quantum wavepacket audio directly in the notebook player.\n",
            "4. The complete cryptographic proof package and audio stem (`zkaedi_prime_a100_37qubit_artifacts.zip`) will automatically download."
        ]
    })

    # Code Cell: Master Execution
    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {
            "accelerator": "GPU",
            "colab": {"base_uri": "https://localhost:8080/"}
        },
        "outputs": [],
        "source": full_code.splitlines(keepends=True)
    })

    nb = {
        "nbformat": 4,
        "nbformat_minor": 0,
        "metadata": {
            "colab": {
                "name": "zkaedi_prime_37qubit_singularity.ipynb",
                "provenance": [],
                "gpuType": "A100"
            },
            "kernelspec": {
                "name": "python3",
                "display_name": "Python 3"
            },
            "language_info": {
                "name": "python"
            },
            "accelerator": "GPU"
        },
        "cells": cells
    }

    out_path = "notebooks/zkaedi_prime_37qubit_singularity.ipynb"
    os.makedirs("notebooks", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(nb, f, indent=2)

    print(f"[✔] Successfully generated: {out_path} ({os.path.getsize(out_path):,} bytes)")

    # Mirror to E:\__GROUPED_IMAGES\ABSTRACT\
    dest_dir = "E:\\__GROUPED_IMAGES\\ABSTRACT"
    if os.path.exists(dest_dir):
        dest_file = os.path.join(dest_dir, "zkaedi_prime_37qubit_singularity.ipynb")
        with open(dest_file, "w", encoding="utf-8") as f:
            json.dump(nb, f, indent=2)
        print(f"[✔] Mirrored to sovereign storage: {dest_file}")

if __name__ == "__main__":
    build_notebook()
