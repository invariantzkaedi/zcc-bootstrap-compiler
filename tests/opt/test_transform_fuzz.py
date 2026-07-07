#!/usr/bin/env python3
import os
import subprocess
import sys
import random

def run_fuzz():
    root = os.path.dirname(os.path.abspath(__file__))
    zcc_opt = os.path.join(root, "..", "..", "zcc-opt")
    if not os.path.exists(zcc_opt):
        print(f"Error: zcc-opt not found at {zcc_opt}", file=sys.stderr)
        sys.exit(1)

    # Collect all .ir input files
    ir_files = []
    dirs_to_search = [
        os.path.join(root, "mixed"),
        os.path.join(root, "unroll"),
        os.path.join(root, "inline")
    ]
    for d in dirs_to_search:
        if not os.path.exists(d):
            continue
        for sub in os.listdir(d):
            sub_path = os.path.join(d, sub)
            if os.path.isdir(sub_path):
                input_ir = os.path.join(sub_path, "input.ir")
                if os.path.exists(input_ir):
                    ir_files.append(input_ir)

    print(f"Found {len(ir_files)} seed IR files for transform fuzzing.")

    # Define pass options
    passes_opts = ["loop", "instcombine", "sccp", "cfg_simplify"]
    
    # Run 5 random fuzz runs for each file
    success = True
    for ir_file in ir_files:
        print(f"Fuzzing {os.path.basename(os.path.dirname(ir_file))}...")
        for run_id in range(5):
            # Select random options
            enable_unroll = random.choice([True, False])
            enable_inline = random.choice([True, False])
            
            # Select random subset of passes (1 to 3 passes)
            selected_passes = random.sample(passes_opts, k=random.randint(1, len(passes_opts)))
            
            cmd = [zcc_opt]
            if enable_unroll:
                cmd.append("--enable-unroll-mvp")
            if enable_inline:
                cmd.append("--enable-inline-mvp")
            for p in selected_passes:
                cmd.append(f"--pass={p}")
            cmd.append(ir_file)
            cmd.extend(["-o", "/dev/null"])

            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode != 0:
                print(f"  [FAIL] Run {run_id} failed!")
                print(f"  Command: {' '.join(cmd)}")
                print(f"  Stdout:\n{res.stdout}")
                print(f"  Stderr:\n{res.stderr}")
                success = False
            else:
                pass

    if success:
        print("ALL TRANSFORM FUZZ RUNS PASSED CLINICALLY! 🟢")
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    run_fuzz()
