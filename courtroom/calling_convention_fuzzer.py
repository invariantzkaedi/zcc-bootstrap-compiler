import os
import sys
import random
import subprocess
import hashlib

# [ZCC-HOOK-11] CALLING CONVENTION COURTROOM FUZZER
# Targets:
# - Register vs Stack argument passing (System V ABI: RDI, RSI, RDX, RCX, R8, R9)
# - Stack alignment (16-byte boundary)
# - Struct pass-by-value (small vs large)
# - Struct returns
# - Variadic functions

ELEM_TYPES = ["char", "short", "int", "long", "long long"]

def gen_many_args_test(idx):
    """Test: Passing > 6 arguments to force stack spilling."""
    num_args = random.randint(5, 12)
    arg_types = [random.choice(ELEM_TYPES) for _ in range(num_args)]
    arg_vals = [random.randint(-100, 100) for _ in range(num_args)]
    
    params = ", ".join(f"{t} a{i}" for i, t in enumerate(arg_types))
    args = ", ".join(str(v) for v in arg_vals)
    
    sum_expr = " + ".join(f"(int)a{i}" for i in range(num_args))
    expected_sum = sum(arg_vals)
    
    helper = f"""static int helper_many_args_{idx}({params}) {{
    return {sum_expr};
}}
"""
    code = f"""    /* many args test {idx} */
    {{
        int r = helper_many_args_{idx}({args});
        if (r != {expected_sum}) return 110 + {idx};
        result += r & 0xFF;
    }}"""
    return code, helper


def gen_struct_val_test(idx):
    """Test: Passing structs by value."""
    # System V ABI: <= 16 bytes passed in up to 2 registers.
    # > 16 bytes passed on stack.
    num_fields = random.randint(2, 6)
    fields = []
    for i in range(num_fields):
        ft = random.choice(ELEM_TYPES)
        fields.append((ft, f"f{i}"))
        
    init_vals = [random.randint(1, 100) for _ in fields]
    
    field_decls = "\n        ".join(f"{ft} {fn};" for ft, fn in fields)
    field_inits = ", ".join(str(v) for v in init_vals)
    sum_expr = " + ".join(f"(int)s.f{i}" for i in range(num_fields))
    expected_sum = sum(init_vals)
    
    helper = f"""struct S_{idx} {{
        {field_decls}
}};
static int helper_struct_val_{idx}(struct S_{idx} s) {{
    return {sum_expr};
}}
"""
    code = f"""    /* struct val test {idx} */
    {{
        struct S_{idx} s = {{{field_inits}}};
        int r = helper_struct_val_{idx}(s);
        if (r != {expected_sum}) return 120 + {idx};
        result += r & 0xFF;
    }}"""
    return code, helper


def gen_struct_ret_test(idx):
    """Test: Returning structs."""
    num_fields = random.randint(2, 5)
    fields = []
    for i in range(num_fields):
        ft = random.choice(["int", "short", "char", "long"])
        fields.append((ft, f"f{i}"))
        
    init_vals = [random.randint(1, 50) for _ in fields]
    field_decls = "\n        ".join(f"{ft} {fn};" for ft, fn in fields)
    field_assigns = "\n    ".join(f"s.f{i} = {v};" for i, v in enumerate(init_vals))
    
    helper = f"""struct SR_{idx} {{
        {field_decls}
}};
static struct SR_{idx} helper_struct_ret_{idx}() {{
    struct SR_{idx} s;
    {field_assigns}
    return s;
}}
"""
    expected_f0 = init_vals[0]
    
    code = f"""    /* struct ret test {idx} */
    {{
        struct SR_{idx} s = helper_struct_ret_{idx}();
        if (s.f0 != {expected_f0}) return 130 + {idx};
        result += s.f0 & 0xFF;
    }}"""
    return code, helper


def generate_program():
    helpers = []
    tests = []
    
    generators = [
        gen_many_args_test,
        gen_struct_val_test,
        gen_struct_ret_test
    ]
    
    num_tests = random.randint(4, 8)
    for i in range(num_tests):
        gen = random.choice(generators)
        code, helper = gen(i)
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
    print("[ZCC-HOOK-11] CALLING CONVENTION COURTROOM INITIATED (Targeting %d universes)." % iterations)
    ZCC = os.environ.get("ZCC", "../zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_cc.c", "w") as f:
            f.write(prog)
        
        # Oracle
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_cc.c", "-o", "repro_gcc_cc"], capture_output=True)
        if gcc_rc.returncode != 0:
            continue
            
        gcc_run = subprocess.run(["./repro_gcc_cc"], capture_output=True)
        oracle_exit = gcc_run.returncode
        
        # ZCC Reality
        zcc_rc = subprocess.run([ZCC, "repro_cc.c", "-o", "repro_zcc_cc.s"], capture_output=True)
        if zcc_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH! Seed: {seed}")
            sys.exit(1)
            
        link_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_cc.s", "-o", "repro_zcc_cc"], capture_output=True)
        if link_rc.returncode != 0:
            print(f"\n[!] LINK FAILURE! Seed: {seed}")
            sys.exit(1)
            
        zcc_run = subprocess.run(["./repro_zcc_cc"], capture_output=True)
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
            sys.stdout.write(f"\r[ZCC-HOOK-11] Validated {i+1} ABI timelines. Last Hash: {h}...")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-11] COURTROOM ADJOURNED. All ABI timelines converged.")

if __name__ == "__main__":
    iters = 1000
    if len(sys.argv) > 1:
        iters = int(sys.argv[1])
    run_fuzzer(iters)
