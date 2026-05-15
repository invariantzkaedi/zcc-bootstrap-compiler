import os
import sys
import random
import subprocess
import hashlib
import re

def gen_alias_basic(idx):
    code = f"""    {{
        int x = {random.randint(1, 10)};
        int *p = &x;
        *p = {random.randint(11, 20)};
        result = result + (x & 0xFF);
    }}"""
    return code

def gen_alias_multi(idx):
    code = f"""    {{
        int x = {random.randint(1, 10)};
        int y = {random.randint(11, 20)};
        int *p = &x;
        int *q = &x;
        *p = {random.randint(21, 30)};
        *q = {random.randint(31, 40)};
        result = result + (x & 0xFF);
    }}"""
    return code

def gen_alias_volatile(idx):
    code = f"""    {{
        volatile int x;
        x = {random.randint(1, 10)};
        x = {random.randint(11, 20)};
        result = result + (x & 0xFF);
    }}"""
    return code

def gen_alias_escape(idx):
    code = f"""    {{
        int x = {random.randint(1, 10)};
        int *p = &x;
        int y = *p;
        *p = {random.randint(11, 20)};
        result = result + ((x + y) & 0xFF);
    }}"""
    return code

def generate_program():
    tests = []
    generators = [gen_alias_basic, gen_alias_multi, gen_alias_volatile, gen_alias_escape]
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
    print(f"[ZCC-HOOK-16] ALIAS COURTROOM INITIATED (Targeting {iterations} universes).")
    ZCC = os.environ.get("ZCC", "./zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_alias.c", "w") as f:
            f.write(prog)
        
        # GCC oracle
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_alias.c", "-o", "repro_gcc_alias"], capture_output=True)
        if gcc_rc.returncode != 0: continue
            
        try:
            gcc_run = subprocess.run(["./repro_gcc_alias"], capture_output=True, timeout=2)
            oracle_exit = gcc_run.returncode
        except subprocess.TimeoutExpired:
            continue
        
        # ZCC Alias Optimization + X86 IR backend
        env_opt = os.environ.copy()
        env_opt["ZCC_OPT"] = "1"
        zcc_opt_rc = subprocess.run([ZCC, "repro_alias.c", "--ir", "-o", "repro_zcc_alias.s"], env=env_opt, capture_output=True)
        
        if zcc_opt_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH (ALIAS)! Seed: {seed}")
            print(zcc_opt_rc.stderr.decode('utf-8'))
            sys.exit(1)
            
        # Verify ZCC binary
        subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_alias.s", "-o", "repro_zcc_alias"], capture_output=True)
        try:
            zcc_run = subprocess.run(["./repro_zcc_alias"], capture_output=True, timeout=2)
            zcc_exit = zcc_run.returncode
        except subprocess.TimeoutExpired:
            zcc_exit = -1 # Indicate hang
        
        if oracle_exit != zcc_exit:
            h = hashlib.sha256(prog.encode()).hexdigest()
            print(f"\n[!] ALIAS SEMANTIC MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {oracle_exit}")
            print(f"    ZCC Alias   : {zcc_exit}")
            print(f"    Address Manifold: {h}")
            sys.exit(1)
            
        if (i+1) % 10 == 0:
            h = hashlib.sha256(prog.encode()).hexdigest()[:8]
            sys.stdout.write(f"\r[ZCC-HOOK-16] Validated {i+1} Alias topologies. Last Hash: {h}...")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-16] COURTROOM ADJOURNED. Alias Optimization identity preserved.")

if __name__ == "__main__":
    iters = 100
    if len(sys.argv) > 1: iters = int(sys.argv[1])
    run_fuzzer(iters)
