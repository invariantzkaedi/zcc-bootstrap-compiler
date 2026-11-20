#!/usr/bin/env python3
"""
Standalone Lua 5.4.6 Native Win64 PE32+ Builder & Sovereign Verifier
====================================================================
Compiles unmodified upstream Lua 5.4.6 (`tests/embedded_lua/*.c`, 22 core C modules
+ `src/wasm_lua_driver.c`) with `src/win64_app_crt.c` and pure Microsoft x64 ABI
`src/win64_setjmp.s` using zero MSVC/MinGW/binutils via BOTH:
  1. Host `./zcc` -> `build_win64/lua_host_zcc.exe`
  2. Native Windows sovereign `build_win64/zcc_sov_stage3.exe` -> `build_win64/lua.exe`
Verifies SHA-256 + Windows `fc /b` byte-for-byte identity, PE32+ headers,
`msvcrt.dll`-only imports, and real `C:\\Windows\\System32\\msvcrt.dll` export resolution.
"""

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build_win64")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
ZCC_WIN64 = os.path.join(BUILD_DIR, "zcc_sov_stage3.exe")
CMD_EXE = "/mnt/c/Windows/System32/cmd.exe"
MSVCRT_PATH = "/mnt/c/Windows/System32/msvcrt.dll"
DEFAULT_BUNDLE = os.path.join(BUILD_DIR, "lua_win64_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_DIR, "lua.exe")
DEFAULT_HOST_OUT = os.path.join(BUILD_DIR, "lua_host_zcc.exe")

LUA_CORE_FILES = [
    "lapi.c",
    "lcode.c",
    "lctype.c",
    "ldebug.c",
    "ldo.c",
    "ldump.c",
    "lfunc.c",
    "lgc.c",
    "llex.c",
    "lmem.c",
    "lobject.c",
    "lopcodes.c",
    "lparser.c",
    "lstate.c",
    "lstring.c",
    "ltable.c",
    "ltm.c",
    "lundump.c",
    "lvm.c",
    "lzio.c",
    "lauxlib.c",
    "lbaselib.c",
]


def to_rel_posix(path: str) -> str:
    return os.path.relpath(path, REPO_ROOT).replace("\\", "/")


def to_rel_win(path: str) -> str:
    return os.path.relpath(path, REPO_ROOT).replace("/", "\\")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def run_win_cmd(exe_path: str, args: list[str], timeout_s: int = 180) -> subprocess.CompletedProcess:
    if sys.platform == "win32":
        cmd = [exe_path] + list(args)
    else:
        if not os.path.exists(CMD_EXE):
            raise RuntimeError(f"Windows cmd.exe not found at {CMD_EXE}")
        cmd = [CMD_EXE, "/c", to_rel_win(exe_path)] + list(args)
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout_s,
    )


def run_fc_b(path_a: str, path_b: str) -> subprocess.CompletedProcess:
    rel_a = to_rel_win(path_a)
    rel_b = to_rel_win(path_b)
    if sys.platform == "win32":
        cmd = ["cmd.exe", "/c", "fc", "/b", rel_a, rel_b]
    else:
        cmd = [CMD_EXE, "/c", "fc", "/b", rel_a, rel_b]
    return subprocess.run(cmd, cwd=REPO_ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def parse_pe_exports(dll_path: str) -> set[str]:
    with open(dll_path, "rb") as f:
        data = f.read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    coff_off = e_lfanew + 4
    _, num_sec, _, _, _, opt_sz, _ = struct.unpack_from("<HHIIIHH", data, coff_off)
    opt_off = coff_off + 20
    exp_rva, _ = struct.unpack_from("<II", data, opt_off + 112)
    sec_off = opt_off + opt_sz
    secs = []
    for i in range(num_sec):
        vsz, va, rsz, rp = struct.unpack_from("<IIII", data, sec_off + i * 40 + 8)
        secs.append((va, max(vsz, rsz), rp, rsz))

    def r2o(rva: int) -> int:
        for va, span, rp, _ in secs:
            if va <= rva < va + span:
                return rp + (rva - va)
        raise ValueError(f"Unmapped export RVA 0x{rva:x}")

    exp_off = r2o(exp_rva)
    n_names = struct.unpack_from("<I", data, exp_off + 24)[0]
    names_rva = struct.unpack_from("<I", data, exp_off + 32)[0]
    names_off = r2o(names_rva)
    out = set()
    for i in range(n_names):
        name_rva = struct.unpack_from("<I", data, names_off + i * 4)[0]
        off = r2o(name_rva)
        end = data.index(b"\x00", off)
        out.add(data[off:end].decode("ascii", errors="ignore"))
    return out


def audit_pe32plus_msvcrt(exe_path: str) -> dict:
    with open(exe_path, "rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        raise AssertionError(f"Missing MZ header in {exe_path}")
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        raise AssertionError(f"Missing PE\\0\\0 signature in {exe_path}")

    coff_off = e_lfanew + 4
    machine, num_sec, timedate, _, _, opt_sz, _ = struct.unpack_from("<HHIIIHH", data, coff_off)
    if machine != 0x8664:
        raise AssertionError(f"Expected AMD64 0x8664, got 0x{machine:04x}")
    if timedate != 0:
        raise AssertionError(f"Expected TimeDateStamp == 0 for reproducible PE, got {timedate}")

    opt_off = coff_off + 20
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x020B:
        raise AssertionError(f"Expected PE32+ 0x020B, got 0x{magic:04x}")
    stack_reserve = struct.unpack_from("<Q", data, opt_off + 72)[0]
    if stack_reserve < 0x1000000:
        raise AssertionError(f"Expected StackReserve >= 0x1000000, got 0x{stack_reserve:x}")

    ddir_off = opt_off + 112
    dirs = [struct.unpack_from("<II", data, ddir_off + 8 * i) for i in range(16)]
    imp_rva, imp_sz = dirs[1]
    exc_rva, exc_sz = dirs[3]
    rel_rva, rel_sz = dirs[5]
    if exc_rva == 0 or exc_sz == 0:
        raise AssertionError("Empty .pdata EXCEPTION directory")
    if rel_rva == 0 or rel_sz == 0:
        raise AssertionError("Empty .reloc BASERELOC directory")
    if imp_rva == 0 or imp_sz == 0:
        raise AssertionError("Empty IMPORT directory")

    sec_off = opt_off + opt_sz
    sections = {}
    for i in range(num_sec):
        s_off = sec_off + i * 40
        raw_name = data[s_off:s_off + 8]
        name = raw_name.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        vsize, vaddr, raw_sz, raw_ptr = struct.unpack_from("<IIII", data, s_off + 8)
        chars = struct.unpack_from("<I", data, s_off + 36)[0]
        sections[name] = {
            "vsize": vsize,
            "vaddr": vaddr,
            "raw_sz": raw_sz,
            "raw_ptr": raw_ptr,
            "chars": chars,
        }

    if ".text" not in sections or sections[".text"]["chars"] != 0x60000020:
        raise AssertionError(f"Unexpected .text flags: {sections.get('.text')}")
    if ".data" not in sections or sections[".data"]["chars"] != 0xC0000040:
        raise AssertionError(f"Unexpected .data flags: {sections.get('.data')}")

    def r2o(rva: int) -> int:
        for sec in sections.values():
            span = max(sec["vsize"], sec["raw_sz"])
            if sec["vaddr"] <= rva < sec["vaddr"] + span:
                off = sec["raw_ptr"] + (rva - sec["vaddr"])
                if off < sec["raw_ptr"] + sec["raw_sz"]:
                    return off
        raise AssertionError(f"Unmapped PE RVA 0x{rva:x}")

    def read_cstr(off: int) -> str:
        end = data.index(b"\x00", off)
        return data[off:end].decode("ascii", errors="replace")

    dlls = {}
    cur = r2o(imp_rva)
    while True:
        oft, _, _, name_rva, first_thunk = struct.unpack_from("<IIIII", data, cur)
        if name_rva == 0 and first_thunk == 0:
            break
        dll_name = read_cstr(r2o(name_rva))
        t_off = r2o(oft if oft else first_thunk)
        funcs = []
        while True:
            val = struct.unpack_from("<Q", data, t_off)[0]
            if val == 0:
                break
            if (val & (1 << 63)) == 0:
                hn_off = r2o(val & 0xFFFFFFFF)
                funcs.append(read_cstr(hn_off + 2))
            t_off += 8
        dlls[dll_name.lower()] = funcs
        cur += 20

    if list(dlls.keys()) != ["msvcrt.dll"]:
        raise AssertionError(f"Expected ONLY msvcrt.dll imports, got {dlls}")

    if os.path.isfile(MSVCRT_PATH):
        exps = parse_pe_exports(MSVCRT_PATH)
        missing = [s for s in dlls["msvcrt.dll"] if s not in exps]
        if missing:
            raise AssertionError(f"Imported msvcrt symbols missing from System32/msvcrt.dll: {missing}")

    return {
        "bytes": len(data),
        "pdata": exc_sz,
        "reloc": rel_sz,
        "stack_reserve": stack_reserve,
        "msvcrt_imports": dlls["msvcrt.dll"],
    }


def compile_and_link_win64_units(
    units: list[tuple[str, str, list[str]]],
    asm_inputs: list[str],
    work_dir: str,
    out_exe: str,
    use_win64_compiler: bool,
    timeout_s: int = 180,
) -> None:
    os.makedirs(work_dir, exist_ok=True)
    os.makedirs(os.path.dirname(out_exe), exist_ok=True)
    s_paths = []
    for s_name, c_src, extra in units:
        s_out = os.path.join(work_dir, s_name)
        args = ["-mabi=ms", "-D_WIN32"] + list(extra) + ["-S", c_src, "-o", to_rel_posix(s_out)]
        if use_win64_compiler:
            res = run_win_cmd(ZCC_WIN64, args, timeout_s=timeout_s)
        else:
            res = subprocess.run([ZCC_BIN] + args, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)
        if res.returncode != 0 or not os.path.isfile(s_out) or os.path.getsize(s_out) == 0:
            raise RuntimeError(f"Compile failed ({s_name}, rc={res.returncode}):\n{res.stdout}\n{res.stderr}")
        s_paths.append(s_out)

    link_args = (
        ["-target", "win64", "-mabi=ms", "--reproducible"]
        + [to_rel_posix(p) for p in s_paths]
        + [to_rel_posix(p) for p in asm_inputs]
        + ["-o", to_rel_posix(out_exe)]
    )
    if use_win64_compiler:
        res = run_win_cmd(ZCC_WIN64, link_args, timeout_s=timeout_s)
    else:
        res = subprocess.run([ZCC_BIN] + link_args, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)
    if res.returncode != 0 or not os.path.isfile(out_exe) or os.path.getsize(out_exe) < 4096:
        raise RuntimeError(f"Link failed ({out_exe}, rc={res.returncode}):\n{res.stdout}\n{res.stderr}")


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    lines = [
        "/* Auto-generated Standalone Lua 5.4.6 Native Win64 Bundle */\n",
        "#define LUA_USE_JUMPTABLE 0\n",
        "#define LUA_USE_LONGJMP 1\n",
        "#include <stdio.h>\n",
        "#include <stdlib.h>\n",
        "#include <string.h>\n",
    ]
    for fname in LUA_CORE_FILES:
        rel = f"tests/embedded_lua/{fname}"
        full = os.path.join(REPO_ROOT, rel)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing Lua source module: {full}")
        lines.append(f'#include "{rel}"\n')
    driver = os.path.join(REPO_ROOT, "src", "wasm_lua_driver.c")
    if not os.path.isfile(driver):
        raise FileNotFoundError(f"Missing Lua driver: {driver}")
    lines.append('#include "src/wasm_lua_driver.c"\n')
    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(lines)
    return bundle_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone Lua 5.4.6 Win64 PE32+ executable cleanly via ZCC.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--host-output", default=DEFAULT_HOST_OUT)
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    args = parser.parse_args()

    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    bundle_path = generate_bundle(args.bundle)
    bundle_rel = to_rel_posix(bundle_path)
    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("lua_bundle.s", bundle_rel, ["-Itests/embedded_lua", "-Iinclude", "-I."]),
    ]
    asm_inputs = [os.path.join(REPO_ROOT, "src", "win64_setjmp.s")]

    host_dir = os.path.join(BUILD_DIR, "lua_host_asm")
    host_out = args.output if args.skip_win64_selfcompile else args.host_output
    print(f"[BUILD-HOST] ./zcc -mabi=ms -> {to_rel_posix(host_out)}", flush=True)
    compile_and_link_win64_units(units, asm_inputs, host_dir, host_out, use_win64_compiler=False)
    host_sha = sha256_file(host_out)
    print(f"[HOST-PE] {host_out}: {os.path.getsize(host_out)} bytes, SHA256={host_sha}", flush=True)

    if not args.skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        win_dir = os.path.join(BUILD_DIR, "lua_win64_asm")
        print(f"[BUILD-WIN64] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(args.output)}", flush=True)
        compile_and_link_win64_units(units, asm_inputs, win_dir, args.output, use_win64_compiler=True)
        win_sha = sha256_file(args.output)
        print(f"[WIN64-PE] {args.output}: {os.path.getsize(args.output)} bytes, SHA256={win_sha}", flush=True)
        fc = run_fc_b(host_out, args.output)
        print(f"[FC /B] {fc.stdout.strip()}", flush=True)
        if host_sha != win_sha or fc.returncode != 0 or "no differences encountered" not in fc.stdout.lower():
            raise AssertionError(f"Parity mismatch between ./zcc and zcc_sov_stage3.exe: {host_sha} vs {win_sha}")
        print("[PARITY] Byte-identical Lua 5.4.6 Win64 .exe produced by BOTH ./zcc and build_win64/zcc_sov_stage3.exe!", flush=True)

    audit = audit_pe32plus_msvcrt(args.output)
    print(
        f"[AUDIT] PE32+ OK: bytes={audit['bytes']} .pdata={audit['pdata']}B .reloc={audit['reloc']}B "
        f"StackReserve=0x{audit['stack_reserve']:x} Imports=msvcrt.dll({len(audit['msvcrt_imports'])})",
        flush=True,
    )
    print("[PASS] Standalone build_win64/lua.exe validated cleanly (zero MSVC/MinGW/binutils)!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
