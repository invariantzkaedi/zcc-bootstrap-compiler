import os
import sys
import random
import subprocess
import hashlib

# [ZCC-HOOK-10] POINTER ARITHMETIC COURTROOM FUZZER
# Invariant: ptr + N == base + N * sizeof(*ptr)
#            ptr - ptr == (byte_diff) / sizeof(*ptr)
#            struct member offset == field offset
#            array decay == &arr[0]

# Base element types for pointers
ELEM_TYPES = ["char", "short", "int", "long", "long long"]
ELEM_SIZES = {"char": 1, "short": 2, "int": 4, "long": 8, "long long": 8}

def gen_array_test(idx):
    """Test: array indexing with pointer arithmetic equivalence."""
    etype = random.choice(ELEM_TYPES)
    sz = random.randint(4, 16)
    init_vals = [random.randint(-100, 100) for _ in range(sz)]
    idx_val = random.randint(0, sz - 1)
    
    init_list = ", ".join(str(v) for v in init_vals)
    
    code = f"""    /* array indexing test {idx} */
    {{
        {etype} arr{idx}[{sz}] = {{{init_list}}};
        {etype} *p{idx} = arr{idx};
        {etype} via_index = arr{idx}[{idx_val}];
        {etype} via_ptr   = *(p{idx} + {idx_val});
        {etype} via_deref = p{idx}[{idx_val}];
        if (via_index != via_ptr || via_index != via_deref) return 100 + {idx};
        result += ({etype})(via_index) & 0xFF;
    }}"""
    return code

def gen_ptr_add_test(idx):
    """Test: pointer + integer scaling."""
    etype = random.choice(ELEM_TYPES)
    sz = random.randint(4, 12)
    init_vals = [random.randint(1, 127) for _ in range(sz)]
    step = random.randint(1, sz - 1)
    
    init_list = ", ".join(str(v) for v in init_vals)
    
    code = f"""    /* ptr add test {idx} */
    {{
        {etype} arr[{sz}] = {{{init_list}}};
        {etype} *base = arr;
        {etype} *stepped = base + {step};
        {etype} val_direct = arr[{step}];
        {etype} val_ptr    = *stepped;
        if (val_direct != val_ptr) return 110 + {idx};
        result += val_direct & 0xFF;
    }}"""
    return code

def gen_ptr_sub_test(idx):
    """Test: pointer - pointer yields element count."""
    etype = random.choice(ELEM_TYPES)
    sz = random.randint(4, 12)
    init_vals = [random.randint(1, 127) for _ in range(sz)]
    lo = random.randint(0, sz - 2)
    hi = random.randint(lo + 1, sz - 1)
    expected_diff = hi - lo
    
    init_list = ", ".join(str(v) for v in init_vals)
    
    code = f"""    /* ptr sub test {idx} */
    {{
        {etype} arr[{sz}] = {{{init_list}}};
        {etype} *lo = &arr[{lo}];
        {etype} *hi = &arr[{hi}];
        long diff = hi - lo;
        if (diff != {expected_diff}) return 120 + {idx};
        result += (int)diff & 0xFF;
    }}"""
    return code

def gen_struct_offset_test(idx):
    """Test: struct member access and offsetof equivalence."""
    # Generate a small struct with 2-4 fields of varying types
    num_fields = random.randint(2, 4)
    fields = []
    for i in range(num_fields):
        ft = random.choice(ELEM_TYPES)
        fields.append((ft, f"f{i}"))
    
    init_vals = [random.randint(1, 100) for _ in fields]
    field_idx = random.randint(0, num_fields - 1)
    
    field_decls = "\n        ".join(f"{ft} {fn};" for ft, fn in fields)
    field_inits = ", ".join(str(v) for v in init_vals)
    target_field = fields[field_idx][1]
    target_val = init_vals[field_idx]
    
    code = f"""    /* struct offset test {idx} */
    {{
        struct S{idx} {{
            {field_decls}
        }};
        struct S{idx} s{idx} = {{{field_inits}}};
        struct S{idx} *ps{idx} = &s{idx};
        if (ps{idx}->{target_field} != {target_val}) return 130 + {idx};
        if (s{idx}.{target_field} != ps{idx}->{target_field}) return 140 + {idx};
        result += s{idx}.{target_field} & 0xFF;
    }}"""
    return code

def gen_void_cast_test(idx):
    """Test: void* cast roundtrip preserves address."""
    etype = random.choice(ELEM_TYPES)
    val = random.randint(1, 127)
    
    code = f"""    /* void* cast test {idx} */
    {{
        {etype} x{idx} = {val};
        void *vp{idx} = &x{idx};
        {etype} *tp{idx} = ({etype} *)vp{idx};
        if (*tp{idx} != {val}) return 150 + {idx};
        result += *tp{idx} & 0xFF;
    }}"""
    return code

def gen_array_decay_test(idx):
    """Test: array name decays to &arr[0]."""
    etype = random.choice(ELEM_TYPES)
    val0 = random.randint(1, 100)
    val1 = random.randint(1, 100)
    
    code = f"""    /* array decay test {idx} */
    {{
        {etype} ad{idx}[2] = {{{val0}, {val1}}};
        {etype} *dp{idx} = ad{idx};
        if (*dp{idx} != {val0}) return 160 + {idx};
        if (*(dp{idx} + 1) != {val1}) return 170 + {idx};
        result += *dp{idx} & 0xFF;
    }}"""
    return code

def gen_funcptr_test(idx):
    """Test: function pointer call."""
    ret_val = random.randint(1, 200)
    
    # This generates a static helper + call-via-pointer
    code = f"""    /* funcptr test {idx} */
    {{
        int (*fp{idx})(void) = helper_{idx};
        int fr{idx} = fp{idx}();
        if (fr{idx} != {ret_val}) return 180 + {idx};
        result += fr{idx} & 0xFF;
    }}"""
    helper = f"static int helper_{idx}(void) {{ return {ret_val}; }}\n"
    return code, helper

def gen_nested_ptr_test(idx):
    """Test: pointer to pointer (int **)."""
    val = random.randint(1, 100)
    
    code = f"""    /* nested ptr test {idx} */
    {{
        int np_v{idx} = {val};
        int *np_p{idx} = &np_v{idx};
        int **np_pp{idx} = &np_p{idx};
        if (**np_pp{idx} != {val}) return 190 + {idx};
        result += **np_pp{idx} & 0xFF;
    }}"""
    return code

def gen_struct_ptr_arith_test(idx):
    """Test: pointer arithmetic on struct arrays."""
    val0 = random.randint(1, 100)
    val1 = random.randint(1, 100)
    
    code = f"""    /* struct array ptr test {idx} */
    {{
        struct SP{idx} {{ int a; int b; }};
        struct SP{idx} sarr{idx}[2] = {{{{{val0}, {val0 + 10}}}, {{{val1}, {val1 + 20}}}}};
        struct SP{idx} *sp{idx} = sarr{idx};
        if ((sp{idx} + 1)->a != {val1}) return 200 + {idx};
        if (sp{idx}[1].b != {val1 + 20}) return 210 + {idx};
        result += sp{idx}[0].a & 0xFF;
    }}"""
    return code


def generate_program():
    """Generate a complete test program covering all pointer categories."""
    helpers = []
    tests = []
    
    # Mix of test categories
    generators = [
        gen_array_test,
        gen_ptr_add_test,
        gen_ptr_sub_test,
        gen_struct_offset_test,
        gen_void_cast_test,
        gen_array_decay_test,
        gen_nested_ptr_test,
        gen_struct_ptr_arith_test,
    ]
    
    num_tests = random.randint(4, 8)
    for i in range(num_tests):
        gen = random.choice(generators)
        if gen == gen_funcptr_test:
            code, helper = gen(i)
            helpers.append(helper)
            tests.append(code)
        else:
            tests.append(gen(i))
    
    # Optionally add 1-2 funcptr tests
    if random.random() < 0.5:
        code, helper = gen_funcptr_test(num_tests)
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
    print("[ZCC-HOOK-10] POINTER ARITHMETIC COURTROOM INITIATED (Targeting %d universes)." % iterations)
    ZCC = os.environ.get("ZCC", "../zcc")
    
    for i in range(iterations):
        seed = random.randint(1, 999999999)
        random.seed(seed)
        prog = generate_program()
        
        with open("repro_ptr.c", "w") as f:
            f.write(prog)
        
        # Oracle
        gcc_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_ptr.c", "-o", "repro_gcc_ptr"], capture_output=True)
        if gcc_rc.returncode != 0:
            continue  # Invalid C or GCC rejected
        
        gcc_run = subprocess.run(["./repro_gcc_ptr"], capture_output=True)
        oracle_exit = gcc_run.returncode
        
        # ZCC Reality
        zcc_rc = subprocess.run([ZCC, "repro_ptr.c", "-o", "repro_zcc_ptr.s"], capture_output=True)
        if zcc_rc.returncode != 0:
            print(f"\n[!] COMPILER CRASH! Seed: {seed}")
            print(f"    stderr: {zcc_rc.stderr.decode()[:500]}")
            sys.exit(1)
        
        link_rc = subprocess.run(["gcc", "-O0", "-w", "-fwrapv", "repro_zcc_ptr.s", "-o", "repro_zcc_ptr"], capture_output=True)
        if link_rc.returncode != 0:
            print(f"\n[!] LINK FAILURE! Seed: {seed}")
            print(f"    stderr: {link_rc.stderr.decode()[:500]}")
            sys.exit(1)
        
        zcc_run = subprocess.run(["./repro_zcc_ptr"], capture_output=True)
        zcc_exit = zcc_run.returncode
        
        if oracle_exit != zcc_exit:
            h = hashlib.sha256(prog.encode()).hexdigest()
            print(f"\n[!] POINTER MISMATCH DETECTED! Seed: {seed}")
            print(f"    Oracle (GCC): {oracle_exit}")
            print(f"    ZCC Reality : {zcc_exit}")
            print(f"    Address Manifold: {h}")
            print("Saved to repro_ptr.c")
            sys.exit(1)
        
        if (i+1) % 10 == 0:
            h = hashlib.sha256(prog.encode()).hexdigest()[:8]
            sys.stdout.write(f"\r[ZCC-HOOK-10] Validated {i+1} pointer timelines. Last Hash: {h}...")
            sys.stdout.flush()
    
    print("\n[ZCC-HOOK-10] COURTROOM ADJOURNED. All pointer timelines converged.")

if __name__ == "__main__":
    iters = 1000
    if len(sys.argv) > 1:
        iters = int(sys.argv[1])
    run_fuzzer(iters)
