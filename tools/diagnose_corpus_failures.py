#!/usr/bin/env python3
import os
import glob
import subprocess

REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ZCC_BIN = os.path.join(REPO_DIR, "zcc")
files = sorted(list(set(glob.glob(os.path.join(REPO_DIR, "tests", "test_*.c")) + glob.glob(os.path.join(REPO_DIR, "test_*.c")))))

fails = []
for f in files:
    name = os.path.basename(f)
    out_s = f"/tmp/_inspect_{name}.s"
    cmd = [ZCC_BIN, f"-I{REPO_DIR}/src", f"-I{REPO_DIR}/include", f"-I{REPO_DIR}", f, "-o", out_s]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=8.0)
        success = False
        if "ZCC Engine Compilation Terminated Successfully" in res.stderr or "ZCC Engine Compilation Terminated Successfully" in res.stdout:
            if os.path.exists(out_s) and os.path.getsize(out_s) > 0:
                with open(out_s, "r", errors="ignore") as sf:
                    if ".section" in sf.read():
                        success = True
        if os.path.exists(out_s):
            try: os.remove(out_s)
            except: pass
        if not success:
            combined = res.stderr + "\n" + res.stdout
            err_lines = [l.strip() for l in combined.splitlines() if any(k in l.lower() for k in ["error", "fatal", "failed", "assert", "undefined", "cannot", "unexpected"])]
            last_err = err_lines[-1] if err_lines else (res.stderr.strip().splitlines()[-1] if res.stderr.strip() else "Exit " + str(res.returncode))
            fails.append((name, last_err))
    except subprocess.TimeoutExpired:
        fails.append((name, "TIMEOUT (>8s)"))

print("=" * 80)
print(f" TOTAL CORPUS FAILURES: {len(fails)} / {len(files)}")
print("=" * 80)
for idx, (name, err) in enumerate(fails, 1):
    print(f"[{idx:02d}] {name:32s} -> {err[:65]}")
