import subprocess
import os
import sys

def run_cmd(cmd):
    print(f"Running: {cmd}")
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return res

def test_division_regressions():
    # 1. Compile test_divzero_backend_ops.c natively
    res = run_cmd("./zcc tests/test_divzero_backend_ops.c -S -o tests/test_divzero_backend_ops.s")
    if res.returncode != 0:
        print("FAIL: Native compilation failed")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        return False
    
    # Check that warnings are present
    if "division by zero proven at compile time" not in res.stderr:
        print("FAIL: Division by zero warning missing")
        print("STDERR:", res.stderr)
        return False
    if "modulo by zero proven at compile time" not in res.stderr:
        print("FAIL: Modulo by zero warning missing")
        print("STDERR:", res.stderr)
        return False

    print("PASS: Division-by-zero warnings present in native mode.")

    # 2. Compile test_divzero_backend_ops.c in IR mode
    # Let's clean up any existing file
    if os.path.exists("tests/test_divzero_backend_ops_ir.s"):
        os.remove("tests/test_divzero_backend_ops_ir.s")
    
    res = run_cmd("./zcc tests/test_divzero_backend_ops.c --emit-ir-graph tests/test_divzero_backend_ops.ir.json -S -o tests/test_divzero_backend_ops_ir.s")
    if res.returncode != 0:
        print("FAIL: IR mode compilation failed")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        return False

    # Check warnings in IR mode
    if "division by zero proven at compile time" not in res.stderr:
        print("FAIL: Division by zero warning missing in IR mode")
        print("STDERR:", res.stderr)
        return False

    print("PASS: Division-by-zero warnings present in IR mode.")

    # 3. Verify clean execution of division by zero tests
    # Compile and run test_div_zero.c
    res = run_cmd("./zcc tests/test_div_zero.c -S -o tests/test_div_zero.s")
    if res.returncode != 0:
        print("FAIL: compiling test_div_zero.c with zcc failed")
        print("STDERR:", res.stderr)
        return False
    
    res = run_cmd("gcc -no-pie -o tests/test_div_zero_bin tests/test_div_zero.s -lm")
    if res.returncode != 0:
        print("FAIL: linking test_div_zero.s with gcc failed")
        print("STDERR:", res.stderr)
        return False
    
    res_run = run_cmd("./tests/test_div_zero_bin")
    if "DIV_ZERO_TEST: PASS" not in res_run.stdout:
        print("FAIL: test_div_zero runtime check failed")
        print("STDOUT:", res_run.stdout)
        return False
    print("PASS: Runtime division-by-zero mitigation behaves correctly (returns 0 instead of SIGFPE).")

    # 4. Check test_icp_div_zero.c
    res = run_cmd("./zcc tests/test_icp_div_zero.c -S -o tests/test_icp_div_zero.s")
    if res.returncode != 0:
        print("FAIL: test_icp_div_zero.c compilation failed")
        return False
    if "division by zero proven at compile time" not in res.stderr:
        print("FAIL: ICP-proven division by zero warning missing")
        return False
    print("PASS: ICP-proven division-by-zero warnings are successfully detected.")

    print("\nALL REGRESSION TESTS PASSED SUCCESSFULLY!")
    return True

if __name__ == "__main__":
    success = test_division_regressions()
    sys.exit(0 if success else 1)
