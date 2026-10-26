#!/usr/bin/env python3
"""
tools/filter_core05_corpus.py

Implements §2 mechanical, ZCC-independent filtering for ZCC-CORE-05.
Evaluates candidates strictly against host GCC 13.3.0 and Clang 18.1.3:
  a. `gcc -E -P` succeeds.
  b. `gcc -O0 -fno-builtin -no-pie <TU>.i -lm` compiles and links.
  c. The gcc binary exits 0 within 10 s, with no argv/stdin required.
  d. Two consecutive gcc runs produce identical (stdout, stderr).
  e. Source contains none of: time( clock( rand( srand( getpid( argv getenv( fopen( /dev/urandom (case-sensitive grep).
  f. GCC and Clang binaries agree with each other on (stdout, stderr, exit).
     Disagreement -> fixture EXCLUDED as "reference-divergent / UB", logged, not counted either way.
  EXCLUDE by name: known harness-only / diagnostic tests (e.g. test_ir_bridge_guard.c).

Outputs:
  - tests/core05_fixtures.txt (frozen list of paths, relative to repo root)
  - tests/core05_fixtures.sha256
  - Detailed summary of inclusions and exclusions with breakdown reasons.
"""

import os
import sys
import glob
import subprocess
import hashlib
import tempfile
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

EXCLUDED_NAMES = {
    "test_ir_bridge_guard.c",  # Explicit compilation-failure guard test (#error expected)
}

BANNED_SUBSTRINGS = [
    "time(",
    "clock(",
    "rand(",
    "srand(",
    "getpid(",
    "argv",
    "getenv(",
    "fopen(",
    "/dev/urandom",
]

INCLUDES = ["-I", "tests", "-I", "include", "-I", "src", "-I", "."]


def run_cmd(cmd, timeout=10, input_data=None):
    try:
        res = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            input=input_data,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        return res.returncode, res.stdout, res.stderr, False
    except subprocess.TimeoutExpired as e:
        return -1, b"", (e.stderr or b"") + b"\nTIMEOUT", True
    except Exception as e:
        return -1, b"", str(e).encode("utf-8"), False


def main():
    import argparse
    parser = argparse.ArgumentParser(description="ZCC-CORE-05 Corpus Filter")
    parser.add_argument("--scope", choices=["tests_only", "all_526"], default="tests_only",
                        help="Whether to evaluate only tests/test_*.c or both tests/ and root test_*.c")
    parser.add_argument("--output-list", default="tests/core05_fixtures.txt",
                        help="Path to output frozen fixture list")
    args = parser.parse_args()

    if args.scope == "tests_only":
        patterns = [os.path.join(REPO_ROOT, "tests", "test_*.c")]
    else:
        patterns = [
            os.path.join(REPO_ROOT, "tests", "test_*.c"),
            os.path.join(REPO_ROOT, "test_*.c"),
        ]

    candidate_files = []
    for pat in patterns:
        candidate_files.extend(glob.glob(pat))
    candidate_files = sorted(list(set(candidate_files)))

    print(f"Total candidate files discovered: {len(candidate_files)} (scope={args.scope})")

    included = []
    excluded = {}

    with tempfile.TemporaryDirectory(prefix="core05_filter_") as tmpdir:
        for idx, src_path in enumerate(candidate_files, 1):
            rel_path = os.path.relpath(src_path, REPO_ROOT)
            basename = os.path.basename(src_path)

            # Name exclusion
            if basename in EXCLUDED_NAMES:
                excluded[rel_path] = "EXCLUDED_BY_NAME: known harness-only / negative test"
                continue

            # Read source for banned substrings (e)
            try:
                with open(src_path, "r", encoding="utf-8", errors="replace") as f:
                    src_content = f.read()
            except Exception as e:
                excluded[rel_path] = f"EXCLUDED_READ_ERROR: {e}"
                continue

            matched_banned = [b for b in BANNED_SUBSTRINGS if b in src_content]
            if matched_banned:
                excluded[rel_path] = f"EXCLUDED_BANNED_PATTERN: matched {matched_banned}"
                continue

            tu_i = os.path.join(tmpdir, f"tu_{idx}.i")
            bin_gcc = os.path.join(tmpdir, f"bin_gcc_{idx}")
            bin_clang = os.path.join(tmpdir, f"bin_clang_{idx}")

            # (a) gcc -E -P succeeds
            cmd_pp = ["gcc", "-E", "-P"] + INCLUDES + [rel_path, "-o", tu_i]
            rc, out, err, timed_out = run_cmd(cmd_pp, timeout=10)
            if rc != 0 or timed_out:
                excluded[rel_path] = f"EXCLUDED_PP_FAIL: gcc -E exit {rc}"
                continue

            # (b) gcc -O0 -fno-builtin -no-pie <TU>.i -lm compiles and links
            cmd_gcc_build = ["gcc", "-O0", "-fno-builtin", "-no-pie", tu_i, "-lm", "-o", bin_gcc]
            rc, out, err, timed_out = run_cmd(cmd_gcc_build, timeout=10)
            if rc != 0 or timed_out:
                excluded[rel_path] = f"EXCLUDED_GCC_BUILD_FAIL: gcc compile exit {rc}"
                continue

            # (c) The gcc binary exits 0 within 10 s, with no argv/stdin required
            rc_gcc1, out_gcc1, err_gcc1, timed_out1 = run_cmd([bin_gcc], timeout=10)
            if timed_out1:
                excluded[rel_path] = "EXCLUDED_GCC_TIMEOUT: exit timeout > 10s"
                continue
            if rc_gcc1 != 0:
                excluded[rel_path] = f"EXCLUDED_GCC_NONZERO_EXIT: exit {rc_gcc1}"
                continue

            # (d) Two consecutive gcc runs produce identical (stdout, stderr)
            rc_gcc2, out_gcc2, err_gcc2, timed_out2 = run_cmd([bin_gcc], timeout=10)
            if timed_out2:
                excluded[rel_path] = "EXCLUDED_GCC_RUN2_TIMEOUT"
                continue
            if (rc_gcc1, out_gcc1, err_gcc1) != (rc_gcc2, out_gcc2, err_gcc2):
                excluded[rel_path] = "EXCLUDED_GCC_NONDETERMINISTIC: run1 != run2"
                continue

            # (f) Clang compiles and agrees on (stdout, stderr, exit)
            cmd_clang_build = ["clang", "-O0", "-fno-builtin", "-no-pie", tu_i, "-lm", "-o", bin_clang]
            rc, out, err, timed_out = run_cmd(cmd_clang_build, timeout=10)
            if rc != 0 or timed_out:
                excluded[rel_path] = f"EXCLUDED_CLANG_BUILD_FAIL: clang compile exit {rc}"
                continue

            rc_clang, out_clang, err_clang, timed_out_clang = run_cmd([bin_clang], timeout=10)
            if timed_out_clang:
                excluded[rel_path] = "EXCLUDED_CLANG_TIMEOUT: clang exit timeout > 10s"
                continue

            if (rc_gcc1, out_gcc1, err_gcc1) != (rc_clang, out_clang, err_clang):
                excluded[rel_path] = (
                    f"EXCLUDED_REF_DIVERGENCE: gcc=(rc={rc_gcc1}, out_len={len(out_gcc1)}, err_len={len(err_gcc1)}) vs "
                    f"clang=(rc={rc_clang}, out_len={len(out_clang)}, err_len={len(err_clang)})"
                )
                continue

            # All 6 criteria passed!
            included.append(rel_path)

    # Sort included list
    included = sorted(included)
    print(f"\nMechanical Filter Results:")
    print(f"  Total Evaluated: {len(candidate_files)}")
    print(f"  INCLUDED:        {len(included)}")
    print(f"  EXCLUDED:        {len(excluded)}")

    # Category breakdown of exclusions
    categories = {}
    for p, reason in excluded.items():
        cat = reason.split(":")[0]
        categories[cat] = categories.get(cat, 0) + 1
    print("\nExclusion Breakdown:")
    for cat, count in sorted(categories.items()):
        print(f"  {cat:32s}: {count}")

    # Write output list
    out_file = os.path.join(REPO_ROOT, args.output_list)
    with open(out_file, "w", encoding="utf-8") as f:
        for p in included:
            f.write(p + "\n")

    # Hash the file
    with open(out_file, "rb") as f:
        file_hash = hashlib.sha256(f.read()).hexdigest()

    sha_file = out_file + ".sha256"
    with open(sha_file, "w", encoding="utf-8") as f:
        f.write(f"{file_hash}  {os.path.basename(out_file)}\n")

    print(f"\nFrozen Fixture List:")
    print(f"  Path:       {out_file}")
    print(f"  Count:      {len(included)}")
    print(f"  SHA-256:    {file_hash}")


if __name__ == "__main__":
    main()
