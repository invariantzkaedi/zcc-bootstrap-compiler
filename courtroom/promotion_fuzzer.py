import os
import sys
import random
import subprocess
import hashlib
import time

# [ZCC-HOOK-07] PROMOTION COURTROOM FUZZER
# Invariant: AST type == IR type == emitted opcode width == ABI result width

TYPES = [
    "char", "unsigned char",
    "short", "unsigned short",
    "int", "unsigned int",
    "long", "unsigned long",
    "signed char", "long long"
]

OPS = ["+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>"]

def gen_expr(depth):
    if depth == 0:
        val = random.randint(-1000, 1000)
        t = random.choice(TYPES)
        if "unsigned" in t and val < 0:
            val = abs(val)
        return f"(({t}){val})"
    
    op = random.choice(OPS)
    lhs = gen_expr(depth - 1)
    rhs = gen_expr(depth - 1)
    
    # Avoid div/mod by zero or tricky UB
    if op in ["/", "%"]:
        rhs = f"({rhs} == 0 ? 1 : {rhs})"
    elif op == "<<":
        lhs = f"({lhs} < 0 ? -({lhs}) : {lhs})"
        rhs = f"({rhs} & 30)"
    elif op == ">>":
        rhs = f"({rhs} & 31)"
    return f"({lhs} {op} {rhs})"

def generate_program():
    vars_code = []
    num_vars = random.randint(2, 5)
    var_names = []
    
    for i in range(num_vars):
        v_type = random.choice(TYPES)
        v_name = f"v{i}"
        expr = gen_expr(random.randint(0, 2))
        vars_code.append(f"    {v_type} {v_name} = {expr};")
        var_names.append(v_name)
    
    ret_type = random.choice(TYPES)
    
    combine_op = random.choice(["+", "^", "&", "/", "%", ">>"])
    
    if combine_op in ["/", "%"]:
        ret_expr = f"({var_names[0]} == 0 ? 1 : {var_names[0]}) {combine_op} ({var_names[1]} == 0 ? 1 : {var_names[1]})"
    elif combine_op == ">>":
        ret_expr = f"{var_names[0]} {combine_op} ({var_names[1]} & 31)"
    else:
        ret_expr = f" {combine_op} ".join(var_names)
    
    prog = f"""
int main() {{
{chr(10).join(vars_code)}
    {ret_type} ret_val = ({ret_expr});
    return (int)(ret_val & 255);
}}
"""
    return prog

def run_fuzzer(iterations):
    print("[ZCC-HOOK-07] PROMOTION COURTROOM INITIATED (Targeting %d universes)." % iterations)
    ZCC = os.environ.get("ZCC", "../zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_promo.c", "w") as f:
            f.write(prog)
            
        # Oracle
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_promo.c", "-o", "repro_gcc_promo"], capture_output=True)
        if gcc_rc.returncode != 0:
            continue # Invalid C or GCC rejected
        
        gcc_run = subprocess.run(["./repro_gcc_promo"], capture_output=True)
        oracle_exit = gcc_run.returncode
        
        # ZCC Reality
        zcc_rc = subprocess.run([ZCC, "repro_promo.c", "-o", "repro_zcc_promo.s"], capture_output=True)
        if zcc_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH! Seed: {seed}")
            sys.exit(1)
            
        subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_promo.s", "-o", "repro_zcc_promo"], capture_output=True)
        zcc_run = subprocess.run(["./repro_zcc_promo"], capture_output=True)
        zcc_exit = zcc_run.returncode
        
        if oracle_exit != zcc_exit:
            h = hashlib.sha256(prog.encode()).hexdigest()
            print(f"\n[!] PROMOTION MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {oracle_exit}")
            print(f"    ZCC Reality : {zcc_exit}")
            print(f"    Type Manifold: {h}")
            print("Saved to repro_promo.c")
            sys.exit(1)
            
        if (i+1) % 10 == 0:
            h = hashlib.sha256(prog.encode()).hexdigest()[:8]
            sys.stdout.write(f"\r[ZCC-HOOK-07] Validated {i+1} promotion timelines. Last Hash: {h}...")
            sys.stdout.flush()

    print("\n[ZCC-HOOK-07] COURTROOM ADJOURNED. All timelines converged.")

if __name__ == "__main__":
    iters = 1000
    if len(sys.argv) > 1:
        iters = int(sys.argv[1])
    run_fuzzer(iters)
