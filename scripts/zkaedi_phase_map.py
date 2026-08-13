import os
import csv
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-np.clip(x, -500.0, 500.0)))

def sigmoid_derivative(x):
    s = sigmoid(x)
    return s * (1.0 - s)

def main():
    print("[+] Starting ZKAEDI PRIME Phase Map Calculation")
    
    # 1. Setup a standard 25x25 maze configuration
    n = 25
    rng = np.random.default_rng(42)
    grid = (rng.random((n, n)) > 0.32).astype(int)
    grid[0, 0] = grid[n-1, n-1] = 1 # path endpoints
    
    end = (n - 1, n - 1)
    ii, jj = np.meshgrid(np.arange(n), np.arange(n), indexing="ij")
    H_base = np.hypot(ii - end[0], jj - end[1]).astype(float)
    H_base[grid == 0] = 1e6
    
    beta = 0.1
    eps = 0.05
    
    # Grid search boundaries
    eta_range = np.linspace(0.0, 1.3, 30)
    gamma_range = np.linspace(0.0, 1.0, 30)
    
    csv_rows = []
    Z_lambda = np.zeros((len(gamma_range), len(eta_range)))
    Z_log_var = np.zeros((len(gamma_range), len(eta_range)))
    
    critical_frontier = []
    
    # Sweep grid
    for gi, g_val in enumerate(gamma_range):
        frontier_eta = None
        min_abs_lam = float('inf')
        
        for ei, e_val in enumerate(eta_range):
            H = H_base.copy()
            lyapunov_sum = 0.0
            T_warmup = 100
            T_calc = 150
            
            # Warmup
            for _ in range(T_warmup):
                sig = sigmoid(H)
                noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
                H = H_base + e_val * H * sig + eps * noise
                
            # Calculation
            variances = []
            for _ in range(T_calc):
                sig = sigmoid(H)
                dsig = sigmoid_derivative(g_val * H)
                deriv = e_val * (sig + H * g_val * dsig)
                
                path_derivs = deriv[grid == 1]
                lyapunov_sum += np.mean(np.log(np.abs(path_derivs) + 1e-12))
                
                noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
                H = H_base + e_val * H * sig + eps * noise
                variances.append(np.var(H[grid == 1]))
                
            avg_lambda = lyapunov_sum / T_calc
            avg_var = np.mean(variances)
            
            Z_lambda[gi, ei] = avg_lambda
            Z_log_var[gi, ei] = np.log10(avg_var + 1e-12)
            
            csv_rows.append({
                "eta": e_val,
                "gamma": g_val,
                "lambda": avg_lambda,
                "variance": avg_var
            })
            
            # Estimate frontier where lambda crosses 0
            if abs(avg_lambda) < min_abs_lam:
                min_abs_lam = abs(avg_lambda)
                frontier_eta = e_val
                
        if frontier_eta is not None:
            critical_frontier.append({
                "gamma": g_val,
                "eta_c": frontier_eta
            })
            
    # Save CSV
    os.makedirs("artifacts", exist_ok=True)
    csv_path = "artifacts/prime_phase_map.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["eta", "gamma", "lambda", "variance"])
        writer.writeheader()
        writer.writerows(csv_rows)
        
    # Save JSON Frontier
    json_path = "artifacts/prime_critical_frontier.json"
    with open(json_path, "w") as f:
        json.dump({"critical_frontier": critical_frontier}, f, indent=2)
        
    # Save Plot Image
    plt.figure(figsize=(8, 6))
    
    # Plot Lyapunov contour map
    X, Y = np.meshgrid(eta_range, gamma_range)
    cp = plt.contourf(X, Y, Z_lambda, levels=20, cmap="plasma")
    plt.colorbar(cp, label="Lyapunov Exponent ($\\lambda$)")
    
    # Plot critical contour line at lambda = 0
    cs = plt.contour(X, Y, Z_lambda, levels=[0.0], colors="white", linestyles="dashed", linewidths=2)
    plt.clabel(cs, inline=True, fmt="$\\lambda=0$ Frontier", fontsize=10)
    
    plt.title("ZKAEDI PRIME Phase Space Map")
    plt.xlabel("Coupling Coefficient ($\\eta$)")
    plt.ylabel("Saturation Scaling ($\\gamma$)")
    plt.tight_layout()
    
    png_path = "artifacts/prime_phase_map.png"
    plt.savefig(png_path, dpi=150)
    plt.close()
    
    print(f"[+] Output written to:")
    print(f"    - {csv_path}")
    print(f"    - {json_path}")
    print(f"    - {png_path}")

if __name__ == "__main__":
    main()
