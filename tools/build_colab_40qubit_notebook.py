#!/usr/bin/env python3
"""
tools/build_colab_40qubit_notebook.py
========================================================================
Generates notebooks/zkaedi_prime_40qubit_hypercube.ipynb
========================================================================
"""

import json
import os
import sys
import shutil

if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

def build_notebook():
    engine_path = "tools/quantum_40qubit_hypercube_engine.py"
    if not os.path.exists(engine_path):
        print(f"[!] '{engine_path}' not found.")
        return

    with open(engine_path, "r", encoding="utf-8") as f:
        full_code = f.read()

    cells = []

    # Cell 1: Header Markdown with Colab Badge & Hardware Guide
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            "# 🔱 ZKAEDI PRIME // 40-QUBIT & 42-QUBIT HYPER-CUBE QUANTUM ENGINE\n",
            "### 1,099,511,627,776 Amplitudes (40Q — 1.10 Trillion!) • 4,398,046,511,104 Amplitudes (42Q — 4.40 Trillion!)\n",
            "\n",
            "[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/invariantzkaedi/zcc-bootstrap-compiler/blob/main/notebooks/zkaedi_prime_40qubit_hypercube.ipynb)\n",
            "\n",
            "```\n",
            "╔════════════════════════════════════════════════════════════════════════╗\n",
            "║  🔱 ZKAEDI PRIME // 40Q & 42Q HYPER-CUBE QUANTUM ENGINE                ║\n",
            "║  Target Hardware : NVIDIA A100-SXM4-80GB or H100-SXM5-80GB/96GB        ║\n",
            "║  Logical State   : 40Q (1.10 Trillion Amps / 8 Octant Super-Slabs)     ║\n",
            "║  Scaling Frontier: 42Q (4.40 Trillion Amps / 32 Super-Slabs)           ║\n",
            "║  Semantic Gates  : Full 7-Stage Multi-Qubit Gauntlet (q0..q3, q37..q39)║\n",
            "║  Controlled Gates: CX, CCX (Toffoli), CCCX (Triple-Toffoli), CSWAP     ║\n",
            "║  Unitary Codec   : 2b Re + 2b Im FP4 Complex Superposition (H, S, T, Rx)║\n",
            "║  Verification    : Dual Invariant: GPU H1 == Ref H1 AND H2 == H0 (U†U=I)║\n",
            "║  Audio DSP       : Lossless 44.1 kHz 16-bit Stereo PCM Audio Synthesis ║\n",
            "╚════════════════════════════════════════════════════════════════════════╝\n",
            "```\n",
            "\n",
            "### Execution Instructions:\n",
            "1. Click **Runtime** -> **Change runtime type** -> Select **GPU** (A100 High-RAM 80GB recommended).\n",
            "2. Click **Run All** (`Ctrl+F9`).\n",
            "3. The engine sequentially stages the 8 octant super-slabs (64 GiB each, 512 GiB net state space),\n",
            "   executes the multi-qubit permutations, multi-controlled reversible circuits (including Triple-Controlled Toffoli),\n",
            "   and continuous unitaries with the streaming FP4 complex codec."
        ]
    })

    # Cell 2: Execution Code
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

    # Cell 3: Audio Player Render
    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {},
        "outputs": [],
        "source": [
            "# In-Notebook Audio Player\n",
            "from IPython.display import Audio, display\n",
            "import os\n",
            "\n",
            "wav_path = 'artifacts/quantum_sonification_40qubit.wav'\n",
            "if os.path.exists(wav_path):\n",
            "    display(Audio(wav_path, autoplay=False))\n"
        ]
    })

    nb = {
        "nbformat": 4,
        "nbformat_minor": 0,
        "metadata": {
            "colab": {
                "name": "zkaedi_prime_40qubit_hypercube.ipynb",
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

    out_path = "notebooks/zkaedi_prime_40qubit_hypercube.ipynb"
    os.makedirs("notebooks", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(nb, f, indent=2)

    print(f"[*] Built: {out_path} ({os.path.getsize(out_path):,} bytes)")

if __name__ == "__main__":
    build_notebook()
