#!/usr/bin/env python3
"""
zcc_fuzz_harness.py — ZCC Differential Fuzzing Harness v1.0
Csmith-based randomized differential testing: ZCC vs GCC
With autonomous creduce minimization on divergence.

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
    status:        str          # "pass" | "diverge" | "crash_zcc" | "crash_gcc" | "timeout"
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
        src   = os.path.join(tmpdir, "test.c")
        gcc_b = os.path.join(tmpdir, "test_gcc")
        zcc_b = os.path.join(tmpdir, "test_zcc")

        # 1. Generate test program
        rc, _, err = run(
            [CSMITH_BIN, "--seed", str(seed)] + CSMITH_FLAGS,
            timeout=10,
            cwd=tmpdir,
        )
        # csmith writes to stdout; redirect via shell
        gen_rc, gen_out, gen_err = run(
            ["sh", "-c",
             f"{CSMITH_BIN} --seed {seed} " + " ".join(CSMITH_FLAGS) + f" > {src}"],
            timeout=10,
            cwd=tmpdir,
        )
        if gen_rc != 0 or not os.path.exists(src) or os.path.getsize(src) == 0:
            return FuzzResult(seed, "crash_gcc", None, None, None, None, None,
                              int((time.time()-t0)*1000))

        # 2. Compile with GCC
        gcc_cc = [GCC_BIN, "-w", f"-I{CSMITH_INCLUDE}", src, "-o", gcc_b]
        gcc_rc, _, gcc_cerr = run(gcc_cc, COMPILE_TIMEOUT, cwd=tmpdir)
        if gcc_rc != 0:
            # GCC can't compile it — skip (Csmith occasionally emits un-gcc-able code)
            return FuzzResult(seed, "crash_gcc", gcc_rc, None, None, None, None,
                              int((time.time()-t0)*1000))

        # 3. Preprocess with GCC, then compile with ZCC
        pp_src = os.path.join(tmpdir, "test_pp.c")
        pp_rc, _, pp_err = run(
            [GCC_BIN, "-w", f"-I{CSMITH_INCLUDE}", "-E", src, "-o", pp_src],
            COMPILE_TIMEOUT, cwd=tmpdir,
        )
        if pp_rc != 0:
            return FuzzResult(seed, "crash_gcc", pp_rc, None, None, None, None,
                              int((time.time()-t0)*1000))
        zcc_cc = [ZCC_BIN, pp_src, "-o", zcc_b]
        zcc_rc, _, zcc_cerr = run(zcc_cc, COMPILE_TIMEOUT, cwd=tmpdir)

        if zcc_rc != 0:
            # ZCC compile failure — real crash divergence
            elapsed = int((time.time()-t0)*1000)
            result = FuzzResult(seed, "crash_zcc", gcc_rc, zcc_rc,
                                None, zcc_cerr[:512], None, elapsed)
            _save_and_reduce(seed, src, result)
            return result

        # 4. Execute both
        gcc_xrc, gcc_out, _ = run([gcc_b], EXEC_TIMEOUT, cwd=tmpdir)
        zcc_xrc, zcc_out, _ = run([zcc_b], EXEC_TIMEOUT, cwd=tmpdir)

        elapsed = int((time.time()-t0)*1000)

        # Timeout handling
        if gcc_xrc == -999:
            return FuzzResult(seed, "timeout", gcc_xrc, zcc_xrc,
                              None, None, None, elapsed)

        # Normalize output (strip trailing whitespace)
        gcc_norm = gcc_out.strip()
        zcc_norm = zcc_out.strip()

        # Skip if gcc itself crashed — UB in generated program, not ZCC bug
        if gcc_xrc not in (0,) and zcc_xrc not in (0,):
            return FuzzResult(seed, "crash_gcc", gcc_xrc, zcc_xrc,
                              None, None, None, elapsed)

        if gcc_norm == zcc_norm and gcc_xrc == zcc_xrc:
            return FuzzResult(seed, "pass", gcc_xrc, zcc_xrc,
                              None, None, None, elapsed)

        # Divergence found
        result = FuzzResult(
            seed, "diverge",
            gcc_xrc, zcc_xrc,
            gcc_norm[:256], zcc_norm[:256],
            None, elapsed
        )
        _save_and_reduce(seed, src, result)
        return result

def _save_and_reduce(seed: int, src_path: str, result: FuzzResult):
    """Copy source to oracle_batches and spawn creduce."""
    ORACLE_DIR.mkdir(parents=True, exist_ok=True)
    dest = ORACLE_DIR / f"divergence_{seed}.c"
    import shutil
    shutil.copy2(src_path, dest)
    result.reduced_path = str(dest)

    log(f"  ⚠ DIVERGENCE seed={seed} — saved to {dest}")
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
    if status == "crash_zcc":
        # ZCC fails to compile — interest = ZCC returns nonzero
        script = f"""#!/bin/bash
set -e
FILE="$1"
{ZCC_BIN} "$FILE" -o /tmp/zcc_interest_{seed} 2>/dev/null
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
{ZCC_BIN} "$FILE" -o /tmp/zcc_interest_{seed} 2>/dev/null || exit 0
GCC_OUT=$(/tmp/gcc_interest_{seed} 2>/dev/null || true)
ZCC_OUT=$(/tmp/zcc_interest_{seed} 2>/dev/null || true)
[ "$GCC_OUT" != "$ZCC_OUT" ] && exit 0
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
            [CREDUCE_BIN, str(interest), work],
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

    counts = {"pass": 0, "diverge": 0, "crash_zcc": 0,
              "crash_gcc": 0, "timeout": 0}

    log(f"🔱 ZCC Fuzz Harness v1.0 — {total} seeds, {args.workers} workers")
    log(f"   ZCC: {ZCC_BIN}")
    log(f"   GCC: {GCC_BIN}")
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

            if done % 50 == 0 or result.status in ("diverge", "crash_zcc"):
                elapsed = time.time() - t_start
                rate = done / elapsed
                log(f"  [{done}/{total}] {rate:.1f} seeds/s | "
                    f"pass={counts['pass']} diverge={counts['diverge']} "
                    f"crash_zcc={counts['crash_zcc']} timeout={counts['timeout']}")

    elapsed = time.time() - t_start
    log(f"\n🔱 Fuzzing complete — {total} seeds in {elapsed:.1f}s")
    log(f"   PASS:       {counts['pass']}")
    log(f"   DIVERGE:    {counts['diverge']}")
    log(f"   CRASH_ZCC:  {counts['crash_zcc']}")
    log(f"   CRASH_GCC:  {counts['crash_gcc']}  (skipped — gcc couldn't compile)")
    log(f"   TIMEOUT:    {counts['timeout']}")
    log(f"   Log:        {DIVERGENCE_LOG}")

    if counts["diverge"] + counts["crash_zcc"] > 0:
        log(f"\n⚠ {counts['diverge'] + counts['crash_zcc']} divergences found.")
        log(f"  Review: {ORACLE_DIR}")
        sys.exit(1)
    else:
        log("\n✅ No divergences found.")
        sys.exit(0)

if __name__ == "__main__":
    main()