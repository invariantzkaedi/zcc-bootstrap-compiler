import os
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# Add repository root to Python path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tools.prime.zkaedi_prime import make_maze, solve_zkaedi_prime_v3

def main():
    print("[+] Running ZKAEDI PRIME Walker Path Trajectory Visualizer")
    
    from tools.prime.zkaedi_prime import bfs_len
    size = 25
    seed = 0
    while True:
        maze = make_maze(n=size, seed=seed, wall_density=0.32)
        if bfs_len(maze) is not None:
            break
        seed += 1

    
    # 2. Run the ZKAEDI PRIME v3 backtrack solver
    sol = solve_zkaedi_prime_v3(maze, eps=0.05, seed=42, max_steps=5000)
    
    if sol is None:
        print("[-] Error: Maze was not solved!")
        return
        
    print(f"[+] Maze solved in {sol.steps} steps! Simple path length: {sol.meta.get('path_len')}")
    
    # 3. Plot the maze and walker path
    plt.figure(figsize=(9, 9))
    
    # Grid coordinates
    grid = maze.grid.copy()
    
    # Background and walls: 0=wall (cyan highlight), 1=open cell (dark space)
    # We use a custom color mapping: dark space background with neon blue grid walls
    cmap = matplotlib.colors.ListedColormap(['#10172e', '#070a14'])
    plt.imshow(grid, cmap=cmap, origin='upper')
    
    # Extract path coordinates
    path = sol.path
    py, px = zip(*path)
    
    # Plot the full walker exploration trajectory (path with backtracking)
    # Draw it in a translucent color representing the "scars"
    plt.plot(px, py, color='#ff3b7a', alpha=0.35, linewidth=2.5, label='Exploration Trail (Scars)')
    
    # Extract the final simplified path (stack state on success)
    # Since trajectory contains backtracking, we can extract the path by reconstructing it
    # or just show the trajectory gradient. Let's trace the steps as a glowing color transition.
    sc = plt.scatter(px, py, c=np.arange(len(path)), cmap='cool', s=12, edgecolors='none', label='Walker Timeline', zorder=3)
    plt.colorbar(sc, label="Locomotion Step Index", orientation="horizontal", pad=0.08, shrink=0.7)
    
    # Marker for Start and End points
    plt.scatter(maze.start[1], maze.start[0], color='#22e6a8', s=120, edgecolors='#ffffff', linewidths=2, label='Start Point', zorder=5)
    plt.scatter(maze.end[1], maze.end[0], color='#ff0055', s=120, edgecolors='#ffffff', linewidths=2, label='Goal / Target', zorder=5)
    
    # Formatting grid appearance
    plt.title(f"ZKAEDI PRIME v3 Walker Trajectory\nExploration Moves: {sol.steps} | Path: {sol.meta.get('path_len')} Steps", fontsize=12, color='black')
    plt.grid(color='#1c2541', linestyle='-', linewidth=0.5)
    plt.legend(loc='upper right')
    plt.xticks(np.arange(0, size, 5))
    plt.yticks(np.arange(0, size, 5))
    plt.tight_layout()
    
    os.makedirs("artifacts", exist_ok=True)
    png_path = "artifacts/walker_navigation.png"
    plt.savefig(png_path, dpi=150)
    plt.close()
    
    print(f"[+] Walker trajectory visual successfully written to {png_path}")

if __name__ == "__main__":
    main()
