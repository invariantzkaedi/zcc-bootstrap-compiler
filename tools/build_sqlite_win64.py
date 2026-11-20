#!/usr/bin/env python3
"""
Standalone SQLite 3.45.0 Native Win64 PE32+ Builder & Sovereign Verifier
========================================================================
Compiles unmodified upstream `src/sqlite3.c` (9.0 MB) + `src/win64_app_crt.c` +
`src/win64_sqlite_driver.c` with zero MSVC/MinGW/binutils via BOTH:
  1. Host `./zcc` -> `build_win64/sqlite3_host_zcc.exe`
  2. Native Windows sovereign `build_win64/zcc_sov_stage3.exe` -> `build_win64/sqlite3.exe`
Verifies SHA-256 + Windows `fc /b` byte-for-byte identity, PE32+ headers,
`msvcrt.dll`-only imports, and real `C:\\Windows\\System32\\msvcrt.dll` export resolution.
"""

import argparse
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    REPO_ROOT,
    ZCC_BIN,
    ZCC_WIN64,
    audit_pe32plus_msvcrt,
    compile_and_link_win64_units,
    run_fc_b,
    sha256_file,
    to_rel_posix,
)

DEFAULT_WRAPPER = os.path.join(BUILD_DIR, "sqlite3_win64_unit.c")
DEFAULT_OUT = os.path.join(BUILD_DIR, "sqlite3.exe")
DEFAULT_HOST_OUT = os.path.join(BUILD_DIR, "sqlite3_host_zcc.exe")


def generate_sqlite_wrapper(wrapper_path: str = DEFAULT_WRAPPER) -> str:
    os.makedirs(os.path.dirname(wrapper_path), exist_ok=True)
    for req in ("src/sqlite3.c", "src/sqlite3.h", "src/win64_app_crt.c", "src/win64_sqlite_driver.c"):
        full = os.path.join(REPO_ROOT, req)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing required SQLite Win64 source: {full}")
    txt = (
        "/* Auto-generated Standalone SQLite 3.45.0 Native Win64 Unit Wrapper */\n"
        "#define SQLITE_THREADSAFE 0\n"
        "#define SQLITE_OMIT_LOAD_EXTENSION 1\n"
        "#define SQLITE_OMIT_WAL 1\n"
        "#define SQLITE_TEMP_STORE 3\n"
        "#define SQLITE_OS_OTHER 1\n"
        "#define SQLITE_ENABLE_MEMORY_MANAGEMENT 0\n"
        "#define LONGDOUBLE_TYPE double\n"
        '#include "src/sqlite3.c"\n'
    )
    with open(wrapper_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(txt)
    return wrapper_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone SQLite 3.45.0 Win64 PE32+ executable cleanly via ZCC.")
    parser.add_argument("--wrapper", default=DEFAULT_WRAPPER)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--host-output", default=DEFAULT_HOST_OUT)
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()

    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    wrapper_path = generate_sqlite_wrapper(args.wrapper)
    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_sqlite_driver.s", "src/win64_sqlite_driver.c", ["-Isrc", "-Iinclude", "-I."]),
        ("sqlite3_win64_unit.s", to_rel_posix(wrapper_path), ["-DLONGDOUBLE_TYPE=double", "-Isrc", "-Iinclude", "-I."]),
    ]

    host_dir = os.path.join(BUILD_DIR, "sqlite3_host_asm")
    host_out = args.output if args.skip_win64_selfcompile else args.host_output
    print(f"[BUILD-HOST] ./zcc -mabi=ms -> {to_rel_posix(host_out)}", flush=True)
    compile_and_link_win64_units(units, [], host_dir, host_out, use_win64_compiler=False, timeout_s=args.timeout)
    host_sha = sha256_file(host_out)
    print(f"[HOST-PE] {host_out}: {os.path.getsize(host_out)} bytes, SHA256={host_sha}", flush=True)

    if not args.skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        win_dir = os.path.join(BUILD_DIR, "sqlite3_win64_asm")
        print(f"[BUILD-WIN64] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(args.output)}", flush=True)
        compile_and_link_win64_units(units, [], win_dir, args.output, use_win64_compiler=True, timeout_s=args.timeout)
        win_sha = sha256_file(args.output)
        print(f"[WIN64-PE] {args.output}: {os.path.getsize(args.output)} bytes, SHA256={win_sha}", flush=True)
        fc = run_fc_b(host_out, args.output)
        print(f"[FC /B] {fc.stdout.strip()}", flush=True)
        if host_sha != win_sha or fc.returncode != 0 or "no differences encountered" not in fc.stdout.lower():
            raise AssertionError(f"Parity mismatch between ./zcc and zcc_sov_stage3.exe: {host_sha} vs {win_sha}")
        print("[PARITY] Byte-identical SQLite 3.45.0 Win64 .exe produced by BOTH ./zcc and build_win64/zcc_sov_stage3.exe!", flush=True)

    audit = audit_pe32plus_msvcrt(args.output)
    print(
        f"[AUDIT] PE32+ OK: bytes={audit['bytes']} .pdata={audit['pdata']}B .reloc={audit['reloc']}B "
        f"StackReserve=0x{audit['stack_reserve']:x} Imports=msvcrt.dll({len(audit['msvcrt_imports'])})",
        flush=True,
    )
    print("[PASS] Standalone build_win64/sqlite3.exe validated cleanly (zero MSVC/MinGW/binutils)!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
