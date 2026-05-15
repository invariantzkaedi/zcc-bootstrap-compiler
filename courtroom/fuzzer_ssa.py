import os
import sys
import random
import subprocess
import hashlib
import re

def gen_ssa_basic(idx):
    code = f"""    {{
        int a = {random.randint(1, 10)};
        int b = {random.randint(11, 20)};
        if (a > 5) {{
            a = a + 2;
        }} else {{
            a = b - 1;
        }}
        result = result + (a & 0xFF);
    }}"""
    return code

def gen_ssa_loop(idx):
    code = f"""    {{
        int sum = 0;
        for (int i = 0; i < {random.randint(3, 8)}; i = i + 1) {{
            sum = sum + i;
        }}
        result = result + (sum & 0xFF);
    }}"""
    return code

def gen_ssa_nested(idx):
    code = f"""    {{
        int x = 1;
        int y = 2;
        if (x < y) {{
            if (y > 0) {{
                x = 10;
            }} else {{
                x = 20;
            }}
        }} else {{
            y = 5;
        }}
        result = result + ((x + y) & 0xFF);
    }}"""
    return code

def generate_program():
    tests = []
    generators = [gen_ssa_basic, gen_ssa_loop, gen_ssa_nested]
    num_tests = random.randint(4, 8)
    for i in range(num_tests):
        tests.append(random.choice(generators)(i))
    test_block = "\n".join(tests)
    prog = f"""int main() {{
    int result = 0;
{test_block}
    return result & 255;
}}
"""
    return prog

def run_fuzzer(iterations):
    print(f"[ZCC-HOOK-15] SSA COURTROOM INITIATED (Targeting {iterations} universes).")
    ZCC = os.environ.get("ZCC", "./zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_ssa.c", "w") as f:
            f.write(prog)
        
        # GCC oracle
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_ssa.c", "-o", "repro_gcc_ssa"], capture_output=True)
        if gcc_rc.returncode != 0: continue
            
        try:
            gcc_run = subprocess.run(["./repro_gcc_ssa"], capture_output=True, timeout=2)
            oracle_exit = gcc_run.returncode
        except subprocess.TimeoutExpired:
            continue
        
        # ZCC SSA Optimization + X86 IR backend
        env_opt = os.environ.copy()
        env_opt["ZCC_OPT"] = "1"
        zcc_opt_rc = subprocess.run([ZCC, "repro_ssa.c", "--ir", "-o", "repro_zcc_ssa.s"], env=env_opt, capture_output=True)
        
        if zcc_opt_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH (SSA)! Seed: {seed}")
            print(zcc_opt_rc.stderr.decode('utf-8'))
            sys.exit(1)
            
        stderr_output = zcc_opt_rc.stderr.decode('utf-8', errors='replace')
        
        # Extract snapshots
        snapshots = re.findall(r"\[ZCC-SNAPSHOT\] Pass (\w+): IR=([0-9a-f]+) CFG=([0-9a-f]+) DOM=([0-9a-f]+) LIVE=([0-9a-f]+)", stderr_output)
        if not snapshots:
            print(f"\n[!] WARNING: No SSA snapshots captured. Check ir_snapshot_state wiring. Seed: {seed}")
            sys.exit(1)
            
        # Verify ZCC binary
        subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_ssa.s", "-o", "repro_zcc_ssa"], capture_output=True)
        try:
            zcc_run = subprocess.run(["./repro_zcc_ssa"], capture_output=True, timeout=2)
            zcc_exit = zcc_run.returncode
        except subprocess.TimeoutExpired:
            zcc_exit = -1 # Indicate hang
        
        if oracle_exit != zcc_exit:
            h = hashlib.sha256(prog.encode()).hexdigest()
            print(f"\n[!] SSA SEMANTIC MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {oracle_exit}")
            print(f"    ZCC SSA     : {zcc_exit}")
            print(f"    Address Manifold: {h}")
            sys.exit(1)
            
        if (i+1) % 10 == 0:
            h = hashlib.sha256(prog.encode()).hexdigest()[:8]
            sys.stdout.write(f"\r[ZCC-HOOK-15] Validated {i+1} SSA topologies. Snapshots active: {len(snapshots)}. Last Hash: {h}...")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-15] COURTROOM ADJOURNED. SSA Optimization identity preserved.")

if __name__ == "__main__":
    iters = 100
    if len(sys.argv) > 1: iters = int(sys.argv[1])
    run_fuzzer(iters)
