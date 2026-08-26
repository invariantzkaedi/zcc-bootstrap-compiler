#!/usr/bin/env python3
"""
Fast Parallel Corpus Test Runner for ZCC
Executes all tests in tests/test_*.c and test_*.c with 8 parallel workers and 2s timeout.
"""

import os
import glob
import subprocess
import concurrent.futures
import time

REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ZCC_BIN = os.path.join(REPO_DIR, "zcc")

def run_single_test(test_path):
    name = os.path.basename(test_path)
    out_s = f"/tmp/_zcc_corp_fast_{name}.s"
    cmd = [
        ZCC_BIN,
        f"-I{REPO_DIR}/src",
        f"-I{REPO_DIR}/include",
        f"-I{REPO_DIR}",
        test_path,
        "-o",
        out_s
    ]
    is_negative_test = "guard" in name or "fail" in name
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8.0)
        success = False
        if is_negative_test:
            # Negative test succeeds if compilation fails with non-zero exit code or error output
            if res.returncode != 0 or "#error" in res.stderr or "error" in res.stderr.lower():
                success = True
        else:
            if "ZCC Engine Compilation Terminated Successfully" in res.stderr or "ZCC Engine Compilation Terminated Successfully" in res.stdout:
                if os.path.exists(out_s) and os.path.getsize(out_s) > 0:
                    with open(out_s, "r", errors="ignore") as f:
                        if ".section" in f.read():
                            success = True
        if os.path.exists(out_s):
            try:
                os.remove(out_s)
            except OSError:
                pass
        return name, success, res.stderr if not success else ""
    except subprocess.TimeoutExpired:
        if os.path.exists(out_s):
            try:
                os.remove(out_s)
            except OSError:
                pass
        return name, False, "TIMEOUT (2.0s)"
    except Exception as e:
        return name, False, str(e)

def main():
    files = sorted(list(set(
        glob.glob(os.path.join(REPO_DIR, "tests", "test_*.c")) +
        glob.glob(os.path.join(REPO_DIR, "test_*.c"))
    )))
    
    total = len(files)
    print(f"[*] Discovered {total} corpus test fixtures across repository.")
    
    t0 = time.time()
    passed = []
    failed = []
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
        futures = {executor.submit(run_single_test, f): f for f in files}
        for fut in concurrent.futures.as_completed(futures):
            name, success, err = fut.result()
            if success:
                passed.append(name)
            else:
                failed.append((name, err))
                
    elapsed = time.time() - t0
    pct = (len(passed) / total) * 100.0 if total > 0 else 0.0
    
    print("\n" + "=" * 80)
    print(f" 🔱 ZCC C CORPUS FAST MULTI-WORKER SUITE")
    print("=" * 80)
    print(f" Total Tests   : {total}")
    print(f" Passed Tests  : {len(passed)}")
    print(f" Failed Tests  : {len(failed)}")
    print(f" Pass Rate     : {pct:.2f}%")
    print(f" Total Runtime : {elapsed:.2f}s")
    print("=" * 80)
    
    if failed:
        print("\nFailing Test Cases (First 15):")
        for idx, (fname, err) in enumerate(failed[:15], 1):
            err_line = err.strip().splitlines()[-1] if err.strip() else "Unknown Error"
            print(f"  [{idx:02d}] {fname} -> {err_line[:70]}")

if __name__ == "__main__":
    main()
