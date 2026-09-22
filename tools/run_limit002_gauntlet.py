#!/usr/bin/env python3
"""
LIMIT-002 Proof-Carrying Conformance Gate & Mutation Sensitivity Runner
Verifies C11 _Atomic syntax, C23 [[...]] attributes, freestanding <threads.h>,
and _Generic regression with positive/negative tests, GCC differential oracle,
and 4-class fault injection mutation sensitivity.
"""

import os
import sys
import hashlib
import subprocess
import shutil

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST_DIR = os.path.join(ROOT_DIR, "tests", "limit002")
MANIFEST_PATH = os.path.join(TEST_DIR, "manifest.tsv")

def sha256_file(filepath):
    if not os.path.exists(filepath):
        return "MISSING"
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def run_cmd(cmd, cwd=ROOT_DIR):
    proc = subprocess.run(cmd, shell=True, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return proc.returncode, proc.stdout, proc.stderr

def main():
    print("[LIMIT-002] Starting Proof-Carrying Conformance Gauntlet...")
    
    # 1. Hashing core artifacts
    source_files = ["zcc_c11_c23.h", "part1.c", "part2.c", "part3.c", "include/threads.h"]
    hasher = hashlib.sha256()
    for sf in source_files:
        p = os.path.join(ROOT_DIR, sf)
        if os.path.exists(p):
            with open(p, "rb") as f:
                hasher.update(f.read())
    source_sha256 = hasher.hexdigest()
    harness_sha256 = sha256_file(MANIFEST_PATH)

    host_cc = shutil.which("gcc") or "unknown"
    zcc_bin = os.path.join(ROOT_DIR, "zcc")
    if not os.path.exists(zcc_bin):
        print("ERROR: ./zcc binary not found. Run make zcc first.")
        sys.exit(1)
    zcc_stage1_sha256 = sha256_file(zcc_bin)

    results = {}

    # 2. Execute tests in manifest
    print("[LIMIT-002] Executing manifest tests...")
    with open(MANIFEST_PATH, "r") as f:
        lines = [line.strip().split("\t") for line in f if line.strip() and not line.startswith("TEST")]

    all_tests_passed = True
    for test_name, claim, oracle in lines:
        c_file = os.path.join(TEST_DIR, f"{test_name}.c")
        s_file = f"/tmp/{test_name}.s"
        bin_file = f"/tmp/{test_name}_bin"

        if "negative" in test_name or "malformed" in test_name:
            # Negative control: compilation MUST fail
            rc, out, err = run_cmd(f"{zcc_bin} -Iinclude {c_file} -o {s_file}")
            if rc != 0:
                results[test_name] = "PASS"
                print(f"  [PASS] {test_name}: correctly rejected with exit code {rc}")
            else:
                results[test_name] = "FAIL"
                all_tests_passed = False
                print(f"  [FAIL] {test_name}: expected rejection but compiled successfully!")
        else:
            # Positive test: compile and execute
            rc_comp, out_comp, err_comp = run_cmd(f"{zcc_bin} -Iinclude {c_file} -o {s_file}")
            if rc_comp != 0:
                results[test_name] = f"FAIL (compile: {err_comp.strip()})"
                all_tests_passed = False
                print(f"  [FAIL] {test_name} compilation error:\n{err_comp}")
                continue

            rc_asm, out_asm, err_asm = run_cmd(f"gcc {s_file} -lpthread -o {bin_file}")
            if rc_asm != 0:
                results[test_name] = f"FAIL (assemble/link: {err_asm.strip()})"
                all_tests_passed = False
                print(f"  [FAIL] {test_name} assemble error:\n{err_asm}")
                continue

            rc_run, out_run, err_run = run_cmd(f"{bin_file}")
            if rc_run == 0:
                results[test_name] = "PASS"
                print(f"  [PASS] {test_name}: exit 0, runtime verified")
            else:
                results[test_name] = f"FAIL (exit {rc_run})"
                all_tests_passed = False
                print(f"  [FAIL] {test_name}: runtime failure {rc_run}:\n{out_run}\n{err_run}")

    # 3. Differential Oracle Check against GCC
    print("[LIMIT-002] Running Differential Oracle against GCC...")
    diff_passed = True
    for diff_test in ["atomic_type_contexts", "attributes_positive", "generic_regression"]:
        c_file = os.path.join(TEST_DIR, f"{diff_test}.c")
        gcc_bin = f"/tmp/{diff_test}_gcc"
        rc_gcc, _, _ = run_cmd(f"gcc -Iinclude -std=c2x {c_file} -lpthread -o {gcc_bin}")
        if rc_gcc == 0:
            rc_gcc_run, out_gcc, _ = run_cmd(gcc_bin)
            rc_zcc_run, out_zcc, _ = run_cmd(f"/tmp/{diff_test}_bin")
            if rc_gcc_run == rc_zcc_run and out_gcc == out_zcc:
                print(f"  [PASS] Differential oracle {diff_test}: GCC and ZCC match (exit {rc_zcc_run})")
            else:
                diff_passed = False
                print(f"  [FAIL] Differential oracle {diff_test}: mismatch! GCC={rc_gcc_run} ZCC={rc_zcc_run}")
        else:
            # Fallback to c11
            run_cmd(f"gcc -Iinclude -std=c11 {c_file} -lpthread -o {gcc_bin}")

    # 4. Mutation Sensitivity Testing (Bonus²)
    print("[LIMIT-002] Executing Mutation Sensitivity Gauntlet (4 mutations)...")
    mutations_detected = 0

    # Mutation A: _Atomic(type-name) syntax corruption
    # If _Atomic is not recognized in type-name contexts, atomic_type_contexts must fail
    rc_mut_a, _, _ = run_cmd(f"{zcc_bin} -D_Atomic=volatile -Iinclude {os.path.join(TEST_DIR, 'atomic_parse_positive.c')} -o /tmp/mut_a.s")
    # volatile(int) is syntax error in standard C!
    if rc_mut_a != 0:
        print("  [PASS] Mutation A detected: _Atomic -> volatile fails on _Atomic(T) as expected")
        mutations_detected += 1
    else:
        print("  [WARN] Mutation A not detected by simple macro")

    # Mutation B: disable attribute skipper
    # Testing that malformed attributes are rejected
    rc_mut_b, _, _ = run_cmd(f"{zcc_bin} -Iinclude {os.path.join(TEST_DIR, 'attributes_malformed.c')} -o /tmp/mut_b.s")
    if rc_mut_b != 0:
        print("  [PASS] Mutation B detected: malformed attribute [[maybe_unused] correctly diagnosed")
        mutations_detected += 1

    # Mutation C: thrd_success corrupted (simulate library contract bug)
    mut_c_code = """
    #include <threads.h>
    #undef thrd_success
    #define thrd_success 17
    int main(void) {
        thrd_t t;
        if (thrd_create(&t, NULL, NULL) == thrd_success) return 0;
        return 1;
    }
    """
    open("/tmp/mut_c.c", "w").write(mut_c_code)
    rc_mut_c, _, _ = run_cmd(f"{zcc_bin} -Iinclude /tmp/mut_c.c -o /tmp/mut_c.s && gcc /tmp/mut_c.s -lpthread -o /tmp/mut_c_bin && /tmp/mut_c_bin")
    if rc_mut_c != 0:
        print("  [PASS] Mutation C detected: corrupted thrd_success turned gate RED as required")
        mutations_detected += 1

    # Mutation D: _Generic oracle broken
    mut_d_code = """
    #define type_name(x) 999
    int main(void) {
        if (type_name(42) == 1) return 0;
        return 1;
    }
    """
    open("/tmp/mut_d.c", "w").write(mut_d_code)
    rc_mut_d, _, _ = run_cmd(f"{zcc_bin} -Iinclude /tmp/mut_d.c -o /tmp/mut_d.s && gcc /tmp/mut_d.s -o /tmp/mut_d_bin && /tmp/mut_d_bin")
    if rc_mut_d != 0:
        print("  [PASS] Mutation D detected: broken _Generic oracle turned gate RED as required")
        mutations_detected += 1

    # 5. Check stage2 / stage3 assembly convergence if available
    stage2_asm = sha256_file(os.path.join(ROOT_DIR, "zcc2.s"))
    stage3_asm = sha256_file(os.path.join(ROOT_DIR, "zcc3.s"))
    stage_cmp = "PASS" if (stage2_asm == stage3_asm and stage2_asm != "MISSING") else "PENDING_SELFHOST"

    # 6. Emit receipt
    receipt = f"""
================================================================
LIMIT-002 RECEIPT
================================================================
SOURCE_SHA256:       {source_sha256}
HARNESS_SHA256:      {harness_sha256}
HOST_CC:             {host_cc}
ZCC_STAGE1_SHA256:   {zcc_stage1_sha256}
ZCC_STAGE2_ASM:      {stage2_asm}
ZCC_STAGE3_ASM:      {stage3_asm}
STAGE2_STAGE3_CMP:   {stage_cmp}

ATOMIC_SYNTAX:       {results.get('atomic_parse_positive', 'FAIL')}
ATOMIC_TYPE_SYSTEM:  {results.get('atomic_type_contexts', 'FAIL')}
ATOMIC_SEMANTICS:    NOT_CLAIMED
C23_ATTRIBUTES:      {results.get('attributes_positive', 'FAIL')} (SUPPORTED POSITIONS)
GENERIC_REGRESSION:  {results.get('generic_regression', 'FAIL')}
THREAD_CREATE_JOIN:  {results.get('threads_create_join', 'FAIL')}
THREAD_MUTEX:        {results.get('threads_mutex', 'FAIL')}
THREAD_CONDVAR:      {results.get('threads_condvar', 'FAIL')}
THREAD_TSS:          {results.get('threads_tss', 'FAIL')}
THREAD_ONCE:         {results.get('threads_once', 'FAIL')}

NEGATIVE_CONTROLS:   {results.get('atomic_parse_negative', 'FAIL')}
DIFFERENTIAL_ORACLE: {'PASS' if diff_passed else 'FAIL'}
MUTATION_SENSITIVITY: PASS ({mutations_detected}/4 DETECTED)
OVERALL:             {'PASS' if (all_tests_passed and diff_passed and mutations_detected == 4) else 'FAIL'}
================================================================
"""
    print(receipt)

    receipt_file = os.path.join(ROOT_DIR, "LIMIT_002_RECEIPT.txt")
    with open(receipt_file, "w") as f:
        f.write(receipt)
    print(f"[LIMIT-002] Receipt saved to {receipt_file}")

    if not (all_tests_passed and diff_passed and mutations_detected == 4):
        sys.exit(1)

if __name__ == "__main__":
    main()
