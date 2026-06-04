#!/usr/bin/env python3
"""
zcc_fuzz_harness.py — ZCC Differential Fuzzing Harness v1.1
Csmith-based randomized differential testing: ZCC vs GCC vs Clang
With autonomous creduce minimization on divergence and preprocessor isolation.

Usage:
  python3 zcc_fuzz_harness.py [--seeds N] [--workers W] [--start-seed S]
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Optional

# ── Configuration ────────────────────────────────────────────────────────────
ZCC_BIN         = "/mnt/g/zccMAIN/zcc/zcc"
GCC_BIN         = "gcc"
CLANG_BIN       = "clang"
CSMITH_BIN      = "csmith"
CREDUCE_BIN     = "creduce"
CSMITH_INCLUDE  = "/usr/include/csmith"
ORACLE_DIR      = Path("/mnt/g/zccMAIN/zcc/oracle_batches")
DIVERGENCE_LOG  = ORACLE_DIR / "fuzz_divergences.jsonl"
COMPILE_TIMEOUT = 30    # seconds
EXEC_TIMEOUT    = 10    # seconds
CREDUCE_TIMEOUT = 300   # seconds per reduction

# Csmith flags: target ZCC's supported C subset, minimize false-positive noise
CSMITH_FLAGS = [
    "--no-bitfields",
    "--no-comma-operators",
    "--no-volatiles",
    "--no-volatile-pointers",
    "--no-const-pointers",
    "--no-consts",
    "--no-packed-struct",
    "--no-inline-function",
    "--no-longlong",
    "--no-safe-math",
    "--max-funcs", "3",
    "--max-block-depth", "3",
]

# ── Data structures ───────────────────────────────────────────────────────────
@dataclass
class FuzzResult:
    seed:          int
    status:        str          # "pass" | "mismatch_stdout" | "crash_zcc_pp" | "crash_zcc_cg" | "crash_gcc" | "timeout"
    gcc_exit:      Optional[int]
    zcc_exit:      Optional[int]
    gcc_stdout:    Optional[str]
    zcc_stdout:    Optional[str]
    reduced_path:  Optional[str]
    elapsed_ms:    int

# ── Helpers ───────────────────────────────────────────────────────────────────
_log_lock = threading.Lock()

def log(msg: str):
    ts = time.strftime("%H:%M:%S")
    with _log_lock:
        print(f"[{ts}] {msg}", flush=True)

def run(cmd, timeout, input_data=None, cwd=None):
    """Run a command, return (returncode, stdout, stderr). Never raises."""
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd,
        )
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return -999, "", "TIMEOUT"
    except Exception as e:
        return -998, "", str(e)

def append_divergence(result: FuzzResult):
    """Thread-safe append to JSONL log."""
    line = json.dumps(asdict(result)) + "\n"
    with _log_lock:
        with open(DIVERGENCE_LOG, "a") as f:
            f.write(line)

# ── Core fuzzing logic ────────────────────────────────────────────────────────
def fuzz_one(seed: int) -> FuzzResult:
    t0 = time.time()

    with tempfile.TemporaryDirectory(prefix=f"zcc_fuzz_{seed}_") as tmpdir:
        src        = os.path.join(tmpdir, "test.c")
        gcc_b      = os.path.join(tmpdir, "test_gcc")
        gcc_o3_b   = os.path.join(tmpdir, "test_gcc_o3")
        clang_b    = os.path.join(tmpdir, "test_clang")
        zcc_pp_b   = os.path.join(tmpdir, "test_zcc_pp")
        zcc_dir_b  = os.path.join(tmpdir, "test_zcc_dir")

        # 1. Generate test program
        gen_rc, gen_out, gen_err = run(
            ["sh", "-c",
             f"{CSMITH_BIN} --seed {seed} " + " ".join(CSMITH_FLAGS) + f" > {src}"],
            timeout=10,
            cwd=tmpdir,
        )
        if gen_rc != 0 or not os.path.exists(src) or os.path.getsize(src) == 0:
            return FuzzResult(seed, "crash_gcc", None, None, None, None, None,
                              int((time.time()-t0)*1000))

        # 2. Compile reference targets (GCC -O0, GCC -O3, Clang -O0)
        # GCC -O0
        gcc_rc, _, gcc_cerr = run([GCC_BIN, "-w", f"-I{CSMITH_INCLUDE}", src, "-o", gcc_b], COMPILE_TIMEOUT, cwd=tmpdir)
        if gcc_rc != 0:
            return FuzzResult(seed, "crash_gcc", gcc_rc, None, None, None, None,
                              int((time.time()-t0)*1000))

        # GCC -O3
        gcc_o3_rc, _, _ = run([GCC_BIN, "-w", "-O3", f"-I{CSMITH_INCLUDE}", src, "-o", gcc_o3_b], COMPILE_TIMEOUT, cwd=tmpdir)
        if gcc_o3_rc != 0:
            return FuzzResult(seed, "crash_gcc", gcc_o3_rc, None, None, None, None,
                              int((time.time()-t0)*1000))

        # Clang -O0
        clang_rc, _, _ = run([CLANG_BIN, "-w", f"-I{CSMITH_INCLUDE}", src, "-o", clang_b], COMPILE_TIMEOUT, cwd=tmpdir)
        if clang_rc != 0:
            return FuzzResult(seed, "crash_gcc", clang_rc, None, None, None, None,
                              int((time.time()-t0)*1000))

        # Run reference programs to get golden output
        gcc_xrc, gcc_out, _ = run([gcc_b], EXEC_TIMEOUT, cwd=tmpdir)
        gcc_o3_xrc, gcc_o3_out, _ = run([gcc_o3_b], EXEC_TIMEOUT, cwd=tmpdir)
        clang_xrc, clang_out, _ = run([clang_b], EXEC_TIMEOUT, cwd=tmpdir)

        # Skip if reference programs crashed or diverged (indicates UB in Csmith generated program)
        if gcc_xrc != 0 or gcc_o3_xrc != 0 or clang_xrc != 0:
            return FuzzResult(seed, "crash_gcc", gcc_xrc, None, None, None, None,
                              int((time.time()-t0)*1000))

        gcc_norm = gcc_out.strip()
        gcc_o3_norm = gcc_o3_out.strip()
        clang_norm = clang_out.strip()

        if gcc_norm != gcc_o3_norm or gcc_norm != clang_norm:
            # Reference compiler output mismatch — skip to avoid UB bugs
            return FuzzResult(seed, "crash_gcc", gcc_xrc, None, None, None, None,
                              int((time.time()-t0)*1000))

        # 3. Path A: Preprocessed compilation with ZCC
        pp_src = os.path.join(tmpdir, "test_pp.c")
        pp_rc, _, pp_err = run([GCC_BIN, "-w", f"-I{CSMITH_INCLUDE}", "-E", src, "-o", pp_src], COMPILE_TIMEOUT, cwd=tmpdir)
        if pp_rc != 0:
            return FuzzResult(seed, "crash_gcc", pp_rc, None, None, None, None,
                              int((time.time()-t0)*1000))

        zcc_pp_s = os.path.join(tmpdir, "test_zcc_pp.s")
        zcc_pp_rc, _, zcc_pp_cerr = run([ZCC_BIN, pp_src, "-o", zcc_pp_s], COMPILE_TIMEOUT, cwd=tmpdir)
        if zcc_pp_rc == 0:
            # Link with GCC
            link_rc, _, zcc_pp_cerr = run([GCC_BIN, "-w", zcc_pp_s, "-o", zcc_pp_b, "-lm"], COMPILE_TIMEOUT, cwd=tmpdir)
            if link_rc != 0:
                zcc_pp_rc = link_rc

        if zcc_pp_rc != 0:
            # ZCC codegen compile crash
            elapsed = int((time.time()-t0)*1000)
            result = FuzzResult(seed, "crash_zcc_cg", gcc_rc, zcc_pp_rc,
                                None, zcc_pp_cerr[:512], None, elapsed)
            _save_and_reduce(seed, src, result)
            return result

        # Run preprocessed ZCC executable
        zcc_pp_xrc, zcc_pp_out, _ = run([zcc_pp_b], EXEC_TIMEOUT, cwd=tmpdir)
        zcc_pp_norm = zcc_pp_out.strip()

        if zcc_pp_xrc != 0 or zcc_pp_norm != gcc_norm:
            elapsed = int((time.time()-t0)*1000)
            result = FuzzResult(seed, "mismatch_stdout", gcc_xrc, zcc_pp_xrc,
                                gcc_norm[:256], zcc_pp_norm[:256], None, elapsed)
            _save_and_reduce(seed, src, result)
            return result

        # 4. Path B: Direct compilation (Tests ZCC Preprocessor)
        zcc_dir_s = os.path.join(tmpdir, "test_zcc_dir.s")
        zcc_dir_rc, _, zcc_dir_cerr = run([ZCC_BIN, f"-I{CSMITH_INCLUDE}", src, "-o", zcc_dir_s], COMPILE_TIMEOUT, cwd=tmpdir)
        if zcc_dir_rc == 0:
            # Link with GCC
            link_rc, _, zcc_dir_cerr = run([GCC_BIN, "-w", zcc_dir_s, "-o", zcc_dir_b, "-lm"], COMPILE_TIMEOUT, cwd=tmpdir)
            if link_rc != 0:
                zcc_dir_rc = link_rc

        if zcc_dir_rc != 0:
            # Direct failed, but preprocessed succeeded -> Preprocessor bug!
            elapsed = int((time.time()-t0)*1000)
            result = FuzzResult(seed, "crash_zcc_pp", gcc_rc, zcc_dir_rc,
                                None, zcc_dir_cerr[:512], None, elapsed)
            _save_and_reduce(seed, src, result)
            return result

        zcc_dir_xrc, zcc_dir_out, _ = run([zcc_dir_b], EXEC_TIMEOUT, cwd=tmpdir)
        zcc_dir_norm = zcc_dir_out.strip()

        if zcc_dir_xrc != 0 or zcc_dir_norm != gcc_norm:
            # Preprocessor produced incorrect expansion causing logical mismatch
            elapsed = int((time.time()-t0)*1000)
            result = FuzzResult(seed, "mismatch_stdout", gcc_xrc, zcc_dir_xrc,
                                gcc_norm[:256], zcc_dir_norm[:256], None, elapsed)
            _save_and_reduce(seed, src, result)
            return result

        # All checks passed
        return FuzzResult(seed, "pass", gcc_xrc, zcc_pp_xrc, None, None, None, int((time.time()-t0)*1000))

def _save_and_reduce(seed: int, src_path: str, result: FuzzResult):
    """Copy source to oracle_batches and spawn creduce."""
    ORACLE_DIR.mkdir(parents=True, exist_ok=True)
    dest = ORACLE_DIR / f"divergence_{seed}.c"
    import shutil
    shutil.copy2(src_path, dest)
    result.reduced_path = str(dest)

    log(f"  ⚠ DIVERGENCE seed={seed} — saved to {dest} [status={result.status}]")
    log(f"    gcc exit={result.gcc_exit} stdout={repr(result.gcc_stdout)}")
    log(f"    zcc exit={result.zcc_exit} stdout={repr(result.zcc_stdout)}")

    # Write interest script for creduce
    interest = ORACLE_DIR / f"interest_{seed}.sh"
    _write_interest_script(interest, seed, result.status)

    # Launch creduce (non-blocking, capped timeout)
    _run_creduce(dest, interest, seed)

def _write_interest_script(path: Path, seed: int, status: str):
    """
    creduce interest test: returns 0 if the divergence still reproduces.
    Discriminates crash vs output-mismatch to avoid false-positive reductions.
    """
    if status == "crash_zcc_pp":
        # Preprocessor crash — ZCC fails to compile directly but GCC compiles it
        script = f"""#!/bin/bash
set -e
FILE="$1"
{GCC_BIN} -w -I{CSMITH_INCLUDE} "$FILE" -o /tmp/gcc_interest_{seed} 2>/dev/null || exit 1
{ZCC_BIN} -I{CSMITH_INCLUDE} "$FILE" -o /tmp/zcc_interest_{seed}.s 2>/dev/null
RET=$?
[ $RET -ne 0 ] && exit 0
exit 1
"""
    elif status == "crash_zcc_cg":
        # Codegen crash — ZCC fails to compile preprocessed code
        script = f"""#!/bin/bash
set -e
FILE="$1"
{GCC_BIN} -w -I{CSMITH_INCLUDE} -E "$FILE" -o /tmp/pp_interest_{seed}.c 2>/dev/null || exit 1
{ZCC_BIN} /tmp/pp_interest_{seed}.c -o /tmp/zcc_interest_{seed}.s 2>/dev/null
RET=$?
[ $RET -ne 0 ] && exit 0
exit 1
"""
    else:
        # Output mismatch — interest = outputs differ
        script = f"""#!/bin/bash
set -e
FILE="$1"
{GCC_BIN} -w -I{CSMITH_INCLUDE} "$FILE" -o /tmp/gcc_interest_{seed} 2>/dev/null || exit 1
/tmp/gcc_interest_{seed} > /tmp/gcc_out_{seed} 2>/dev/null || exit 1
{ZCC_BIN} -I{CSMITH_INCLUDE} "$FILE" -o /tmp/zcc_interest_{seed}.s 2>/dev/null
RET=$?
if [ $RET -eq 0 ]; then
    {GCC_BIN} -w /tmp/zcc_interest_{seed}.s -o /tmp/zcc_interest_{seed} -lm 2>/dev/null || exit 1
else
    # If direct fails, try preprocessed ZCC
    {GCC_BIN} -w -I{CSMITH_INCLUDE} -E "$FILE" -o /tmp/pp_interest_{seed}.c 2>/dev/null || exit 1
    {ZCC_BIN} /tmp/pp_interest_{seed}.c -o /tmp/zcc_interest_{seed}.s 2>/dev/null || exit 1
    {GCC_BIN} -w /tmp/zcc_interest_{seed}.s -o /tmp/zcc_interest_{seed} -lm 2>/dev/null || exit 1
fi
/tmp/zcc_interest_{seed} > /tmp/zcc_out_{seed} 2>/dev/null || exit 1
if ! diff -q /tmp/gcc_out_{seed} /tmp/zcc_out_{seed} >/dev/null; then
    exit 0
fi
exit 1
"""
    path.write_text(script)
    path.chmod(0o755)

def _run_creduce(src: Path, interest: Path, seed: int):
    """Run creduce on a divergence file, save reduced result."""
    import shutil, tempfile
    reduced = ORACLE_DIR / f"divergence_{seed}_reduced.c"
    # creduce works in-place on a copy
    with tempfile.TemporaryDirectory(prefix=f"creduce_{seed}_") as tmpdir:
        work = os.path.join(tmpdir, "test.c")
        shutil.copy2(src, work)
        rc, out, err = run(
            ["nice", "-n", "19", CREDUCE_BIN, "--n", "1", str(interest), work],
            timeout=CREDUCE_TIMEOUT,
            cwd=tmpdir,
        )
        if rc == 0 and os.path.exists(work):
            shutil.copy2(work, reduced)
            log(f"  ✅ creduce seed={seed} → {reduced} "
                f"({os.path.getsize(reduced)} bytes)")
        else:
            log(f"  ⚠ creduce seed={seed} failed/timeout (rc={rc})")

# ── Main harness ──────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="ZCC Csmith Differential Fuzzer")
    parser.add_argument("--seeds",      type=int, default=500,
                        help="Number of seeds to test (default: 500)")
    parser.add_argument("--workers",    type=int, default=4,
                        help="Parallel workers (default: 4)")
    parser.add_argument("--start-seed", type=int, default=1,
                        help="Starting seed (default: 1)")
    args = parser.parse_args()

    ORACLE_DIR.mkdir(parents=True, exist_ok=True)

    seeds = range(args.start_seed, args.start_seed + args.seeds)
    total = len(seeds)

    counts = {"pass": 0, "mismatch_stdout": 0, "crash_zcc_pp": 0,
              "crash_zcc_cg": 0, "crash_gcc": 0, "timeout": 0}

    log(f"🔱 ZCC Fuzz Harness v1.1 — {total} seeds, {args.workers} workers")
    log(f"   ZCC: {ZCC_BIN}")
    log(f"   GCC: {GCC_BIN}")
    log(f"   Clang: {CLANG_BIN}")
    log(f"   Divergences → {ORACLE_DIR}")

    t_start = time.time()

    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(fuzz_one, s): s for s in seeds}
        done = 0
        for fut in as_completed(futures):
            done += 1
            result = fut.result()
            counts[result.status] = counts.get(result.status, 0) + 1

            if result.status != "pass":
                append_divergence(result)

            if done % 50 == 0 or result.status in ("mismatch_stdout", "crash_zcc_pp", "crash_zcc_cg"):
                elapsed = time.time() - t_start
                rate = done / elapsed
                log(f"  [{done}/{total}] {rate:.1f} seeds/s | "
                    f"pass={counts['pass']} mismatch={counts['mismatch_stdout']} "
                    f"crash_pp={counts['crash_zcc_pp']} crash_cg={counts['crash_zcc_cg']} "
                    f"timeout={counts['timeout']}")

    elapsed = time.time() - t_start
    log(f"\n🔱 Fuzzing complete — {total} seeds in {elapsed:.1f}s")
    log(f"   PASS:       {counts['pass']}")
    log(f"   MISMATCH:   {counts['mismatch_stdout']}")
    log(f"   CRASH_PP:   {counts['crash_zcc_pp']}")
    log(f"   CRASH_CG:   {counts['crash_zcc_cg']}")
    log(f"   CRASH_GCC:  {counts['crash_gcc']}  (skipped — gcc/clang failed/crashed/diverged)")
    log(f"   TIMEOUT:    {counts['timeout']}")
    log(f"   Log:        {DIVERGENCE_LOG}")

    total_failures = counts["mismatch_stdout"] + counts["crash_zcc_pp"] + counts["crash_zcc_cg"]
    if total_failures > 0:
        log(f"\n⚠ {total_failures} divergences found.")
        log(f"  Review: {ORACLE_DIR}")
        sys.exit(1)
    else:
        log("\n✅ No divergences found.")
        sys.exit(0)

if __name__ == "__main__":
    main()