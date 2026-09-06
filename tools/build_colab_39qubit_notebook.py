#!/usr/bin/env python3
"""
tools/build_colab_39qubit_notebook.py
========================================================================
Generates notebooks/zkaedi_prime_39qubit_hyperslab.ipynb
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
    engine_path = "tools/quantum_39qubit_hyperslab_engine.py"
    if not os.path.exists(engine_path):
        engine_path = "tools/colab_39qubit_hyperslab_runner.py"
    if not os.path.exists(engine_path):
        print("[!] 'tools/quantum_39qubit_hyperslab_engine.py' not found in active directory.")
        print("[*] If running in Google Colab, please paste the contents of 'tools/colab_39qubit_hyperslab_runner.py'")
        print("    directly into this cell, or clone the repository first:")
        print("    !git clone https://github.com/invariantzkaedi/zcc-bootstrap-compiler.git && cd zcc-bootstrap-compiler")
        return

    with open(engine_path, "r", encoding="utf-8") as f:
        full_code = f.read()

    cells = []

    # Cell 1: Header Markdown with Colab Badge & Hardware Guide
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            "# 🔱 ZKAEDI PRIME // 39-QUBIT & 40-QUBIT QUANTUM HYPER-SLAB ENGINE\n",
            "### 549.76 Billion (39Q) & 1.10 Trillion (40Q) Amplitudes • Multi-Qubit Permutations & Controlled Reversible Circuits\n",
            "\n",
            "[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/invariantzkaedi/zcc-bootstrap-compiler/blob/main/notebooks/zkaedi_prime_39qubit_hyperslab.ipynb)\n",
            "\n",
            "```\n",
            "╔════════════════════════════════════════════════════════════════════════╗\n",
            "║  🔱 ZKAEDI PRIME // 39Q & 40Q QUANTUM HYPER-SLAB & HYPER-CUBE ENGINE   ║\n",
            "║  Target Hardware : NVIDIA A100-SXM4-80GB or H100-SXM5-80GB/96GB        ║\n",
            "║  State Vectors   : 39Q (549.76B Amps / 4 Slabs) • 40Q (1.10T / 8 Slabs)║\n",
            "║  Semantic Gates  : Full Multi-Qubit Pauli-X Gauntlet (q0..q3, q37, q38)║\n",
            "║  Controlled Gates: CX(q37->q0), CCX(q38,q37->q0), CSWAP(q37->q0,q1)    ║\n",
            "║  Verification    : Dual Invariant: GPU H1 == Ref H1 AND H2 == H0 (G²=I)║\n",
            "║  Audio DSP       : Lossless 44.1 kHz 16-bit Stereo PCM Audio Synthesis ║\n",
            "╚════════════════════════════════════════════════════════════════════════╝\n",
            "```\n",
            "\n",
            "### Execution Instructions:\n",
            "1. Click **Runtime** -> **Change runtime type** -> Select **GPU** (A100 High-RAM 80GB recommended).\n",
            "2. Click **Run All** (`Ctrl+F9`).\n",
            "3. The engine sequentially stages the 37-qubit super-slabs (64 GiB each),\n",
            "   executes the multi-qubit and controlled reversible gates, and verifies CPU reference oracles and involution restoration."
        ]
    })

    # Cell 2: In-Notebook Audio Player & Execution Code
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

    # Cell 3: Markdown / Audio Player Render
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
            "wav_path = 'artifacts/quantum_sonification_39qubit.wav'\n",
            "if os.path.exists(wav_path):\n",
            "    display(Audio(wav_path, autoplay=False))\n"
        ]
    })

    nb = {
        "nbformat": 4,
        "nbformat_minor": 0,
        "metadata": {
            "colab": {
                "name": "zkaedi_prime_39qubit_hyperslab.ipynb",
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

    out_path = "notebooks/zkaedi_prime_39qubit_hyperslab.ipynb"
    os.makedirs("notebooks", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(nb, f, indent=2)

    print(f"[✔] Successfully generated: {out_path} ({os.path.getsize(out_path):,} bytes)")

    dest_dir = "E:\\__GROUPED_IMAGES\\ABSTRACT"
    if os.path.exists(dest_dir):
        dest_file = os.path.join(dest_dir, "zkaedi_prime_39qubit_hyperslab.ipynb")
        with open(dest_file, "w", encoding="utf-8") as f:
            json.dump(nb, f, indent=2)
        print(f"[✔] Mirrored to sovereign storage: {dest_file}")

if __name__ == "__main__":
    build_notebook()
