import os
import csv
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import sys

# Add repository root to Python path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tools.prime.zkaedi_prime import make_maze, solve_zkaedi_prime_v3, hamiltonian_field


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-np.clip(x, -500.0, 500.0)))

def sigmoid_derivative(x):
    s = sigmoid(x)
    return s * (1.0 - s)

def main():
    print("[+] Initializing ZKAEDI PRIME Phase-Navigation Co-dependency Mapping")
    
    # Generate 5 test mazes of size 20x20
    test_mazes = [make_maze(n=20, seed=i, wall_density=0.32) for i in range(5)]
    
    # Sweeping coordinates (15x15 grid for fast interactive execution)
    eta_range = np.linspace(0.0, 1.3, 15)
    gamma_range = np.linspace(0.0, 1.0, 15)
    
    Z_lambda = np.zeros((len(gamma_range), len(eta_range)))
    Z_success = np.zeros((len(gamma_range), len(eta_range)))
    
    csv_rows = []
    
    # Base configuration
    beta = 0.1
    eps = 0.05
    kick = 2.0
    decay = 1.0
    
    for gi, g_val in enumerate(gamma_range):
        for ei, e_val in enumerate(eta_range):
            # 1. Lyapunov Exponent & Field dynamics calculation on first maze
            maze = test_mazes[0]
            n, m = maze.size
            H_base = hamiltonian_field(maze)
            H = H_base.copy()
            rng = np.random.default_rng(42)
            
            # Field warmup
            for _ in range(50):
                sig = sigmoid(H)
                noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
                H = H_base + e_val * H * sig + eps * noise
                
            # Exponent accumulation
            lyapunov_sum = 0.0
            T_calc = 100
            for _ in range(T_calc):
                sig = sigmoid(H)
                dsig = sigmoid_derivative(g_val * H)
                deriv = e_val * (sig + H * g_val * dsig)
                
                path_derivs = deriv[maze.grid == 1]
                lyapunov_sum += np.mean(np.log(np.abs(path_derivs) + 1e-12))
                
                noise = rng.normal(0.0, 1.0 + beta * np.minimum(np.abs(H), 100.0))
                H = H_base + e_val * H * sig + eps * noise
                
            avg_lambda = lyapunov_sum / T_calc
            Z_lambda[gi, ei] = avg_lambda
            
            # 2. Navigation Success Rate across the 5 test mazes
            solved_count = 0
            for m_idx, m_obj in enumerate(test_mazes):
                # Run the backtrack solver
                sol = solve_zkaedi_prime_v3(
                    m_obj,
                    eta=e_val,
                    gamma=g_val,

                    beta=beta,
                    eps=eps,
                    kick=kick,
                    decay=decay,
                    seed=m_idx,
                    max_steps=1000
                )
                if sol is not None:
                    solved_count += 1
                    
            success_rate = solved_count / len(test_mazes)
            Z_success[gi, ei] = success_rate
            
            csv_rows.append({
                "eta": e_val,
                "gamma": g_val,
                "lambda": avg_lambda,
                "success_rate": success_rate
            })
            
    # Write details to CSV
    csv_path = "artifacts/prime_navigation_phase_map.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["eta", "gamma", "lambda", "success_rate"])
        writer.writeheader()
        writer.writerows(csv_rows)
        
    # Plot side-by-side Dual Panel
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    X, Y = np.meshgrid(eta_range, gamma_range)
    
    # Panel A: Lyapunov exponent
    cp1 = axes[0].contourf(X, Y, Z_lambda, levels=15, cmap="plasma")
    fig.colorbar(cp1, ax=axes[0], label="Lyapunov Exponent ($\\lambda$)")
    cs1 = axes[0].contour(X, Y, Z_lambda, levels=[0.0], colors="white", linestyles="dashed", linewidths=2)
    axes[0].clabel(cs1, inline=True, fmt="$\\lambda=0$ Frontier", fontsize=8)
    axes[0].set_title("Panel A: Field Attractor ($\\lambda$)")
    axes[0].set_xlabel("Coupling Coefficient ($\\eta$)")
    axes[0].set_ylabel("Saturation ($\\gamma$)")
    
    # Panel B: Navigation Success Rate
    cp2 = axes[1].contourf(X, Y, Z_success, levels=5, cmap="viridis")
    fig.colorbar(cp2, ax=axes[1], label="Navigation Success Rate")
    axes[1].set_title("Panel B: Walker Success Rate")
    axes[1].set_xlabel("Coupling Coefficient ($\\eta$)")
    axes[1].set_ylabel("Saturation ($\\gamma$)")
    
    plt.suptitle("ZKAEDI PRIME Dual-Regime Phase & Navigation Manifold", fontsize=14)
    plt.tight_layout()
    
    png_path = "artifacts/prime_navigation_phase_map.png"
    plt.savefig(png_path, dpi=150)
    plt.close()
    
    print(f"[+] Multi-manifold mapping complete:")
    print(f"    - {csv_path}")
    print(f"    - {png_path}")

if __name__ == "__main__":
    main()
