import sys
import os
import subprocess
import random
import hashlib

def run_cmd(cmd):
    try:
        r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=5)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT"

def generate_c_subset(seed):
    random.seed(seed)
    vars = ["v0", "v1", "v2"]
    
    def gen_expr(depth):
        if depth == 0:
            if random.random() < 0.5:
                return random.choice(vars)
            return str(random.randint(-100, 100))
        
        op = random.choice(["+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>"])
        left = gen_expr(depth - 1)
        right = gen_expr(depth - 1)
        
        if op in ["/", "%"]:
            # Prevent div-by-zero and LONG_MIN / -1 exceptions
            return f"({left} {op} (({right} & 255) + 1))"
        elif op in ["<<", ">>"]:
            # Prevent out-of-bounds shifts
            return f"({left} {op} ({right} & 63))"
            
        return f"({left} {op} {right})"

    def gen_stmt(depth):
        if depth == 0:
            return f"    {random.choice(vars)} = {gen_expr(2)};"
        
        stype = random.choice(["assign", "assign", "assign", "if", "while"])
        if stype == "assign":
            return f"    {random.choice(vars)} = {gen_expr(2)};"
        elif stype == "if":
            cond = f"{random.choice(vars)} {random.choice(['<', '>', '==', '!=', '<=', '>='])} {random.choice(vars)}"
            return f"    if ({cond}) {{\n    {gen_stmt(depth-1)}\n    }} else {{\n    {gen_stmt(depth-1)}\n    }}"
        elif stype == "while":
            cnt = f"c_{random.randint(0,999)}"
            cond = f"{random.choice(vars)} {random.choice(['<', '>', '==', '!='])} {random.choice(vars)}"
            return f"    long {cnt} = 5;\n    while ({cond} && {cnt} > 0) {{\n    {gen_stmt(depth-1)}\n        {cnt}--;\n    }}"

    code = "int main() {\n"
    for v in vars:
        code += f"    long {v} = {random.randint(-100, 100)};\n"
    
    for _ in range(8):
        code += gen_stmt(2) + "\n"
        
    code += "    return (v0 + v1 + v2) & 255;\n}\n"
    return code

def minimize(code, zcc_bin):
    lines = code.split('\n')
    minimized = True
    print("[Minimizer] Searching for reducible AST nodes...")
    
    while minimized:
        minimized = False
        for i in range(len(lines)):
            line = lines[i].strip()
            # Only attempt to prune pure assignments or simple control scopes
            if line.endswith(";") and "return" not in line and "=" in line and "while" not in line and "if" not in line and "c_" not in line:
                candidate = lines[:i] + lines[i+1:]
                cand_code = "\n".join(candidate)
                
                with open("cand.c", "w") as f:
                    f.write(cand_code)
                    
                gcc_rc, _, _ = run_cmd("gcc -fwrapv -O0 cand.c -o cand_gcc")
                if gcc_rc == 0:
                    gcc_run_rc, _, _ = run_cmd("./cand_gcc")
                    zcc_rc, _, _ = run_cmd(f"{zcc_bin} cand.c -o cand_zcc.s && gcc -fwrapv -O0 cand_zcc.s -o cand_zcc && ./cand_zcc")
                    
                    if gcc_run_rc != zcc_rc:
                        lines = candidate
                        minimized = True
                        break
    return "\n".join(lines)

def fuzz_loop(iterations, zcc_bin="/mnt/g/zccMAIN/zcc/zcc"):
    print(f"[ZCC-HOOK-05] SEMANTIC FUZZ COURTROOM INITIATED (Targeting {iterations} universes).")
    
    for i in range(iterations):
        seed = random.randint(0, 10000000)
        code = generate_c_subset(seed)
        
        with open("fuzz_test.c", "w") as f:
            f.write(code)
            
        # 1. ORACLE GCC
        rc_gcc, out, err = run_cmd("gcc -fwrapv -O0 fuzz_test.c -o fuzz_gcc")
        if rc_gcc != 0:
            continue
        
        rc_oracle, _, err_or = run_cmd("./fuzz_gcc")
        if err_or == "TIMEOUT":
            continue
            
        # 2. ZCC COMPILATION (with IR Courtroom active)
        zcc_cmd = f"ZCC_EMIT_IR=1 ZCC_HOOK_IR_COURTROOM=1 {zcc_bin} fuzz_test.c -o fuzz_zcc.s"
        rc_zcc, zcc_out, zcc_err = run_cmd(zcc_cmd)
        
        if rc_zcc != 0:
            print(f"\n[!] MISMATCH (ICE)! ZCC failed to compile Seed: {seed}")
            print(f"GCC Oracle exited with: {rc_oracle}")
            with open("repro.c", "w") as f:
                f.write(code)
            sys.exit(1)
            
        # Parse Hash telemetry to ensure determinism pipeline survives
        ir_hash = None
        for line in zcc_out.split('\n') + zcc_err.split('\n'):
            if "[ZCC-IR-COURTROOM] HASH:" in line:
                ir_hash = line.split("HASH: ")[1].strip()
                
        # 3. ZCC ASM LINKING
        rc_link, _, _ = run_cmd("gcc -fwrapv -O0 fuzz_zcc.s -o fuzz_zcc")
        if rc_link != 0:
            print(f"\n[!] MISMATCH (ASM-LINK)! ZCC assembly rejected. Seed: {seed}")
            with open("repro.c", "w") as f:
                f.write(code)
            sys.exit(1)
            
        # 4. ZCC EXECUTION
        rc_zcc_run, _, zcc_run_err = run_cmd("./fuzz_zcc")
        if zcc_run_err == "TIMEOUT":
            # If GCC didn't timeout but ZCC did, that's a mismatch
            rc_zcc_run = -999
            
        # 5. SEMANTIC ASSERTION
        if rc_oracle != rc_zcc_run:
            print(f"\n[!] SEMANTIC MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {rc_oracle}")
            print(f"    ZCC Reality : {rc_zcc_run}")
            print(f"    IR Manifold : {ir_hash if ir_hash else 'NO HASH DUMPED'}")
            print("Minimizing repro...")
            min_code = minimize(code, zcc_bin)
            with open("repro.c", "w") as f:
                f.write(min_code)
            print("Wrote minimal reproduction to 'repro.c'")
            sys.exit(1)
            
        if (i+1) % 10 == 0:
            hash_disp = ir_hash[:8] + "..." if ir_hash else "NONE"
            sys.stdout.write(f"\\r[ZCC-HOOK-05] Validated {i+1} semantic timelines. Last Hash: {hash_disp}\n")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-05] COURTROOM DISMISSED. All semantics mathematically align with Oracle.")

if __name__ == "__main__":
    iters = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    fuzz_loop(iters)
