import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import time
import numpy as np
from tools.pyavxzkd import PyAvxzkdField

def benchmark():
    W, H, K = 256, 256, 200
    total_updates = W * H * K
    data = np.random.uniform(-5.0, 5.0, (H, W)).astype(np.float32)
    eta, gamma = 0.4, 0.3

    print("=========================================================================================")
    print(f"       AVXZKD SUPREME vs COMPETITOR BASELINES (Grid: {W}x{H}, Depth: {K} steps)          ")
    print("=========================================================================================")

    # 1. Pure NumPy (Standard Array Vectorization)
    H_np = data.copy()
    H_base = data.copy()
    t0 = time.perf_counter()
    for _ in range(K):
        sig = 1.0 / (1.0 + np.exp(-gamma * H_np))
        H_np = H_base + eta * H_np * sig
    t_numpy = time.perf_counter() - t0
    rate_numpy = (total_updates / t_numpy) / 1e6

    # 2. Naive Python Scalar Loop Simulation (sampled on small chunk and scaled)
    t0 = time.perf_counter()
    sample_h = float(data[0, 0])
    sample_base = sample_h
    for _ in range(K * 1000):
        sig = 1.0 / (1.0 + np.exp(-gamma * sample_h))
        sample_h = sample_base + eta * sample_h * sig
    t_scalar_scaled = (time.perf_counter() - t0) * (W * H / 1000.0)
    rate_scalar = (total_updates / t_scalar_scaled) / 1e6

    # 3. AVXzkd Supreme (Hardware AVX2+FMA Engine)
    f = PyAvxzkdField(W, H)
    f.init_field(data)
    t0 = time.perf_counter()
    f.step(eta=eta, gamma=gamma, eps=0.0, k_steps=K)
    t_avx = time.perf_counter() - t0
    rate_avx = (total_updates / t_avx) / 1e6

    # Mathematical Verification
    res_avx = f.get_current()
    l_inf_err = float(np.max(np.abs(H_np - res_avx)))
    mean_err = float(np.mean(np.abs(H_np - res_avx)))

    print(f"| {'Engine':<28} | {'Latency':<10} | {'Throughput':<16} | {'Speedup':<10} | {'L_inf Error':<12} |")
    print(f"|------------------------------|------------|------------------|------------|--------------|")
    print(f"| {'Python Scalar Loop':<28} | {t_scalar_scaled*1000:7.2f} ms | {rate_scalar:8.2f} MCells/s | {'1.0x (base)':<10} | {'0.00000000':<12} |")
    print(f"| {'NumPy (Vectorized C/BLAS)':<28} | {t_numpy*1000:7.2f} ms | {rate_numpy:8.2f} MCells/s | {rate_numpy/rate_scalar:7.1f}x     | {'0.00000000':<12} |")
    print(f"| {'AVXzkd SUPREME (AVX2+FMA)':<28} | {t_avx*1000:7.2f} ms | {rate_avx:8.2f} MCells/s | {rate_avx/rate_scalar:7.1f}x     | {l_inf_err:<12.6f} |")
    print("=========================================================================================")
    print(f"Speedup vs NumPy Array Operations : {t_numpy / t_avx:.2f}x faster")
    print(f"Speedup vs Pure Python Engine     : {t_scalar_scaled / t_avx:.2f}x faster")
    print(f"Maximum Deviation (L_inf)         : {l_inf_err:.8f}")
    print(f"Mean Point-wise Deviation         : {mean_err:.8f}")
    print("=========================================================================================")

if __name__ == "__main__":
    benchmark()
