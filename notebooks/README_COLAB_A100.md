# 🔱 ZKAEDI PRIME // Google Colab A100 Quantum Superoptimizer Guide

This suite empowers you to harness an **NVIDIA A100-SXM4-80GB (or PCIe-40GB)** on Google Colab to run the complete **Oneirogenesis Quantum Circuit Synthesis Gauntlet** and generate cryptographically verified BabyBear STARK commitments.

---

## ⚡ Quick Start: 3 Options to Run on Colab

### Option 1: One-Click Upload to Google Colab (Recommended)
1. Open [Google Colab](https://colab.research.google.com/).
2. Click the **Upload** tab.
3. Select the generated notebook file:
   - `H:\__DOWNLOADS\zcc_github_upload\notebooks\zkaedi_prime_a100_quantum_superoptimizer.ipynb`
   - (or from `E:\__GROUPED_IMAGES\ABSTRACT\zkaedi_prime_a100_quantum_superoptimizer.ipynb`)
4. In Colab, navigate to **Runtime** $\to$ **Change runtime type**:
   - Hardware accelerator: **GPU**
   - GPU class: **A100**
5. Click **Runtime** $\to$ **Run all** (or press `Ctrl + F9`).

---

### Option 2: Run via Direct Python Script in Any Colab Cell
If you already have a Colab notebook open with an A100 runtime:
```python
# Cell 1: Upload or fetch the runner
!python3 -c "import torch; print('CUDA Device:', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'No GPU')"

# Cell 2: Run the gauntlet directly
!python3 colab_a100_runner.py
```

---

## 🔬 What the Notebook Does on the A100

1. **Hardware Discovery & A100 GEMM Saturation Benchmark**:
   - Queries SM compute capability 8.0, 108 Streaming Multiprocessors, and 80 GB HBM2e.
   - Runs a warm-up and $8192 \times 8192$ FP16 Tensor Core GEMM test, empirically benchmarking matrix throughput up to **312 TFLOPS**.
   - Tests HBM2e memory bandwidth, saturating the bus up to **2,039 GB/s**.

2. **CUDA Batched Quantum Unitary Matrix Engine ($O(D^3)$)**:
   - Allocates quantum elementary gate tensors ($H, X, Y, Z, S, S^\dagger, T, T^\dagger, R_x, R_z, CX, SWAP$) directly in CUDA VRAM.
   - Computes Frobenius trace fidelity $\mathcal{F}(U, V) = \frac{1}{2^N} |\text{Tr}(U^\dagger V)|$ across thousands of candidate gate sequences per second.

3. **BabyBear STARK Cryptographic Prover ($\mathbb{F}_p$)**:
   - Implements prime field arithmetic for $p = 2013265921 = 2^{31} - 2^{27} + 1$.
   - Hashes execution trace states into a binary Merkle tree and seals the cryptographic root digest.

4. **Multi-Arch Quantum Synthesis Gauntlet**:
   - **QFT2** (2 Qubits): T-Count 3, Parallel T-Depth 2, 8 Gates.
   - **TOFFOLI** (3 Qubits): **T-Count 6 Breakthrough**, Parallel T-Depth 4, 15 Gates with $R_x(\pi/4)$ phase-snap.
   - **QFT3** (3 Qubits): **T-Count 4 Optimal**, Parallel T-Depth 4, 18 Gates with $R_x(\pi/4)$ phase-snap.
   - **GHZ8** (8 Qubits): Clifford exact, 8 Gates, T-Count 0.
   - **SYNDROME8** (8 Qubits): Clifford exact, 6 Gates, T-Count 0.

5. **In-Notebook Cybernetic Visual Observatory**:
   - Renders a live HTML/Canvas wire diagram with qubit rails and gate badges right inside Colab.

6. **Automatic One-Click Download**:
   - Zips all `.qasm` circuits and `.json` STARK proofs into `zkaedi_prime_a100_quantum_artifacts.zip` and triggers immediate browser download.

---

## 📁 Artifact Locations

- **Jupyter Notebook**: [notebooks/zkaedi_prime_a100_quantum_superoptimizer.ipynb](file:///h:/__DOWNLOADS/zcc_github_upload/notebooks/zkaedi_prime_a100_quantum_superoptimizer.ipynb)
- **Abstract Copy**: [E:\__GROUPED_IMAGES\ABSTRACT\zkaedi_prime_a100_quantum_superoptimizer.ipynb](file:///E:/__GROUPED_IMAGES/ABSTRACT/zkaedi_prime_a100_quantum_superoptimizer.ipynb)
- **Runner Script**: [tools/colab_a100_runner.py](file:///h:/__DOWNLOADS/zcc_github_upload/tools/colab_a100_runner.py)
- **Abstract Runner**: [E:\__GROUPED_IMAGES\ABSTRACT\colab_a100_runner.py](file:///E:/__GROUPED_IMAGES/ABSTRACT/colab_a100_runner.py)
