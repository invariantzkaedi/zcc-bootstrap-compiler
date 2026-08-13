#!/usr/bin/env python3
import os
import sys
import random
import subprocess

# Task 3: Differential Float Fuzzing Harness
# Generates random float calculations in both Rust (for ZCC) and C (for GCC),
# compiles, runs, and asserts zero output/exit-code discrepancies.

FLOAT_VALS = ["0.5", "1.25", "2.5", "3.0", "4.5", "5.0", "10.0", "0.25", "8.0"]
OPS = ["+", "-", "*", "/"]
COMP_OPS = [">", "<", ">=", "<=", "==", "!="]

def gen_float_expr(depth, is_f64):
    suffix = "f64" if is_f64 else "f32"
    c_suffix = "" if is_f64 else "f"
    
    if depth == 0:
        val = random.choice(FLOAT_VALS)
        # Randomly choose variable or literal
        choice = random.choice(["x", "y", "lit"])
        if choice == "x":
            return "x", "x"
        elif choice == "y":
            return "y", "y"
        else:
            return f"{val}{suffix}", f"{val}{c_suffix}"
            
    op = random.choice(OPS)
    lhs_rs, lhs_c = gen_float_expr(depth - 1, is_f64)
    rhs_rs, rhs_c = gen_float_expr(depth - 1, is_f64)
    
    # Avoid division by zero by forcing divisor to be a safe constant
    if op == "/":
        rhs_rs = f"2.0{suffix}"
        rhs_c = f"2.0{c_suffix}"
        
    return f"({lhs_rs} {op} {rhs_rs})", f"({lhs_c} {op} {rhs_c})"

def generate_codes(is_f64, depth):
    ty_rs = "f64" if is_f64 else "f32"
    ty_c = "double" if is_f64 else "float"
    suffix = "f64" if is_f64 else "f32"
    c_suffix = "" if is_f64 else "f"
    
    # Inputs
    x_val = random.choice(FLOAT_VALS)
    y_val = random.choice(FLOAT_VALS)
    
    # Math expression
    expr_rs, expr_c = gen_float_expr(depth, is_f64)
    
    # Comparison
    comp_op = random.choice(COMP_OPS)
    comp_val = random.choice(FLOAT_VALS)
    
    # Rust source
    rust_code = f"""
fn test_func(x: {ty_rs}, y: {ty_rs}) -> i32 {{
    let res: {ty_rs} = {expr_rs};
    if res {comp_op} {comp_val}{suffix} {{
        return 42;
    }} else {{
        return 7;
    }}
}}

fn main() -> i32 {{
    return test_func({x_val}{suffix}, {y_val}{suffix});
}}
"""

    # C source
    c_code = f"""
int test_func({ty_c} x, {ty_c} y) {{
    {ty_c} res = {expr_c};
    if (res {comp_op} {comp_val}{c_suffix}) {{
        return 42;
    }} else {{
        return 7;
    }}
}}

int main() {{
    return test_func({x_val}{c_suffix}, {y_val}{c_suffix});
}}
"""
    return rust_code, c_code

def main():
    iterations = 50
    if len(sys.argv) > 1:
        iterations = int(sys.argv[1])
        
    seed = random.randint(1, 999999999)
    if len(sys.argv) > 2:
        seed = int(sys.argv[2])
    random.seed(seed)
    
    print(f"[FLOAT-FUZZ] Initiating differential float fuzzing: {iterations} iterations, seed={seed}")
    
    ZCC = "./zcc"
    temp_rs = "tests/rust/temp_fuzz.rs"
    temp_c = "tests/rust/temp_fuzz.c"
    temp_gcc_bin = "tests/rust/temp_gcc_bin"
    temp_zcc_s = "tests/rust/temp_zcc.s"
    temp_zcc_bin = "tests/rust/temp_zcc_bin"
    
    for i in range(iterations):
        is_f64 = random.choice([True, False])
        depth = random.randint(1, 3)
        
        rust_src, c_src = generate_codes(is_f64, depth)
        
        # Write files
        with open(temp_rs, "w") as f:
            f.write(rust_src)
        with open(temp_c, "w") as f:
            f.write(c_src)
            
        # 1. Compile and run GCC reference
        gcc_rc = subprocess.run(["gcc", "-w", "-O0", temp_c, "-o", temp_gcc_bin, "-lm"], capture_output=True)
        if gcc_rc.returncode != 0:
            print(f"GCC compilation failed:\n{gcc_rc.stderr.decode()}")
            sys.exit(1)
            
        gcc_run = subprocess.run([f"./{temp_gcc_bin}"], capture_output=True)
        expected_exit = gcc_run.returncode
        
        # 2. Compile and run ZCC
        # Compile Rust to assembly
        zcc_rc = subprocess.run([ZCC, temp_rs, "--rust-backend-v1", "-o", temp_zcc_s], env={**os.environ, "ZCC_IR_BACKEND": "1"}, capture_output=True)
        if zcc_rc.returncode != 0:
            print(f"[!] ZCC compiler crash on iteration {i}!")
            print(f"Rust Source:\n{rust_src}")
            print(f"ZCC stderr:\n{zcc_rc.stderr.decode()}")
            sys.exit(1)
            
        # Assemble with GCC
        as_rc = subprocess.run(["gcc", "-no-pie", "-fno-pie", temp_zcc_s, "-o", temp_zcc_bin, "-lm"], capture_output=True)
        if as_rc.returncode != 0:
            print(f"[!] GCC assembler failed on assembly emitted by ZCC!")
            print(f"ZCC stderr:\n{zcc_rc.stderr.decode()}")
            print(f"Assembler stderr:\n{as_rc.stderr.decode()}")
            sys.exit(1)
            
        zcc_run = subprocess.run([f"./{temp_zcc_bin}"], capture_output=True)
        actual_exit = zcc_run.returncode
        
        # 3. Verify
        if expected_exit != actual_exit:
            print(f"[!] MISMATCH on iteration {i}!")
            print(f"Rust Source:\n{rust_src}")
            print(f"C Source:\n{c_src}")
            print(f"Expected (GCC): {expected_exit}")
            print(f"Actual (ZCC): {actual_exit}")
            sys.exit(1)
            
    # Clean up
    for f in [temp_rs, temp_c, temp_gcc_bin, temp_zcc_s, temp_zcc_bin]:
        if os.path.exists(f):
            os.remove(f)
            
    print(f"[FLOAT-FUZZ] SUCCESS: {iterations} iterations executed with zero discrepancies.")
    sys.exit(0)

if __name__ == "__main__":
    main()
