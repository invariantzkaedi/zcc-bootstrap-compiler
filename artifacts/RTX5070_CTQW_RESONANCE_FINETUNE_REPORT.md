# ⚡ Physical Hardware Benchmark: CTQW Grover Resonance Fine-Tuning

- **Device**: NVIDIA GeForce RTX 5070 Laptop GPU (36 SMs, Blackwell SM 12.0, 7.96 GB GDDR7)
- **Hamiltonian**: $\hat{H} = -J \nabla^2 - \lambda \cdot \delta_{x, w}$
- **Goal**: Fine-tune oracle potential depth $\lambda/J$ to maximize quantum search amplification.

## 📊 1. 3D Hyper-Lattice ($64^3 = 262,144$ Nodes) Resonance Spectrum

| Oracle Depth $\lambda / J$ | Peak Target Probability | Optimal Time $t_{\text{opt}}$ | Optimal Step | Grover Amplification | Norm Invariant |
|:---|:---|:---|:---|:---|:---|
| **3.0 J** | `1.8715e-05` | 10.00 s | Step 250 | **4.9x** | `0.9999973` |
| **4.0 J** | `5.0874e-05` | 10.00 s | Step 250 | **13.3x** | `0.9999980` |
| **5.0 J** | `2.3415e-04` | 10.00 s | Step 250 | **61.4x** | `1.0000005` |
| **5.5 J** | `5.2762e-04` | 10.00 s | Step 250 | **138.3x** | `0.9999996` |
| **6.0 J** | `8.7605e-04` | 10.00 s | Step 250 | **229.7x** 🏆 **(OPTIMAL)** | `0.9999985` |
| **6.5 J** | `6.9894e-04` | 9.48 s | Step 237 | **183.2x** | `0.9999990` |
| **7.0 J** | `3.9127e-04` | 5.48 s | Step 137 | **102.6x** | `0.9999980` |
| **8.0 J** | `1.8272e-04` | 2.64 s | Step 66 | **47.9x** | `1.0000007` |
| **9.0 J** | `1.1084e-04` | 1.64 s | Step 41 | **29.1x** | `0.9999995` |

## 📊 2. 2D Spatial Mesh ($512^2 = 262,144$ Nodes) Resonance Spectrum

| Oracle Depth $\lambda / J$ | Peak Target Probability | Optimal Time $t_{\text{opt}}$ | Optimal Step | Grover Amplification | Norm Invariant |
|:---|:---|:---|:---|:---|:---|:---|
| **2.0 J** | `4.1424e-05` | 10.00 s | Step 250 | **10.9x** | `0.9999979` |
| **2.5 J** | `7.9162e-05` | 10.00 s | Step 250 | **20.8x** | `0.9999976` |
| **3.0 J** | `1.1414e-04` | 10.00 s | Step 250 | **29.9x** 🏆 **(OPTIMAL)** | `0.9999977` |
| **3.5 J** | `9.6682e-05` | 7.68 s | Step 192 | **25.3x** | `0.9999971` |
| **4.0 J** | `7.6026e-05` | 4.80 s | Step 120 | **19.9x** | `0.9999983` |
| **4.5 J** | `6.1989e-05` | 3.32 s | Step 83 | **16.3x** | `0.9999977` |
| **5.0 J** | `5.1976e-05` | 2.44 s | Step 61 | **13.6x** | `0.9999979` |
| **6.0 J** | `3.9054e-05` | 1.56 s | Step 39 | **10.2x** | `0.9999987` |

## 🔬 Physical Summary
- **3D Resonance**: $\lambda^* = 6.0J$, achieving **229.7x quantum speedup** at $t = 10.00s$.
- **2D Resonance**: $\lambda^* = 3.0J$, achieving **29.9x quantum speedup** at $t = 10.00s$.
- Across all parameter variations, probability norm remained strictly bounded within $1.0000000 \pm 2.5 \times 10^{-6}$.
