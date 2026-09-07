#!/usr/bin/env python3
"""
tools/build_colab_chem_notebook.py
========================================================================
Compiles notebooks/zkaedi_prime_quantum_chemistry_vqe.ipynb
Zero-dependency interactive Google Colab notebook for the
Quantum Chemistry & Material Discovery Hyper-Slab Engine.
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
    engine_path = "tools/quantum_chemistry_hyperslab_engine.py"
    if not os.path.exists(engine_path):
        print(f"[!] '{engine_path}' not found.")
        return

    with open(engine_path, "r", encoding="utf-8") as f:
        full_code = f.read()

    cells = []

    # Cell 1: Header Markdown with Colab Badge & Overview
    cells.append({
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            "# 🔱 ZKAEDI PRIME // QUANTUM CHEMISTRY & MATERIAL DISCOVERY HYPER-SLAB ENGINE\n",
            "### Complete Active Space (CASSCF) • Unitary Coupled Cluster (UCCSD) • Variational Quantum Eigensolver (VQE)\n",
            "#### Exact Multi-Reference Correlation • N2 Triple Bond Dissociation • 36Q–40Q Industrial Active Space Sizing\n",
            "\n",
            "[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/invariantzkaedi/zcc-bootstrap-compiler/blob/main/notebooks/zkaedi_prime_quantum_chemistry_vqe.ipynb)\n",
            "\n",
            "```\n",
            "╔════════════════════════════════════════════════════════════════════════╗\n",
            "║  🔱 ZKAEDI PRIME // QUANTUM CHEMISTRY & MATERIAL DISCOVERY ENGINE      ║\n",
            "║  Target Hardware : NVIDIA A100-SXM4-80GB / H100 or CPU Classical Host  ║\n",
            "║  Fermionic Basis : Second Quantization Jordan-Wigner POPCNT Parity     ║\n",
            "║  Ansatz Model    : Unitary Coupled Cluster with Singles & Doubles      ║\n",
            "║  Precision Target: Chemical Accuracy Threshold (< 1.5936 mHa / 1 kcal) ║\n",
            "║  Active Spaces   : H2 (4Q), H4 (8Q), N2 (12Q), Li2S4 (32Q), FeMo-co (40Q)║\n",
            "║  Sonification    : 44.1 kHz Stereo PCM Audio Molecular Orbitals        ║\n",
            "╚════════════════════════════════════════════════════════════════════════╝\n",
            "```\n",
            "\n",
            "### How to Run on Google Colab:\n",
            "1. Click **Runtime** -> **Change runtime type** -> Select **GPU** (or standard CPU).\n",
            "2. Click **Run All** (`Ctrl+F9`).\n",
            "3. The engine automatically runs the Full-CI vs Hartree-Fock vs UCCSD-VQE benchmarks, displays chemical accuracy plots, and plays the audio stem."
        ]
    })

    # Cell 2: In-Notebook Execution Code
    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {
            "cellView": "form",
            "id": "chem_engine_cell"
        },
        "outputs": [],
        "source": [
            "# @title ⚡ Execute Quantum Chemistry & Material Discovery Gauntlet\n",
            "# Automatically creates directories, executes VQE and renders audio & plots\n",
            "\n",
            "import os, sys\n",
            "os.makedirs('artifacts', exist_ok=True)\n",
            "\n",
            full_code + "\n",
            "\n",
            "# Display Audio in Notebook\n",
            "from IPython.display import Audio, display\n",
            "if os.path.exists('artifacts/quantum_chemistry_vqe_sonification.wav'):\n",
            "    display(Audio('artifacts/quantum_chemistry_vqe_sonification.wav'))\n"
        ]
    })

    # Cell 3: Interactive Potential Energy & Chemical Accuracy Visualization
    cells.append({
        "cell_type": "code",
        "execution_count": None,
        "metadata": {
            "cellView": "form",
            "id": "plot_cell"
        },
        "outputs": [],
        "source": [
            "# @title 📊 Interactive Chemical Energy Surface & Industrial Scaling Plotter\n",
            "import json\n",
            "import matplotlib.pyplot as plt\n",
            "\n",
            "if os.path.exists('artifacts/quantum_chemistry_metrics.json'):\n",
            "    with open('artifacts/quantum_chemistry_metrics.json', 'r') as f:\n",
            "        data = json.load(f)\n",
            "    \n",
            "    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))\n",
            "    plt.style.use('dark_background')\n",
            "    \n",
            "    # Subplot 1: N2 Potential Energy Curve\n",
            "    n2_pts = data['n2_dissociation_curve']\n",
            "    r_vals = [pt['R_angstrom'] for pt in n2_pts]\n",
            "    e_hf = [pt['e_hf'] for pt in n2_pts]\n",
            "    e_fci = [pt['e_fci'] for pt in n2_pts]\n",
            "    e_vqe = [pt['e_vqe'] for pt in n2_pts]\n",
            "    \n",
            "    ax1.plot(r_vals, e_hf, 'r--', label='Hartree-Fock (Diverges)', linewidth=2)\n",
            "    ax1.plot(r_vals, e_fci, 'g-', label='Full-CI Exact Ground Truth', linewidth=2.5)\n",
            "    ax1.plot(r_vals, e_vqe, 'c^', label='UCCSD-VQE (Hyper-Slab)', markersize=8)\n",
            "    ax1.set_xlabel('N-N Bond Length (Å)', fontsize=12)\n",
            "    ax1.set_ylabel('Electronic Energy (Hartree)', fontsize=12)\n",
            "    ax1.set_title('N2 Dinitrogen Triple Bond Dissociation Curve', fontsize=14, color='#00ffcc')\n",
            "    ax1.grid(True, alpha=0.3)\n",
            "    ax1.legend(loc='upper right', fontsize=11)\n",
            "    \n",
            "    # Subplot 2: Industrial Target Determinants Scaling\n",
            "    targets = data['industrial_targets']\n",
            "    names = [t['formula'] for t in targets]\n",
            "    qubits = [t['qubits'] for t in targets]\n",
            "    vram = [t['mem_fp4_gb'] for t in targets]\n",
            "    \n",
            "    bars = ax2.bar(names, vram, color=['#3399ff', '#ff9933', '#cc33ff', '#00ff99'], width=0.5)\n",
            "    ax2.set_ylabel('FP4 State Space Footprint (GiB)', fontsize=12)\n",
            "    ax2.set_title('Multi-Orbital Active Spaces (32Q -> 40Q)', fontsize=14, color='#ffcc00')\n",
            "    for bar, q in zip(bars, qubits):\n",
            "        yval = bar.get_height()\n",
            "        ax2.text(bar.get_x() + bar.get_width()/2.0, yval + 10, f'{q} Qubits\\n({yval:.1f} GiB)', ha='center', va='bottom', color='white', fontsize=10)\n",
            "    ax2.grid(True, alpha=0.3, axis='y')\n",
            "    \n",
            "    plt.tight_layout()\n",
            "    plt.show()\n",
            "else:\n",
            "    print('[!] Run the previous cell first to generate metrics.')\n"
        ]
    })

    notebook_content = {
        "cells": cells,
        "metadata": {
            "accelerator": "GPU",
            "colab": {
                "provenance": []
            },
            "kernelspec": {
                "display_name": "Python 3",
                "name": "python3"
            },
            "language_info": {
                "name": "python"
            }
        },
        "nbformat": 4,
        "nbformat_minor": 0
    }

    os.makedirs("notebooks", exist_ok=True)
    out_path = "notebooks/zkaedi_prime_quantum_chemistry_vqe.ipynb"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(notebook_content, f, indent=1)

    print(f"[✔] Compiled Notebook: {out_path} ({os.path.getsize(out_path):,} bytes)")

    # Mirror to local directory if available
    mirror_dir = r"E:\__GROUPED_IMAGES\ABSTRACT"
    if os.path.exists(mirror_dir):
        mirror_dest = os.path.join(mirror_dir, "zkaedi_prime_quantum_chemistry_vqe.ipynb")
        shutil.copyfile(out_path, mirror_dest)
        print(f"[✔] Mirrored to: {mirror_dest}")

if __name__ == "__main__":
    build_notebook()
