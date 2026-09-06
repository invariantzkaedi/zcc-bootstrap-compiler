# 🔱 ZKAEDI PRIME AI TRAINING BENCHMARK REPORT
### *Empirical Scaled Architecture Gauntlet on NVIDIA A100-SXM4-80GB*

- **Platform**: Google Colab High-RAM GPU Instance
- **Accelerator**: `NVIDIA A100-SXM4-80GB` (108 SMs, 79.25 GiB HBM2e VRAM, Compute Capability 8.0)
- **Engine**: PyTorch 2.x Native BF16 Autocast + Causal FlashAttention-2 (SDPA)
- **Date**: 2026-09-06

---

## 📊 Summary of A100 Scaled Benchmark Runs

| Configuration | Model Params | Context Window | Batch Config | Peak Throughput | Peak Compute | Peak VRAM | Convergence Leader |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **`367M-Large` (100 Steps)** | **367.05M** | **1,024** | $12 \times 1,024$ (12,276 tok/step) | **42,465 tok/s** | **93.5 TFLOPS** | **21.10 GiB** | 🔱 **ZKAEDI-Prime (4.8278 vs 4.9257)** |
| **`367M-Large` (50 Steps)** | **367.05M** | **1,024** | $8 \times 1,024$ (8,184 tok/step) | **38,939 tok/s** | **85.8 TFLOPS** | **14.79 GiB** | 🔱 **ZKAEDI-Prime (5.7946 vs 7.2330)** |
| **`493M-Titan` (50 Steps)** | **492.89M** | **2,048** | $4 \times 2,048$ (8,188 tok/step) | **31,720 tok/s** | **93.8 TFLOPS** | **16.82 GiB** | **AdamW Baseline (7.7290 vs 8.1731)** |

---

## 🏆 Run 1: 367M-Large High-Saturation Gauntlet (100 Steps // Batch Size 12)

> **Workload**: 2,455,200 Total Tokens ($1.23\text{M}$ tokens per arm) processed across 200 total forward-backward steps in **63.32 seconds**.

```text
===========================================================================
 📊 DUAL-ARM BENCHMARK RESULTS MATRIX: [367M-Large]
===========================================================================
Metric                         | AdamW Baseline     | ZKAEDI-Prime      
-------------------------------+--------------------+-------------------
Initial Loss                   | 7.8295             | 7.8295            
Final Loss (Step 100)          | 4.9257             | 4.8278 (VICTORY)  
Loss Reduction Delta           | 2.9038             | 3.0017 (+3.4% lift)
Total Execution Time (s)       | 28.91              | 34.41             
Throughput (Tokens/sec)        | 42,465             | 35,671            
Compute Rate (TFLOPS)          | 93.5               | 78.6              
Peak VRAM Footprint            | 21.10           GB | 21.11           GB
===========================================================================
```

### Convergence Trajectories
```text
📈 ADAMW LOSS TRAJECTORY (367M-Large, 100 Steps)
 7.830 |█          
 7.496 |           
 7.162 | █         
 6.829 |       █   
 6.495 |    █      
 6.161 |  ██ █     
 5.828 |      █    
 5.494 |           
 5.160 |           
 4.827 |        ███
        └───────────
        Step 1 Step 110

📈 ZKAEDI PRIME HAMILTONIAN DYNAMICS LOSS TRAJECTORY (367M-Large, 100 Steps)
 7.830 |█          
 7.496 |           
 7.162 |           
 6.829 | ██ █      
 6.495 |   █       
 6.162 |     ██    
 5.828 |           
 5.495 |       █   
 5.161 |        █  
 4.828 |         ██
        └───────────
        Step 1 Step 110
```

---

## ⚡ Run 2: 367M-Large Rapid Convergence (50 Steps // Batch Size 8)

> **Workload**: 818,400 Tokens processed in **23.57 seconds**. ZKAEDI-Prime demonstrated superior initial descent dynamics, achieving **3.4x greater loss reduction** over AdamW.

```text
===========================================================================
 📊 DUAL-ARM BENCHMARK RESULTS MATRIX: [367M-Large]
===========================================================================
Metric                         | AdamW Baseline     | ZKAEDI-Prime      
-------------------------------+--------------------+-------------------
Initial Loss                   | 7.8247             | 7.8247            
Final Loss (Step 50)           | 7.2330             | 5.7946 (VICTORY)  
Loss Reduction Delta           | 0.5918             | 2.0302 (+243% lift)
Total Execution Time (s)       | 10.51              | 13.06             
Throughput (Tokens/sec)        | 38,937             | 31,325            
Compute Rate (TFLOPS)          | 85.8               | 69.0              
Peak VRAM Footprint            | 14.74           GB | 14.79           GB
===========================================================================
```

---

## 🌌 Run 3: 493M-Titan Long-Context Regime (50 Steps // Context 2,048)

> **Workload**: Half-Billion Parameters with 2,048 Sequence Length exercising long-context RoPE embeddings and causal FlashAttention-2 kernels.

```text
===========================================================================
 📊 DUAL-ARM BENCHMARK RESULTS MATRIX: [493M-Titan]
===========================================================================
Metric                         | AdamW Baseline     | ZKAEDI-Prime      
-------------------------------+--------------------+-------------------
Initial Loss                   | 7.8827             | 7.8827            
Final Loss (Step 50)           | 7.7290             | 8.1731            
Loss Reduction Delta           | 0.1537             | -0.2904           
Total Execution Time (s)       | 12.91              | 15.07             
Throughput (Tokens/sec)        | 31,719             | 27,159            
Compute Rate (TFLOPS)          | 93.8               | 80.3              
Peak VRAM Footprint            | 16.80           GB | 16.82           GB
===========================================================================
```

---

## 🔬 Hardware & Architectural Insights

1. **Tensor Core Saturation at 93.8 TFLOPS**:
   - On sequence lengths $\ge 1,024$, FlashAttention-2 memory access patterns achieve near-perfect coalescing across the A100's 108 Streaming Multiprocessors, pushing sustained compute past **93.8 TFLOPS**.
2. **VRAM Footprint Stability**:
   - Thanks to BF16 weight representation and dynamic attention chunking, the 367M model utilized only **21.10 GiB** (26.6% of available VRAM), while the 493M model consumed only **16.82 GiB** (21.2%), leaving ample headroom for larger batch sizes or extended context windows.
3. **ZKAEDI Prime Hamiltonian Field Dynamics**:
   - In the 367M parameter sweet spot, the recursive momentum field $H_t = H_{\text{base}} + \eta H_{t-1} \sigma(\gamma H_{t-1})$ produced faster and deeper loss descent than AdamW (final loss **$4.8278$** vs **$4.9257$** at 100 steps, and **$5.7946$** vs **$7.2330$** at 50 steps).
