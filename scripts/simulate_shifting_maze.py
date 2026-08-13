import os
import sys
import numpy as np
from collections import deque
import multiprocessing

# Add repository root to Python path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tools.prime.zkaedi_prime import make_maze, bfs_len, hamiltonian_field

_MOVES = ((0, 1), (1, 0), (0, -1), (-1, 0))

def has_path(grid, start, end):
    """Verify if a path exists between start and end using BFS."""
    if grid[start[0]][start[1]] == 0 or grid[end[0]][end[1]] == 0:
        return False
    q = deque([start])
    visited = {start}
    while q:
        x, y = q.popleft()
        if (x, y) == end:
            return True
        for dx, dy in _MOVES:
            nx, ny = x + dx, y + dy
            if 0 <= nx < grid.shape[0] and 0 <= ny < grid.shape[1]:
                if grid[nx][ny] == 1 and (nx, ny) not in visited:
                    visited.add((nx, ny))
                    q.append((nx, ny))
    return False

def calculate_entropy(visit_matrix):
    """Compute the Shannon entropy of the visited cells distribution."""
    total = np.sum(visit_matrix)
    if total == 0:
        return 0.0
    probs = visit_matrix[visit_matrix > 0] / total
    return -np.sum(probs * np.log2(probs))

def run_simulation(seed, use_solvability_guard=False, use_adaptive_memory=False):
    size = 20
    maze = make_maze(n=size, seed=seed, wall_density=0.32)
    while bfs_len(maze) is None:
        seed += 99
        maze = make_maze(n=size, seed=seed, wall_density=0.32)
        
    grid = maze.grid.copy()
    current = maze.start
    goal = maze.end
    
    scars = np.zeros_like(grid, dtype=float)
    visit_counts = np.zeros_like(grid, dtype=int)
    
    path_stack = [current]
    path_set = {current}
    sealed = set()
    
    steps = 0
    max_steps = 3000
    warp_count = 0
    backtrack_steps = 0
    forward_steps = 0
    neighbors_options_sum = 0
    
    rng = np.random.default_rng(seed)
    
    while current != goal and steps < max_steps:
        # 1. 18% Shifting Maze check
        if rng.random() < 0.18:
            path_candidates = []
            wall_candidates = []
            for r in range(1, size-1):
                for c in range(1, size-1):
                    if (r, c) == current or (r, c) == goal:
                        continue
                    if (r, c) in path_set:
                        continue
                    if grid[r][c] == 1:
                        path_candidates.append((r, c))
                    else:
                        wall_candidates.append((r, c))
                        
            if path_candidates and wall_candidates:
                p_tile = path_candidates[rng.choice(len(path_candidates))]
                w_tile = wall_candidates[rng.choice(len(wall_candidates))]
                
                grid[p_tile[0]][p_tile[1]] = 0
                grid[w_tile[0]][w_tile[1]] = 1
                
                if use_solvability_guard:
                    if not has_path(grid, current, goal):
                        grid[p_tile[0]][p_tile[1]] = 1
                        grid[w_tile[0]][w_tile[1]] = 0
                    else:
                        sealed.discard(w_tile)
                        warp_count += 1
                        if use_adaptive_memory:
                            scars *= 0.70
                else:
                    sealed.discard(w_tile)
                    warp_count += 1
                    
        # 2. Step the walker
        scars[current[0]][current[1]] += 2.0
        visit_counts[current[0]][current[1]] += 1
        
        # Loop Check
        if use_adaptive_memory and visit_counts[current[0]][current[1]] > 3:
            sealed.add(current)
            scars[current[0]][current[1]] += 10.0
            
        neighbors = []
        for dr, dc in _MOVES:
            nr, nc = current[0] + dr, current[1] + dc
            if 0 <= nr < size and 0 <= nc < size:
                if grid[nr][nc] == 1 and (nr, nc) not in path_set and (nr, nc) not in sealed:
                    neighbors.append((nr, nc))
                    
        neighbors_options_sum += len(neighbors)
        
        if neighbors:
            best = None
            best_val = float('inf')
            for n in neighbors:
                dist = np.hypot(n[0] - goal[0], n[1] - goal[1])
                val = dist + scars[n[0]][n[1]]
                if val < best_val:
                    best_val = val
                    best = n
            current = best
            path_stack.append(current)
            path_set.add(current)
            forward_steps += 1
        else:
            sealed.add(current)
            scars[current[0]][current[1]] += 10.0
            path_set.discard(current)
            path_stack.pop()
            backtrack_steps += 1
            if path_stack:
                current = path_stack[-1]
            else:
                return {
                    "success": False,
                    "reason": "BLOCKED_FULLY",
                    "steps": steps,
                    "forward_steps": forward_steps,
                    "backtrack_steps": backtrack_steps,
                    "warp_count": warp_count,
                    "avg_branching": neighbors_options_sum / max(1, steps),
                    "path_len": len(path_stack),
                    "visit_entropy": calculate_entropy(visit_counts),
                    "pot_field_var": np.var(scars[grid == 1])
                }
                
        steps += 1
        
    success = (current == goal)
    return {
        "success": success,
        "reason": "SUCCESS" if success else "TIMEOUT",
        "steps": steps,
        "forward_steps": forward_steps,
        "backtrack_steps": backtrack_steps,
        "warp_count": warp_count,
        "avg_branching": neighbors_options_sum / max(1, steps),
        "path_len": len(path_stack),
        "visit_entropy": calculate_entropy(visit_counts),
        "pot_field_var": np.var(scars[grid == 1])
    }

def run_protected(seed):
    return run_simulation(seed, use_solvability_guard=True, use_adaptive_memory=False)

def run_adaptive(seed):
    return run_simulation(seed, use_solvability_guard=True, use_adaptive_memory=True)

def analyze_regime(results):
    total = len(results)
    successes = [r for r in results if r["success"]]
    failures = [r for r in results if not r["success"]]
    
    success_rate = len(successes) / total * 100
    
    # Bootstrap CI for success rate (95%)
    success_boot = []
    rng = np.random.default_rng(42)
    for _ in range(1000):
        sample = rng.choice(results, size=total, replace=True)
        rate = sum(1 for r in sample if r["success"]) / total * 100
        success_boot.append(rate)
    success_ci = (np.percentile(success_boot, 2.5), np.percentile(success_boot, 97.5))
    
    steps = [r["steps"] for r in successes]
    backtracks = [r["backtrack_steps"] for r in successes]
    warps = [r["warp_count"] for r in successes]
    branching = [r["avg_branching"] for r in successes]
    path_lengths = [r["path_len"] for r in successes]
    entropies = [r["visit_entropy"] for r in successes]
    field_vars = [r["pot_field_var"] for r in successes]
    
    efficiency = [r["path_len"] / max(1, r["steps"]) for r in successes]
    
    # Correlation between warps and steps
    warp_step_corr = 0.0
    if len(successes) > 1:
        warp_step_corr = np.corrcoef(warps, steps)[0, 1]
        
    blocked = sum(1 for r in failures if r["reason"] == "BLOCKED_FULLY")
    timeout = sum(1 for r in failures if r["reason"] == "TIMEOUT")
    
    return {
        "success_rate": success_rate,
        "success_ci": success_ci,
        "median_steps": np.median(steps) if steps else 0.0,
        "mean_steps": np.mean(steps) if steps else 0.0,
        "std_steps": np.std(steps) if steps else 0.0,
        "p95_steps": np.percentile(steps, 95) if steps else 0.0,
        "mean_backtracks": np.mean(backtracks) if backtracks else 0.0,
        "mean_warps": np.mean(warps) if warps else 0.0,
        "mean_branching": np.mean(branching) if branching else 0.0,
        "mean_path_len": np.mean(path_lengths) if path_lengths else 0.0,
        "mean_efficiency": np.mean(efficiency) if efficiency else 0.0,
        "mean_entropy": np.mean(entropies) if entropies else 0.0,
        "mean_field_var": np.mean(field_vars) if field_vars else 0.0,
        "warp_step_corr": warp_step_corr,
        "blocked": blocked,
        "timeout": timeout
    }

def main():
    total_runs = 10000
    print(f"[+] Initializing Complete Multifold Metrics Simulator: {total_runs} runs per regime")
    
    cpus = multiprocessing.cpu_count()
    pool = multiprocessing.Pool(processes=cpus)
    
    print("\n[~] Running Protected (Gated-Only)...")
    protected_results = pool.map(run_protected, range(total_runs))
    p_metrics = analyze_regime(protected_results)
    
    print("\n[~] Running Adaptive Memory (Decay + Loop checks)...")
    adaptive_results = pool.map(run_adaptive, range(total_runs))
    a_metrics = analyze_regime(adaptive_results)
    
    pool.close()
    pool.join()
    
    print("\n======================================================================")
    print("                    ZKAEDI PRIME ULTRA METRIC LOG")
    print("======================================================================")
    
    print(f"Success Rate (95% CI):")
    print(f"  - Protected: {p_metrics['success_rate']:.2f}%  [{p_metrics['success_ci'][0]:.2f}% - {p_metrics['success_ci'][1]:.2f}%]")
    print(f"  - Adaptive:  {a_metrics['success_rate']:.2f}%  [{a_metrics['success_ci'][0]:.2f}% - {a_metrics['success_ci'][1]:.2f}%]")
    
    print(f"\nLocomotion Latencies (Steps):")
    print(f"  - Protected: Median={p_metrics['median_steps']:.1f} | Mean={p_metrics['mean_steps']:.2f} | StdDev={p_metrics['std_steps']:.2f} | P95={p_metrics['p95_steps']:.1f}")
    print(f"  - Adaptive:  Median={a_metrics['median_steps']:.1f} | Mean={a_metrics['mean_steps']:.2f} | StdDev={a_metrics['std_steps']:.2f} | P95={a_metrics['p95_steps']:.1f}")
    
    print(f"\nExploration & Path Efficiencies:")
    print(f"  - Protected: Mean Path Length={p_metrics['mean_path_len']:.2f} | Directness Ratio={p_metrics['mean_efficiency']:.4f} | Branching Factor={p_metrics['mean_branching']:.3f}")
    print(f"  - Adaptive:  Mean Path Length={a_metrics['mean_path_len']:.2f} | Directness Ratio={a_metrics['mean_efficiency']:.4f} | Branching Factor={a_metrics['mean_branching']:.3f}")
    
    print(f"\nEntropy & Attractor Variance:")
    print(f"  - Protected: Shannon Position Entropy={p_metrics['mean_entropy']:.4f} | Field Potential Var={p_metrics['mean_field_var']:.2e}")
    print(f"  - Adaptive:  Shannon Position Entropy={a_metrics['mean_entropy']:.4f} | Field Potential Var={a_metrics['mean_field_var']:.2e}")
    
    print(f"\nDynamics Interferences:")
    print(f"  - Protected: Warp-to-Step Correlation={p_metrics['warp_step_corr']:.4f} | Mean Backtracks={p_metrics['mean_backtracks']:.2f}")
    print(f"  - Adaptive:  Warp-to-Step Correlation={a_metrics['warp_step_corr']:.4f} | Mean Backtracks={a_metrics['mean_backtracks']:.2f}")
    
    print(f"\nFails Categorized:")
    print(f"  - Protected: Blocked Fully={p_metrics['blocked']} | Timeout={p_metrics['timeout']}")
    print(f"  - Adaptive:  Blocked Fully={a_metrics['blocked']} | Timeout={a_metrics['timeout']}")
    print("======================================================================")

if __name__ == "__main__":
    main()
