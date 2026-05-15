import os
import sys
import random
import subprocess
import hashlib

# [ZCC-HOOK-12] Variadic ABI Courtroom Fuzzer
ELEM_TYPES = ["char", "short", "int", "long", "long long"]

def gen_many_args_test(idx):
    num_args = random.randint(5, 12)
    arg_types = [random.choice(ELEM_TYPES) for _ in range(num_args)]
    arg_vals = [random.randint(-100, 100) for _ in range(num_args)]
    params = ", ".join(f"{t} a{i}" for i, t in enumerate(arg_types))
    args = ", ".join(str(v) for v in arg_vals)
    sum_expr = " + ".join(f"(int)a{i}" for i in range(num_args))
    expected_sum = sum(arg_vals)
    
    helper = f"""static int helper_many_args_{idx}({params}) {{
    return {sum_expr};
}}"""
    code = f"""    {{
        int r = helper_many_args_{idx}({args});
        if (r != {expected_sum}) return 110 + {idx};
        result += r & 0xFF;
    }}"""
    return code, helper

def gen_struct_val_test(idx):
    num_fields = random.randint(2, 6)
    fields = [(random.choice(ELEM_TYPES), f"f{i}") for i in range(num_fields)]
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
}}"""
    code = f"""    {{
        struct S_{idx} s = {{{field_inits}}};
        int r = helper_struct_val_{idx}(s);
        if (r != {expected_sum}) return 120 + {idx};
        result += r & 0xFF;
    }}"""
    return code, helper

def gen_struct_ret_test(idx):
    num_fields = random.randint(2, 5)
    fields = [(random.choice(["int", "short", "char", "long"]), f"f{i}") for i in range(num_fields)]
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
}}"""
    expected_f0 = init_vals[0]
    
    code = f"""    {{
        struct SR_{idx} s = helper_struct_ret_{idx}();
        if (s.f0 != {expected_f0}) return 130 + {idx};
        result += s.f0 & 0xFF;
    }}"""
    return code, helper

def gen_variadic_test(idx):
    num_args = random.randint(3, 10)
    arg_types = [random.choice(ELEM_TYPES) for _ in range(num_args)]
    arg_vals = [random.randint(-50, 50) for _ in range(num_args)]
    args = ", ".join(f"({t}){v}" for t, v in zip(arg_types, arg_vals))
    
    # Generate the va_arg reads
    reads = []
    for t in arg_types:
        # Default argument promotions: char/short -> int
        read_type = "int" if t in ["char", "short"] else t
        reads.append(f"va_arg(ap, {read_type})")
        
    read_expr = " + ".join(f"(int)({r})" for r in reads)
    expected_sum = sum(arg_vals)
    
    helper = f"""#include <stdarg.h>
static int helper_variadic_{idx}(int count, ...) {{
    va_list ap;
    va_start(ap, count);
    int s = {read_expr};
    va_end(ap);
    return s;
}}"""
    code = f"""    {{
        int r = helper_variadic_{idx}({num_args}, {args});
        if (r != {expected_sum}) return 140 + {idx};
        result += r & 0xFF;
    }}"""
    return code, helper

def generate_program():
    helpers = []
    tests = []
    generators = [gen_many_args_test, gen_struct_val_test, gen_struct_ret_test, gen_variadic_test]
    
    num_tests = random.randint(4, 8)
    for i in range(num_tests):
        code, helper = random.choice(generators)(i)
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
    print("[ZCC-HOOK-12] VARIADIC COURTROOM INITIATED (Targeting %d universes)." % iterations)
    ZCC = os.environ.get("ZCC", "../zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_v2.c", "w") as f:
            f.write(prog)
        
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_v2.c", "-o", "repro_gcc_v2"], capture_output=True)
        if gcc_rc.returncode != 0: continue
            
        gcc_run = subprocess.run(["./repro_gcc_v2"], capture_output=True)
        oracle_exit = gcc_run.returncode
        
        zcc_rc = subprocess.run([ZCC, "repro_v2.c", "-o", "repro_zcc_v2.s"], capture_output=True)
        if zcc_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH! Seed: {seed}")
            sys.exit(1)
            
        link_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_v2.s", "-o", "repro_zcc_v2"], capture_output=True)
        if link_rc.returncode != 0:
            print(f"\n[!] LINK FAILURE! Seed: {seed}")
            sys.exit(1)
            
        zcc_run = subprocess.run(["./repro_zcc_v2"], capture_output=True)
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
            sys.stdout.write(f"\r[ZCC-HOOK-12] Validated {i+1} ABI timelines. Last Hash: {h}...")
            sys.stdout.flush()
            
    print("\n[ZCC-HOOK-12] COURTROOM ADJOURNED. All ABI timelines converged.")

if __name__ == "__main__":
    iters = 100
    if len(sys.argv) > 1: iters = int(sys.argv[1])
    run_fuzzer(iters)
