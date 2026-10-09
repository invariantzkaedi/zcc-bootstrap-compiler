#!/usr/bin/env python3
import os
import sys
import subprocess
import shutil
import glob
import re

try:
    import z3
except ImportError:
    print("ERROR: Python z3-solver is required to run verification tests.")
    sys.exit(1)

def run_cmd(args, cwd=None):
    res = subprocess.run(
        args,
        shell=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
        cwd=cwd
    )
    return res

def audit_smt_file(path):
    """Audits generated SMT file for duplicate declarations, sanitization correctness, and Z3 parsing."""
    print(f"Auditing: {os.path.basename(path)}")
    
    with open(path, "r") as f:
        content = f.read()

    # 1. Check duplicate declarations
    declared_vars = []
    # Pattern: (declare-fun <name> () ...
    matches = re.findall(r"\(declare-fun\s+([^\s\)]+)", content)
    for var in matches:
        if var in declared_vars:
            print(f"  [FAIL] Duplicate SMT variable declaration: {var}")
            return False
        declared_vars.append(var)

    # 2. Check Z3 parser
    try:
        ctx = z3.Context()
        assertions = z3.parse_smt2_file(path, ctx=ctx)
        solver = z3.Solver(ctx=ctx)
        solver.add(assertions)
        res = solver.check()
        if res != z3.unsat:
            print(f"  [FAIL] Proof not verified. Z3 returned: {res}")
            return False
    except Exception as e:
        print(f"  [FAIL] Z3 SMT-LIB2 Parse Error: {e}")
        return False

    return True

def main():
    repo_dir = "/mnt/h/__DOWNLOADS/zcc_github_upload"
    # Fallback to local directory if not run in WSL
    if not os.path.exists(repo_dir):
        repo_dir = os.path.abspath(os.path.dirname(__file__))

    print("=== SMT Optimization Prover Verification Gauntlet ===")
    
    # Clean previous proofs
    proofs_dir = os.path.join(repo_dir, "test_proofs")
    if os.path.isdir(proofs_dir):
        shutil.rmtree(proofs_dir)

    # Compile verify_smt C harness
    print("Compiling SMT test harness...")
    compile_res = run_cmd(
        [
            "gcc",
            f"-I{repo_dir}",
            os.path.join(repo_dir, "verify_smt.c"),
            os.path.join(repo_dir, "src", "zcc_smt_prover.c"),
            "-lpthread",
            "-o",
            os.path.join(repo_dir, "verify_smt")
        ],
        cwd=repo_dir
    )
    if compile_res.returncode != 0:
        print(f"ERROR: Compilation failed:\nSTDOUT:\n{compile_res.stdout}\nSTDERR:\n{compile_res.stderr}")
        sys.exit(1)

    # Execute test binary to generate SMT proofs
    print("Generating SMT test proofs...")
    run_res = run_cmd([os.path.join(repo_dir, "verify_smt")], cwd=repo_dir)
    if run_res.returncode != 0:
        print(f"ERROR: Generating SMT proofs failed:\nSTDOUT:\n{run_res.stdout}\nSTDERR:\n{run_res.stderr}")
        sys.exit(1)

    # Read generated proofs
    proof_files = glob.glob(os.path.join(proofs_dir, "*.smt2"))
    
    # Verify expected set completeness
    expected_files = {
        "proof_push_pop_replace_line100.smt2",
        "proof_push_pop_replace_line101.smt2",
        "proof_push_pop_replace_line102.smt2",
        "proof_push_pop_replace_line103.smt2",
        "proof_push_pop_replace_line104.smt2",
        "proof_push_pop_replace_line105.smt2",
        "proof_push_pop_elide_line106.smt2",
        "proof_push_pop_replace_line107.smt2",
        "proof_push_pop_replace_line108.smt2",
        "proof_push_lea_pop_triad_line109.smt2",
        "proof_push_pop_replace_line110.smt2",
        "proof_push_pop_replace_line111.smt2",
        "proof_push_pop_replace_line200.smt2",
        "proof_push_pop_replace_line201.smt2",
        "proof_push_pop_replace_line202.smt2",
        "proof_push_pop_replace_line203.smt2",
        # IR Strength Reduction proofs
        "proof_strength_ir_mul2_add_id301.smt2",
        "proof_strength_ir_mul_shl_id302.smt2",
        "proof_strength_ir_div_shr_id303.smt2",
        "proof_strength_ir_mod_band_id304.smt2",
        "proof_strength_ir_mul2_add_id305.smt2",
        # IR Algebraic Peephole proofs
        "proof_peep_ir_peep_self_zero_id401.smt2",
        "proof_peep_ir_peep_self_zero_id402.smt2",
        "proof_peep_ir_peep_self_copy_id403.smt2",
        "proof_peep_ir_peep_band_not_self_id404.smt2",
        "proof_peep_ir_peep_bor_not_self_id405.smt2",
        "proof_peep_ir_peep_double_bnot_id406.smt2",
        "proof_peep_ir_peep_op_zero_id407.smt2",
        "proof_peep_ir_peep_op_one_id408.smt2",
        "proof_peep_ir_peep_op_zero_const_id409.smt2",
        "proof_peep_ir_peep_band_neg1_id410.smt2",
        "proof_peep_ir_peep_bxor_neg1_id411.smt2",
        "proof_peep_ir_peep_cmp_self_id412.smt2",
        "proof_peep_ir_peep_cmp_self_id413.smt2",
        "proof_peep_ir_peep_shift_zero_id414.smt2",
        # Stack alignment proof
        "proof_stack_align_line501.smt2",
    }
    
    actual_files = {os.path.basename(path) for path in proof_files}
    failures = 0

    missing = expected_files - actual_files
    unexpected = actual_files - expected_files

    if missing:
        print(f"[FAIL] Missing expected proofs: {sorted(missing)}")
        failures += len(missing)

    if unexpected:
        print(f"[FAIL] Unexpected proof files: {sorted(unexpected)}")
        failures += len(unexpected)

    for pf in sorted(proof_files):
        if not audit_smt_file(pf):
            failures += 1

    # Specific audits for collision/empty/long cases
    print("\nRunning targeted edge-case verification:")
    
    # 1. Collision audit for %foo-bar vs %foo_bar (line 107)
    proof_107 = os.path.join(proofs_dir, "proof_push_pop_replace_line107.smt2")
    if os.path.exists(proof_107):
        with open(proof_107, "r") as f:
            c = f.read()
        # Verify that %foo-bar is hashed but %foo_bar is not hashed
        # %foo-bar -> foo_bar__<hash>
        # %foo_bar -> foo_bar
        # We check that the unique variable match is 'foo_bar' and a hashed one 'foo_bar__<hash>'
        hashed_matches = re.findall(r"foo_bar__([a-f0-9]+)_0", c)
        unique_hashed = list(set(hashed_matches))
        if len(unique_hashed) == 1:
            if "(declare-fun foo_bar_0" in c or "foo_bar_post" in c:
                print(f"  [PASS] Collision check: %foo-bar mapped to distinct hashed symbol: {unique_hashed[0]} and %foo_bar correctly mapped to unhashed foo_bar.")
            else:
                print("  [FAIL] Collision check: unhashed foo_bar variable was not found in proof.")
                failures += 1
        else:
            print(f"  [FAIL] Collision check: %foo-bar and %foo_bar did not sanitize correctly: {hashed_matches}")
            failures += 1
    else:
        print("  [FAIL] Missing line 107 proof file.")
        failures += 1

    # 2. Empty fallback audit for % (line 105)
    proof_105 = os.path.join(proofs_dir, "proof_push_pop_replace_line105.smt2")
    if os.path.exists(proof_105):
        with open(proof_105, "r") as f:
            c = f.read()
        if "fallback__" in c:
            print("  [PASS] Empty register input % correctly sanitized to a fallback hash.")
        else:
            print("  [FAIL] Empty register input did not map to fallback.")
            failures += 1
    else:
        print("  [FAIL] Missing line 105 proof file.")
        failures += 1

    # 3. Standard-register exclusion and prefix stripping check
    proof_111 = os.path.join(proofs_dir, "proof_push_pop_replace_line111.smt2")
    if os.path.exists(proof_111):
        with open(proof_111, "r") as f:
            c = f.read()
        # standard register rax or rbx should NOT be hashed
        if "rax__" in c or "rbx__" in c:
            print("  [FAIL] Standard registers %rax or %rbx were hashed incorrectly.")
            failures += 1
        else:
            print("  [PASS] Standard registers %rax and %rbx were correctly treated as rax and rbx.")
            
        rax_count = c.count("(declare-fun rax_0")
        rbx_count = c.count("(declare-fun rbx_0")
        if rax_count != 1 or rbx_count != 1:
            print(f"  [FAIL] Standard registers rax or rbx were not declared exactly once. counts: rax={rax_count}, rbx={rbx_count}")
            failures += 1
        else:
            print("  [PASS] Standard registers rax and rbx were declared exactly once in the prelude.")
    else:
        print("  [FAIL] Missing line 111 proof file.")
        failures += 1

    # 4. Long-name uniqueness and truncation audit (line 108)
    proof_108 = os.path.join(proofs_dir, "proof_push_pop_replace_line108.smt2")
    if os.path.exists(proof_108):
        with open(proof_108, "r") as f:
            c = f.read()
        a_matches = re.findall(r"(a+__[0-9a-f]{8})_0", c)
        b_matches = re.findall(r"(b+__[0-9a-f]{8})_0", c)
        if a_matches and b_matches:
            print(f"  [PASS] Long name audit: long names correctly sanitized and hashed: {a_matches[0]} vs {b_matches[0]}")
            # Check length is bounded to maximum 127 characters
            if len(a_matches[0]) <= 127 and len(b_matches[0]) <= 127:
                print(f"  [PASS] Long name audit: sanitized names remain within 127-character limits.")
            else:
                print(f"  [FAIL] Long name audit: length exceeded limits: {len(a_matches[0])}")
                failures += 1
        else:
            print("  [FAIL] Long name audit: could not extract sanitized hashed long registers.")
            failures += 1
    else:
        print("  [FAIL] Missing line 108 proof file.")
        failures += 1

    print("==================================================")
    if failures == 0:
        print("=== SMT VERIFICATION GAUNTLET: ALL TESTS PASSED ===")
        sys.exit(0)
    else:
        print(f"=== SMT VERIFICATION GAUNTLET: {failures} FAILURES FOUND ===")
        sys.exit(1)

if __name__ == "__main__":
    main()
