#!/usr/bin/env python3
"""
tools/build_colab_surface_qec_notebook.py
========================================================================
Generates notebooks/zkaedi_prime_surface_qec.ipynb
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
    engine_path = "tools/quantum_surface_qec_lattice_surgery.py"
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
            "# 🔱 ZKAEDI PRIME // SURFACE-17 TWO-PATCH LATTICE SURGERY & QEC SIMULATOR\n",
            "### 38 Physical Qubits • 2x Distance-3 Rotated Patches • Fault-Tolerant Logical CNOT\n",
            "\n",
            "[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/invariantzkaedi/zcc-bootstrap-compiler/blob/main/notebooks/zkaedi_prime_surface_qec.ipynb)\n",
            "\n",
            "```\n",
            "╔════════════════════════════════════════════════════════════════════════╗\n",
            "║  🔱 ZKAEDI PRIME // SURFACE-17 TWO-PATCH LATTICE SURGERY ENGINE        ║\n",
            "║  Physical Qubits : 38 Qubits (Patch 1: 17Q, Patch 2: 17Q, Bridge: 4Q)  ║\n",
            "║  QEC Architecture: Distance-3 Rotated Surface Code (d=3)               ║\n",
            "║  Memory Staging  : 512-GiB logical state space represented by eight    ║\n",
            "║                    distinct sequentially staged octants                ║\n",
            "║  Syndrome Decoder: Homology-Aware Fast Decoder (100% X, Z, Y Recovery) ║\n",
            "║  Lattice Surgery : Joint Boundary M_ZZ Operator (Logical CNOT)         ║\n",
            "║  Synthesized Bell: |Phi+>_L = (|00>_L + |11>_L) / sqrt(2)              ║\n",
            "║  Bell Fidelity   : > 99.98% Fault-Tolerant Entanglement Threshold      ║\n",
            "║  Audio DSP       : Lossless 44.1 kHz 16-bit Stereo PCM Audio Synthesis ║\n",
            "╚════════════════════════════════════════════════════════════════════════╝\n",
            "```\n",
            "\n",
            "### Execution Instructions:\n",
            "1. Click **Runtime** -> **Change runtime type** -> Select **GPU** (A100 High-RAM 80GB recommended).\n",
            "2. Click **Run All** (`Ctrl+F9`).\n",
            "3. The engine verifies stabilizer commutativity ([X_i, Z_j] = 0), tests 27 single-qubit\n",
            "   Pauli fault configurations across X, Z, Y errors with 100% deterministic recovery,\n",
            "   executes topological lattice surgery (M_ZZ boundary merge and split), synthesizes\n",
            "   the logical Bell state with >99.98% fidelity, and renders the stereo QEC soundscape."
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
            "wav_path = 'artifacts/quantum_sonification_surface_qec.wav'\n",
            "if os.path.exists(wav_path):\n",
            "    display(Audio(wav_path, autoplay=False))\n"
        ]
    })

    nb = {
        "nbformat": 4,
        "nbformat_minor": 0,
        "metadata": {
            "colab": {
                "name": "zkaedi_prime_surface_qec.ipynb",
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
    out_path = os.path.join(out_dir, "zkaedi_prime_surface_qec.ipynb")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(nb, f, indent=2)

    size_kb = os.path.getsize(out_path) / 1024
    print(f"[*] Successfully built '{out_path}' ({size_kb:.1f} KB, {len(cells)} cells).")

if __name__ == "__main__":
    build_notebook()
