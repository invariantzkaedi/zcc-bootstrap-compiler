import os
import subprocess
import time
import sys

# Ensure we are in the script's directory
os.chdir(os.path.dirname(os.path.abspath(__file__)))

def run_cmd(cmd, env=None):
    print(f"Executing: {cmd}")
    process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        print(f"Error executing command: {cmd}")
        print("STDOUT:", stdout.decode())
        print("STDERR:", stderr.decode())
        return False
    return True

def build_raytracer():
    print("=== Building Raytracer with ZCC (Stress Test) ===")
    # Step 1: ZCC to Assembly
    if not run_cmd("./zcc raytracer.c -o ray.s"):
        return False
    # Step 2: GCC (as assembler/linker) to Binary
    # Note: Using -O3 for the final binary since we are rendering
    if not run_cmd("gcc -O3 ray.s -o ray -lm"):
        return False
    print("=== Build Successful ===")
    return True

import concurrent.futures

def render_frame(i, total_frames):
    frame_start = time.time()
    output_file = f"frames/frame_{i:02d}.ppm"
    
    env = os.environ.copy()
    env["FRAME"] = str(i)
    
    print(f"[{i+1}/{total_frames}] Started {output_file}...", flush=True)
    
    with open(output_file, "wb") as f:
        process = subprocess.Popen("./ray", env=env, stdout=f, stderr=subprocess.PIPE)
        process.communicate()
    
    if process.returncode != 0:
        print(f"[{i+1}/{total_frames}] FAILED")
    else:
        elapsed = time.time() - frame_start
        print(f"[{i+1}/{total_frames}] DONE ({elapsed:.2f}s)")

def render_frames(total_frames=30):
    if not os.path.exists("frames"):
        os.makedirs("frames")
    
    print(f"=== Rendering {total_frames} Frames Concurrently ===")
    start_time = time.time()
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=30) as executor:
        futures = [executor.submit(render_frame, i, total_frames) for i in range(total_frames)]
        concurrent.futures.wait(futures)
            
    total_elapsed = time.time() - start_time
    print(f"=== Rendering Complete in {total_elapsed:.2f}s ===")

if __name__ == "__main__":
    if not os.path.exists("./ray") or "--rebuild" in sys.argv:
        if not build_raytracer():
            sys.exit(1)
    
    render_frames(30)
