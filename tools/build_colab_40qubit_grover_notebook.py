#!/usr/bin/env python3
"""
tools/build_colab_40qubit_grover_notebook.py
========================================================================
Generates notebooks/zkaedi_prime_40qubit_grover.ipynb
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
    engine_path = "tools/quantum_40qubit_grover_engine.py"
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
            "# 🔱 ZKAEDI PRIME // 40-QUBIT GROVER CRYPTANALYTIC SEARCH ENGINE\n",
            "### 1,099,511,627,776 Amplitudes (40Q — 1.10 Trillion!) • Quadratic Quantum Speedup\n",
            "\n",
            "[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/invariantzkaedi/zcc-bootstrap-compiler/blob/main/notebooks/zkaedi_prime_40qubit_grover.ipynb)\n",
            "\n",
            "```\n",
            "╔════════════════════════════════════════════════════════════════════════╗\n",
            "║  🔱 ZKAEDI PRIME // 40Q GROVER AMPLITUDE AMPLIFICATION ENGINE          ║\n",
            "║  Target Hardware : NVIDIA A100-SXM4-80GB or H100-SXM5-80GB/96GB        ║\n",
            "║  Search Space    : 40 Qubits (1,099,511,627,776 Amplitudes)            ║\n",
            "║  Memory Staging  : 512-GiB logical state space represented by eight    ║\n",
            "║                    distinct sequentially staged octants                ║\n",
            "║  Phase Oracle    : O_f phase flip with localized octant targeting      ║\n",
            "║  Diffusion Op    : D = 2|psi><psi| - I via 32-MiB streaming windowing ║\n",
            "║  Step-1 Invariant: P_1 / P_0 = 9.00x exact physical mass amplification ║\n",
            "║  Speedup         : 667,547x faster than classical brute-force          ║\n",
            "║  Audio DSP       : Lossless 44.1 kHz 16-bit Stereo PCM Audio Synthesis ║\n",
            "╚════════════════════════════════════════════════════════════════════════╝\n",
            "```\n",
            "\n",
            "### Execution Instructions:\n",
            "1. Click **Runtime** -> **Change runtime type** -> Select **GPU** (A100 High-RAM 80GB recommended).\n",
            "2. Click **Run All** (`Ctrl+F9`).\n",
            "3. The engine verifies theoretical quadratic scaling, sets up initial uniform superposition,\n",
            "   executes localized phase inversion, applies streaming diffusion reflection about the mean,\n",
            "   verifies the exact 9.00x physical amplification invariant, and renders the audio sonification."
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
            "wav_path = 'artifacts/quantum_sonification_40qubit_grover.wav'\n",
            "if os.path.exists(wav_path):\n",
            "    display(Audio(wav_path, autoplay=False))\n"
        ]
    })

    nb = {
        "nbformat": 4,
        "nbformat_minor": 0,
        "metadata": {
            "colab": {
                "name": "zkaedi_prime_40qubit_grover.ipynb",
                "provenance": [],
                "gpuType": "A100"
            },
            "kernelspec": {
                "display_name": "Python 3",
                "name": "python3"
            },
            "language_info": {
                "name": "python"
            },
            "accelerator": "GPU"
        },
        "cells": cells
    }

    out_dir = "notebooks"
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "zkaedi_prime_40qubit_grover.ipynb")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(nb, f, indent=2)

    size_kb = os.path.getsize(out_path) / 1024
    print(f"[*] Successfully built '{out_path}' ({size_kb:.1f} KB, {len(cells)} cells).")

if __name__ == "__main__":
    build_notebook()
