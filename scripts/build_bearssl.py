#!/usr/bin/env python3
"""
ZCC BearSSL Conquest Build & Test Gauntlet
Compiles all BearSSL cryptographic library sources with ZCC,
links test_crypto, executes Known Answer Tests (KAT),
and verifies cryptographic correctness across hashes, symmetric ciphers,
PRNG, MAC/AEAD, RSA, and Elliptic Curves.
"""

import os
import sys
import subprocess
import time
from multiprocessing import Pool, cpu_count

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ZCC = os.path.join(REPO_ROOT, "zcc")
BEARSSL_DIR = "/tmp/bearssl"
OBJ_DIR = "/tmp/bearssl_objs"

os.makedirs(OBJ_DIR, exist_ok=True)

def parse_rules():
    rules_mk = os.path.join(BEARSSL_DIR, "mk", "Rules.mk")
    with open(rules_mk, "r") as f:
        content = f.read()

    import re
    matches = re.findall(r'\$\(OBJDIR\)\$P([a-zA-Z0-9_]+)\$O:\s+([^\s]+)', content)
    obj_to_src = {}
    for obj, src in matches:
        clean_src = src.replace('$P', '/')
        full_src = os.path.join(BEARSSL_DIR, clean_src)
        if os.path.exists(full_src):
            obj_to_src[obj] = full_src

    obj_match = re.search(r'OBJ\s*=\s*(.*?)(?:\n\n|\n[A-Z0-9_]+\s*=)', content, re.DOTALL)
    lib_objs = []
    if obj_match:
        raw_objs = re.findall(r'\$\(OBJDIR\)\$P([a-zA-Z0-9_]+)\$O', obj_match.group(1))
        lib_objs = raw_objs
        if '$(OBJSETTINGS)' in obj_match.group(1):
            lib_objs.insert(0, 'settings')

    return obj_to_src, lib_objs

def compile_worker(args):
    src_path, obj_name = args
    s_path = os.path.join(OBJ_DIR, f"{obj_name}.s")
    o_path = os.path.join(OBJ_DIR, f"{obj_name}.o")

    # If object exists and is newer than zcc and src, reuse
    if os.path.exists(o_path):
        o_mtime = os.path.getmtime(o_path)
        if o_mtime > os.path.getmtime(ZCC) and o_mtime > os.path.getmtime(src_path):
            return True, obj_name, o_path, "cached"

    cmd_zcc = [
        ZCC,
        "-q",
        f"-I{BEARSSL_DIR}/inc",
        f"-I{BEARSSL_DIR}/src",
        src_path,
        "-o",
        s_path
    ]
    res_zcc = subprocess.run(cmd_zcc, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_zcc.returncode != 0:
        return False, obj_name, f"ZCC error:\n{res_zcc.stderr}\n{res_zcc.stdout}", "error"

    cmd_as = ["as", "-o", o_path, s_path]
    res_as = subprocess.run(cmd_as, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_as.returncode != 0:
        return False, obj_name, f"AS error:\n{res_as.stderr}\n{res_as.stdout}", "error"

    return True, obj_name, o_path, "compiled"

def main():
    print("=" * 70)
    print("  ZCC BEARSSL CRYPTOGRAPHIC CONQUEST PIPELINE (PARALLEL)")
    print(f"  Target:  {ZCC}")
    print(f"  Source:  {BEARSSL_DIR}")
    print(f"  Output:  {OBJ_DIR}")
    print(f"  Workers: {min(16, cpu_count())}")
    print("=" * 70)

    obj_to_src, lib_objs = parse_rules()
    print(f"[*] Found {len(obj_to_src)} total object rules, {len(lib_objs)} objects in libbearssl.a")

    tasks = []
    skipped = []

    for obj in lib_objs:
        src = obj_to_src.get(obj)
        if not src or not os.path.exists(src):
            skipped.append(obj)
            continue
        tasks.append((src, obj))

    t0 = time.time()
    built_objs = []
    failed = []

    num_workers = min(16, cpu_count())
    with Pool(num_workers) as pool:
        for ok, obj_name, res, status in pool.imap_unordered(compile_worker, tasks):
            if ok:
                built_objs.append(res)
                if status == "compiled":
                    print(f"  [+] [ZCC] Compiled {obj_name}.o ({os.path.getsize(res)} bytes)")
            else:
                failed.append((obj_name, res))
                print(f"  [-] FAILED: {obj_name}")
                print(res[:400])

    t1 = time.time()
    print("-" * 70)
    print(f"[*] Compilation phase finished in {t1 - t0:.2f}s")
    print(f"    Total Objects: {len(built_objs)} / {len(tasks)}")
    print(f"    Skipped:       {len(skipped)}")
    print(f"    Failed:        {len(failed)}")

    if failed:
        print("[-] Build aborted due to compilation failures.")
        return 1

    # Archive libbearssl.a
    lib_path = os.path.join(OBJ_DIR, "libbearssl.a")
    subprocess.run(["rm", "-f", lib_path])
    cmd_ar = ["ar", "rcs", lib_path] + built_objs
    res_ar = subprocess.run(cmd_ar, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_ar.returncode != 0:
        print(f"[-] ar error:\n{res_ar.stderr}")
        return 1
    print(f"[+] Successfully generated {lib_path} ({os.path.getsize(lib_path):,} bytes)")

    # Compile test_crypto.c
    print("=" * 70)
    print("  COMPILING TESTCRYPTO HARNESS WITH ZCC")
    print("=" * 70)
    test_src = os.path.join(BEARSSL_DIR, "test", "test_crypto.c")
    test_s = os.path.join(OBJ_DIR, "test_crypto.s")
    test_o = os.path.join(OBJ_DIR, "test_crypto.o")

    t_start = time.time()
    cmd_test_zcc = [
        ZCC,
        "-q",
        f"-I{BEARSSL_DIR}/inc",
        f"-I{BEARSSL_DIR}/src",
        test_src,
        "-o",
        test_s
    ]
    res_tz = subprocess.run(cmd_test_zcc, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_tz.returncode != 0:
        print(f"[-] ZCC failed to compile test_crypto.c:\n{res_tz.stderr}\n{res_tz.stdout}")
        return 1

    subprocess.run(["as", "-o", test_o, test_s], check=True)
    print(f"[+] test_crypto.o generated in {time.time() - t_start:.2f}s ({os.path.getsize(test_o):,} bytes)")

    # Link testcrypto executable
    bin_path = os.path.join(OBJ_DIR, "testcrypto")
    cmd_link = ["gcc", "-o", bin_path, test_o, lib_path, "-lm"]
    res_link = subprocess.run(cmd_link, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res_link.returncode != 0:
        print(f"[-] Link error:\n{res_link.stderr}")
        return 1
    print(f"[+] Linked {bin_path} ({os.path.getsize(bin_path):,} bytes)")

    # Run Known Answer Tests (KAT)
    print("=" * 70)
    print("  RUNNING BEARSSL CRYPTOGRAPHIC GAUNTLET")
    print("=" * 70)
    cmd_run = [bin_path, "all"]
    t_kat = time.time()
    proc = subprocess.Popen(cmd_run, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    out, _ = proc.communicate()
    t_kat_end = time.time()

    print(out)
    print(f"[*] Gauntlet finished in {t_kat_end - t_kat:.2f}s with exit code {proc.returncode}")
    if proc.returncode == 0 and "ERR" not in out:
        print("[+] ALL BEARSSL TESTS PASSED CLEANLY!")
        return 0
    else:
        print("[-] BEARSSL TESTS ENCOUNTERED DIVERGENCES OR ERRORS")
        return 1

if __name__ == "__main__":
    sys.exit(main())
