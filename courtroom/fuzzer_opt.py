import os
import sys
import random
import subprocess
import hashlib

ELEM_TYPES = ["char", "short", "int", "long"]

def gen_short_circuit(idx):
    code = f"""    {{
        int a = 0, b = 1, c = 2;
        if (a && (b = 3)) c = 4;
        if (b != 1) return 200 + {idx}; // b should not be evaluated
        if (a || (b = 4)) c = 5;
        if (b != 4) return 201 + {idx}; // b should be evaluated
        result += c & 0xFF;
    }}"""
    return code, ""

def gen_ternary(idx):
    x_val = random.randint(1, 100)
    y_val = random.randint(1, 100)
    code = f"""    {{
        int x = 1 ? {x_val} : 0;
        int y = 0 ? 0 : {y_val};
        volatile int z = x + y;
        result += z & 0xFF;
    }}"""
    return code, ""

def gen_volatile_barrier(idx):
    v1 = random.randint(1, 50)
    v2 = random.randint(51, 100)
    code = f"""    {{
        int x = {v1};
        volatile int *p = &x;
        x = {v2};
        *p = {v1}; // barrier
        result += x & 0xFF;
    }}"""
    return code, ""

def gen_pointer_aliasing(idx):
    aliased = random.choice(['&x', '&y'])
    code = f"""    {{
        int x = 10;
        int y = 20;
        int *p = {aliased};
        int *q = &x;
        *p = 50;
        *q = 60;
        result += (*p + *q) & 0xFF;
    }}"""
    return code, ""

def gen_signed_overflow(idx):
    addend = random.randint(1, 100)
    code = f"""    {{
        int max = 2147483647;
        int x = max + {addend}; // wrapped
        if (x > 0) return 202 + {idx};
        result += (x >> 24) & 0xFF;
    }}"""
    return code, ""

def gen_struct_copy(idx):
    a = random.randint(1, 10)
    b = random.randint(1, 10)
    c = random.randint(1, 10)
    code = f"""    {{
        struct S_{idx} {{ int a, b, c; }} s1 = {{{a}, {b}, {c}}};
        struct S_{idx} s2;
        s2 = s1;
        s1.a = 100;
        result += (s2.a + s2.b + s2.c) & 0xFF;
    }}"""
    return code, ""

def generate_program():
    helpers = []
    tests = []
    generators = [gen_short_circuit, gen_ternary, gen_volatile_barrier, gen_pointer_aliasing, gen_signed_overflow, gen_struct_copy]
    
    num_tests = random.randint(4, 10)
    for i in range(num_tests):
        code, helper = random.choice(generators)(i)
        if helper: helpers.append(helper)
        tests.append(code)
        
    helper_block = "\n".join(helpers)
    test_block = "\n".join(tests)
    
    prog = f"""{helper_block}
int main() {{
    int result = 0;
{test_block}
    return result & 255;
}}
"""
    return prog

def run_fuzzer(iterations):
    print("[ZCC-HOOK-14] OPTIMIZER COURTROOM INITIATED (Targeting %d universes)." % iterations)
    ZCC = os.environ.get("ZCC", "./zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_opt.c", "w") as f:
            f.write(prog)
        
        # GCC oracle
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_opt.c", "-o", "repro_gcc_opt"], capture_output=True)
        if gcc_rc.returncode != 0: continue
            
        gcc_run = subprocess.run(["./repro_gcc_opt"], capture_output=True)
        oracle_exit = gcc_run.returncode
        
        # ZCC OPT=0
        env_raw = os.environ.copy()
        env_raw["ZCC_OPT"] = "0"
        zcc_raw_rc = subprocess.run([ZCC, "repro_opt.c", "-o", "repro_zcc_raw.s"], env=env_raw, capture_output=True)
        if zcc_raw_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH (RAW)! Seed: {seed}")
            sys.exit(1)
            
        subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_raw.s", "-o", "repro_zcc_raw"], capture_output=True)
        zcc_raw_run = subprocess.run(["./repro_zcc_raw"], capture_output=True)
        raw_exit = zcc_raw_run.returncode
        
        # ZCC OPT=1
        env_opt = os.environ.copy()
        env_opt["ZCC_OPT"] = "1"
        zcc_opt_rc = subprocess.run([ZCC, "repro_opt.c", "-o", "repro_zcc_opt.s"], env=env_opt, capture_output=True)
        if zcc_opt_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH (OPT)! Seed: {seed}")
            sys.exit(1)
            
        subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_opt.s", "-o", "repro_zcc_opt"], capture_output=True)
        zcc_opt_run = subprocess.run(["./repro_zcc_opt"], capture_output=True)
        opt_exit = zcc_opt_run.returncode
        
        if not (oracle_exit == raw_exit == opt_exit):
            h = hashlib.sha256(prog.encode()).hexdigest()
            print(f"\n[!] OPTIMIZER MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {oracle_exit}")
            print(f"    ZCC RAW     : {raw_exit}")
            print(f"    ZCC OPT     : {opt_exit}")
            print(f"    Address Manifold: {h}")
            sys.exit(1)
            
        if (i+1) % 10 == 0:
            h = hashlib.sha256(prog.encode()).hexdigest()[:8]
            sys.stdout.write(f"\r[ZCC-HOOK-14] Validated {i+1} optimizer topologies. Last Hash: {h}...")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-14] COURTROOM ADJOURNED. Optimizer Warden constraints met.")

if __name__ == "__main__":
    iters = 100
    if len(sys.argv) > 1: iters = int(sys.argv[1])
    run_fuzzer(iters)
