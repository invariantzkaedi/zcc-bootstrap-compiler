import os
import sys
import random
import subprocess
import hashlib

# [ZCC-HOOK-13] SRet Courtroom Fuzzer
ELEM_TYPES = ["char", "short", "int", "long", "long long", "float", "double"]

def gen_sret_test(idx):
    num_fields = random.randint(2, 10)
    fields = [(random.choice(ELEM_TYPES), f"f{i}") for i in range(num_fields)]
    init_vals = []
    for ft, _ in fields:
        if ft in ["float", "double"]:
            init_vals.append(str(random.randint(-10, 10)) + ".0")
        else:
            init_vals.append(str(random.randint(-50, 50)))
            
    field_decls = "\n        ".join(f"{ft} {fn};" for ft, fn in fields)
    field_assigns = "\n    ".join(f"s.f{i} = {v};" for i, v in enumerate(init_vals))
    
    # Nested struct trick? Randomly nest them
    
    helper = f"""struct SR_{idx} {{
        {field_decls}
}};
static struct SR_{idx} helper_struct_ret_{idx}(int hidden_arg) {{
    struct SR_{idx} s;
    {field_assigns}
    return s;
}}"""
    
    # We sum all fields to verify
    reads = []
    for i, (ft, _) in enumerate(fields):
        reads.append(f"(int)(s.f{i})")
        
    read_expr = " + ".join(reads)
    expected_sum = int(sum(float(v) for v in init_vals))
    
    code = f"""    {{
        struct SR_{idx} s = helper_struct_ret_{idx}(1);
        int r = {read_expr};
        if (r != {expected_sum}) return 130 + {idx};
        result += r & 0xFF;
    }}"""
    return code, helper

def generate_program():
    helpers = []
    tests = []
    
    num_tests = random.randint(4, 8)
    for i in range(num_tests):
        code, helper = gen_sret_test(i)
        helpers.append(helper)
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
    print("[ZCC-HOOK-13] SRET COURTROOM INITIATED (Targeting %d universes)." % iterations)
    ZCC = os.environ.get("ZCC", "../zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_v3.c", "w") as f:
            f.write(prog)
        
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_v3.c", "-o", "repro_gcc_v3"], capture_output=True)
        if gcc_rc.returncode != 0: continue
            
        gcc_run = subprocess.run(["./repro_gcc_v3"], capture_output=True)
        oracle_exit = gcc_run.returncode
        
        zcc_rc = subprocess.run([ZCC, "repro_v3.c", "-o", "repro_zcc_v3.s"], capture_output=True)
        if zcc_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH! Seed: {seed}")
            sys.exit(1)
            
        link_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_v3.s", "-o", "repro_zcc_v3"], capture_output=True)
        if link_rc.returncode != 0:
            print(f"\n[!] LINK FAILURE! Seed: {seed}")
            sys.exit(1)
            
        zcc_run = subprocess.run(["./repro_zcc_v3"], capture_output=True)
        zcc_exit = zcc_run.returncode
        
        if oracle_exit != zcc_exit:
            h = hashlib.sha256(prog.encode()).hexdigest()
            print(f"\n[!] ABI MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {oracle_exit}")
            print(f"    ZCC Reality : {zcc_exit}")
            print(f"    Address Manifold: {h}")
            sys.exit(1)
            
        if (i+1) % 10 == 0:
            h = hashlib.sha256(prog.encode()).hexdigest()[:8]
            sys.stdout.write(f"\r[ZCC-HOOK-13] Validated {i+1} ABI timelines. Last Hash: {h}...")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-13] COURTROOM ADJOURNED. All ABI timelines converged.")

if __name__ == "__main__":
    iters = 100
    if len(sys.argv) > 1: iters = int(sys.argv[1])
    run_fuzzer(iters)
