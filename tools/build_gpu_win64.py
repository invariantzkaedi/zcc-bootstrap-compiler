#!/usr/bin/env python3
"""
Sovereign Zero-Toolchain Win64 + Live NVIDIA RTX 5070 PTX GPU Builder
=======================================================================
Compiles `src/win64_gpu_kernels.c` -> NVIDIA PTX ISA 8.7 (`sm_120`) using BOTH:
  1. Host `./zcc --target=ptx --ptx-arch=sm_120`
  2. Native Windows sovereign `build_win64/zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120`

Asserts byte-identical PTX parity (`fc /b` + SHA-256), embeds the sovereign PTX
into `build_win64/gpu_kernels_ptx.h`, and compiles two standalone Win64 PE32+
executables with ZERO MSVC / MinGW / binutils / nvcc / CUDA Toolkit:
  - `build_win64/zcc_gpu_compute.exe` (`msvcrt.dll` + `nvcuda.dll`)
  - `build_win64/zcc_gpu_live.exe` (`msvcrt.dll` + `KERNEL32.dll` + `USER32.dll` + `GDI32.dll` + `nvcuda.dll`)

Verifies SHA-256 + Windows `fc /b` byte-for-byte identity against `./zcc`
counterparts (`*_host_zcc.exe`), audits PE32+ headers (`.pdata`, `.reloc`,
`StackReserve >= 16MB`, `TimeDateStamp == 0`), and resolves EVERY imported symbol
against real `C:\\Windows\\System32\\*.dll` export tables.
"""

import argparse
import os
import struct
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    REPO_ROOT,
    ZCC_BIN,
    ZCC_WIN64,
    compile_and_link_win64_units,
    parse_pe_exports,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
)

KERNEL_SRC = os.path.join(REPO_ROOT, "src", "win64_gpu_kernels.c")
PTX_HOST_OUT = os.path.join(BUILD_DIR, "gpu_kernels_host.ptx")
PTX_WIN64_OUT = os.path.join(BUILD_DIR, "gpu_kernels.ptx")
PTX_HEADER_OUT = os.path.join(BUILD_DIR, "gpu_kernels_ptx.h")

COMPUTE_OUT = os.path.join(BUILD_DIR, "zcc_gpu_compute.exe")
COMPUTE_HOST_OUT = os.path.join(BUILD_DIR, "zcc_gpu_compute_host_zcc.exe")
LIVE_OUT = os.path.join(BUILD_DIR, "zcc_gpu_live.exe")
LIVE_HOST_OUT = os.path.join(BUILD_DIR, "zcc_gpu_live_host_zcc.exe")


def _sys32_path(name_lower: str) -> str:
    if sys.platform == "win32":
        return os.path.join(r"C:\Windows\System32", name_lower)
    return f"/mnt/c/Windows/System32/{name_lower}"


SYSTEM32_DLL_PATHS = {
    "msvcrt.dll": _sys32_path("msvcrt.dll"),
    "kernel32.dll": _sys32_path("kernel32.dll"),
    "user32.dll": _sys32_path("user32.dll"),
    "gdi32.dll": _sys32_path("gdi32.dll"),
    "nvcuda.dll": _sys32_path("nvcuda.dll"),
    "ws2_32.dll": _sys32_path("ws2_32.dll"),
}


def audit_pe32plus_multi_dll(
    exe_path: str,
    required_dlls: tuple[str, ...],
    allowed_dlls: tuple[str, ...] | None = None,
) -> dict:
    """Validate PE32+ headers, sections, .pdata/.reloc, and System32 DLL exports."""
    if allowed_dlls is None:
        allowed_dlls = required_dlls
    allowed_lc = {d.lower(): d for d in allowed_dlls}
    required_lc = [d.lower() for d in required_dlls]

    with open(exe_path, "rb") as f:
        data = f.read()

    if len(data) < 0x200 or data[:2] != b"MZ":
        raise AssertionError(f"Missing MZ signature in {exe_path}")
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_off + 0x110 > len(data) or data[pe_off : pe_off + 4] != b"PE\x00\x00":
        raise AssertionError(f"Invalid PE signature in {exe_path}")

    machine, num_sec, timedate, _, _, opt_sz, _ = struct.unpack_from("<HHIIIHH", data, pe_off + 4)
    if machine != 0x8664:
        raise AssertionError(f"Expected AMD64 0x8664, got 0x{machine:04x}")
    if timedate != 0:
        raise AssertionError(f"Expected TimeDateStamp == 0 for reproducible PE, got {timedate}")

    opt_off = pe_off + 24
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x020B:
        raise AssertionError(f"Expected PE32+ magic 0x020B, got 0x{magic:04x}")
    stack_reserve = struct.unpack_from("<Q", data, opt_off + 72)[0]
    if stack_reserve < 0x1000000:
        raise AssertionError(f"Expected StackReserve >= 0x1000000, got 0x{stack_reserve:x}")

    dd_off = opt_off + 112
    imp_rva, imp_sz = struct.unpack_from("<II", data, dd_off + 1 * 8)
    exc_rva, exc_sz = struct.unpack_from("<II", data, dd_off + 3 * 8)
    rel_rva, rel_sz = struct.unpack_from("<II", data, dd_off + 5 * 8)
    if exc_rva == 0 or exc_sz == 0:
        raise AssertionError("Empty .pdata EXCEPTION directory")
    if rel_rva == 0 or rel_sz == 0:
        raise AssertionError("Empty .reloc BASERELOC directory")
    if imp_rva == 0 or imp_sz == 0:
        raise AssertionError("Empty IMPORT directory")

    sec_off = opt_off + opt_sz
    sections = {}
    sec_spans = []
    for i in range(num_sec):
        s_off = sec_off + i * 40
        raw_name = data[s_off : s_off + 8]
        name = raw_name.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        vsz, va, rsz, rp = struct.unpack_from("<IIII", data, s_off + 8)
        chars = struct.unpack_from("<I", data, s_off + 36)[0]
        sections[name] = {
            "vsize": vsz,
            "vaddr": va,
            "raw_sz": rsz,
            "raw_ptr": rp,
            "chars": chars,
        }
        sec_spans.append((va, max(vsz, rsz), rp, rsz))

    if ".text" not in sections or sections[".text"]["chars"] != 0x60000020:
        raise AssertionError(f"Unexpected .text flags: {sections.get('.text')}")
    if ".data" not in sections or sections[".data"]["chars"] != 0xC0000040:
        raise AssertionError(f"Unexpected .data flags: {sections.get('.data')}")

    def rva_to_off(rva: int) -> int:
        for va, span, rp, rsz in sec_spans:
            if va <= rva < va + span:
                off = rp + (rva - va)
                if off < rp + rsz:
                    return off
        raise AssertionError(f"Unmapped PE RVA 0x{rva:x} in {exe_path}")

    def read_cstr_rva(rva: int) -> str:
        off = rva_to_off(rva)
        end = data.index(b"\x00", off)
        return data[off:end].decode("ascii", errors="replace")

    dll_exports = {}
    for dll_lc in allowed_lc:
        dll_path = SYSTEM32_DLL_PATHS.get(dll_lc)
        if not dll_path or not os.path.isfile(dll_path):
            raise FileNotFoundError(f"Missing Windows System32 DLL for export audit: {dll_path}")
        dll_exports[dll_lc] = parse_pe_exports(dll_path)

    dlls = {}
    dlls_lc = {}
    cur = rva_to_off(imp_rva)
    while True:
        orig_first, _, _, name_rva, first_thunk = struct.unpack_from("<IIIII", data, cur)
        if orig_first == 0 and name_rva == 0 and first_thunk == 0:
            break
        dll_name = read_cstr_rva(name_rva)
        dll_lc = dll_name.lower()
        if dll_lc not in allowed_lc:
            raise AssertionError(f"Unexpected DLL import {dll_name!r}; allowed={list(allowed_dlls)}")
        exports = dll_exports[dll_lc]
        thunk_rva = orig_first if orig_first != 0 else first_thunk
        t_off = rva_to_off(thunk_rva)
        funcs = []
        while True:
            val = struct.unpack_from("<Q", data, t_off)[0]
            if val == 0:
                break
            if val & (1 << 63):
                raise AssertionError(f"Unexpected ordinal import 0x{val:x} from {dll_name}")
            fn = read_cstr_rva(int((val & 0xFFFFFFFF) + 2))
            if fn not in exports:
                raise AssertionError(f"Imported symbol {dll_name}!{fn} missing from {SYSTEM32_DLL_PATHS[dll_lc]}")
            funcs.append(fn)
            t_off += 8
        dlls[dll_name] = funcs
        dlls_lc[dll_lc] = funcs
        cur += 20

    for req_lc in required_lc:
        if req_lc not in dlls_lc or not dlls_lc[req_lc]:
            raise AssertionError(f"Missing required DLL import descriptor {req_lc!r}; found={list(dlls.keys())}")

    return {
        "bytes": len(data),
        "pdata": exc_sz,
        "reloc": rel_sz,
        "stack_reserve": stack_reserve,
        "dlls": dlls,
    }


def emit_ptx(compiler_is_win64: bool, src_path: str, out_ptx: str, arch: str = "sm_120", timeout_s: int = 120) -> None:
    os.makedirs(os.path.dirname(out_ptx), exist_ok=True)
    args = ["--target=ptx", f"--ptx-arch={arch}", to_rel_posix(src_path), "-o", to_rel_posix(out_ptx)]
    if compiler_is_win64:
        res = run_win_cmd(ZCC_WIN64, args, timeout_s=timeout_s)
    else:
        res = subprocess.run([ZCC_BIN] + args, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)
    if res.returncode != 0 or not os.path.isfile(out_ptx) or os.path.getsize(out_ptx) == 0:
        raise RuntimeError(f"PTX emission failed ({out_ptx}, rc={res.returncode}):\n{res.stdout}\n{res.stderr}")


def write_ptx_header(
    ptx_path: str,
    header_path: str,
    guard: str = "ZCC_GPU_KERNELS_PTX_H",
    var_name: str = "g_gpu_kernels_ptx",
) -> int:
    with open(ptx_path, "rb") as f:
        ptx_bytes = f.read()
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    lines = [
        f"/* Auto-generated from sovereign {to_rel_posix(ptx_path)} */\n",
        f"#ifndef {guard}\n",
        f"#define {guard}\n",
        f"static const unsigned char {var_name}[] = {{\n",
    ]
    data = ptx_bytes + b"\x00"
    for off in range(0, len(data), 16):
        chunk = data[off : off + 16]
        hex_items = ", ".join(f"0x{b:02X}" for b in chunk)
        comma = "," if (off + 16 < len(data)) else ""
        lines.append(f"    {hex_items}{comma}\n")
    lines.append("};\n")
    lines.append(f"#endif /* {guard} */\n")
    with open(header_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(lines)
    return len(ptx_bytes)


def build_dual_win64_target(
    label: str,
    c_src: str,
    work_prefix: str,
    host_out: str,
    win64_out: str,
    skip_win64_selfcompile: bool,
    timeout_s: int,
) -> str:
    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        (f"{work_prefix}.s", to_rel_posix(c_src), ["-Ibuild_win64", "-Iinclude", "-I."]),
    ]
    effective_host = win64_out if skip_win64_selfcompile else host_out
    print(f"[BUILD-HOST:{label}] ./zcc -mabi=ms -> {to_rel_posix(effective_host)}", flush=True)
    compile_and_link_win64_units(
        units,
        [],
        os.path.join(BUILD_DIR, f"{work_prefix}_host_asm"),
        effective_host,
        use_win64_compiler=False,
        timeout_s=timeout_s,
    )
    host_sha = sha256_file(effective_host)
    print(f"[HOST-PE:{label}] {effective_host}: {os.path.getsize(effective_host)} bytes, SHA256={host_sha}", flush=True)

    if not skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        print(f"[BUILD-WIN64:{label}] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(win64_out)}", flush=True)
        compile_and_link_win64_units(
            units,
            [],
            os.path.join(BUILD_DIR, f"{work_prefix}_win64_asm"),
            win64_out,
            use_win64_compiler=True,
            timeout_s=timeout_s,
        )
        win_sha = sha256_file(win64_out)
        print(f"[WIN64-PE:{label}] {win64_out}: {os.path.getsize(win64_out)} bytes, SHA256={win_sha}", flush=True)
        fc = run_fc_b(effective_host, win64_out)
        print(f"[FC /B:{label}] {fc.stdout.strip()}", flush=True)
        if host_sha != win_sha or fc.returncode != 0 or "no differences encountered" not in fc.stdout.lower():
            raise AssertionError(f"PE parity mismatch for {label}: {host_sha} vs {win_sha}")
    return win64_out


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 GPU compute + live shader binaries cleanly via ZCC.")
    parser.add_argument("--arch", default="sm_120")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()

    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found; run `make zcc` first.")
    if not os.path.isfile(KERNEL_SRC):
        raise FileNotFoundError(f"Missing GPU kernel source: {KERNEL_SRC}")

    print(f"[PTX-HOST] ./zcc --target=ptx --ptx-arch={args.arch} -> {to_rel_posix(PTX_HOST_OUT)}", flush=True)
    emit_ptx(False, KERNEL_SRC, PTX_HOST_OUT, arch=args.arch, timeout_s=args.timeout)
    host_ptx_sha = sha256_file(PTX_HOST_OUT)
    print(f"[PTX-HOST] {PTX_HOST_OUT}: {os.path.getsize(PTX_HOST_OUT)} bytes, SHA256={host_ptx_sha}", flush=True)

    if args.skip_win64_selfcompile:
        ptx_active = PTX_HOST_OUT
    else:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        print(f"[PTX-WIN64] zcc_sov_stage3.exe --target=ptx --ptx-arch={args.arch} -> {to_rel_posix(PTX_WIN64_OUT)}", flush=True)
        emit_ptx(True, KERNEL_SRC, PTX_WIN64_OUT, arch=args.arch, timeout_s=args.timeout)
        win_ptx_sha = sha256_file(PTX_WIN64_OUT)
        print(f"[PTX-WIN64] {PTX_WIN64_OUT}: {os.path.getsize(PTX_WIN64_OUT)} bytes, SHA256={win_ptx_sha}", flush=True)
        fc_ptx = run_fc_b(PTX_HOST_OUT, PTX_WIN64_OUT)
        print(f"[FC /B:PTX] {fc_ptx.stdout.strip()}", flush=True)
        if host_ptx_sha != win_ptx_sha or fc_ptx.returncode != 0 or "no differences encountered" not in fc_ptx.stdout.lower():
            raise AssertionError(f"PTX parity mismatch between ./zcc and zcc_sov_stage3.exe: {host_ptx_sha} vs {win_ptx_sha}")
        ptx_active = PTX_WIN64_OUT

    ptx_len = write_ptx_header(ptx_active, PTX_HEADER_OUT)
    print(f"[HEADER] {to_rel_posix(PTX_HEADER_OUT)}: embedded {ptx_len} PTX bytes", flush=True)

    build_dual_win64_target(
        label="COMPUTE",
        c_src=os.path.join(REPO_ROOT, "src", "win64_gpu_compute.c"),
        work_prefix="gpu_compute",
        host_out=COMPUTE_HOST_OUT,
        win64_out=COMPUTE_OUT,
        skip_win64_selfcompile=args.skip_win64_selfcompile,
        timeout_s=args.timeout,
    )
    audit_comp = audit_pe32plus_multi_dll(
        COMPUTE_OUT,
        required_dlls=("msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
    )
    comp_dlls = ", ".join(f"{k}({len(v)})" for k, v in audit_comp["dlls"].items())
    print(
        f"[AUDIT:COMPUTE] PE32+ OK: bytes={audit_comp['bytes']} .pdata={audit_comp['pdata']}B "
        f".reloc={audit_comp['reloc']}B StackReserve=0x{audit_comp['stack_reserve']:x} Imports=[{comp_dlls}]",
        flush=True,
    )

    build_dual_win64_target(
        label="LIVE",
        c_src=os.path.join(REPO_ROOT, "src", "win64_gpu_live.c"),
        work_prefix="gpu_live",
        host_out=LIVE_HOST_OUT,
        win64_out=LIVE_OUT,
        skip_win64_selfcompile=args.skip_win64_selfcompile,
        timeout_s=args.timeout,
    )
    audit_live = audit_pe32plus_multi_dll(
        LIVE_OUT,
        required_dlls=("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll", "nvcuda.dll"),
        allowed_dlls=("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll", "nvcuda.dll"),
    )
    live_dlls = ", ".join(f"{k}({len(v)})" for k, v in audit_live["dlls"].items())
    print(
        f"[AUDIT:LIVE] PE32+ OK: bytes={audit_live['bytes']} .pdata={audit_live['pdata']}B "
        f".reloc={audit_live['reloc']}B StackReserve=0x{audit_live['stack_reserve']:x} Imports=[{live_dlls}]",
        flush=True,
    )

    print("[PASS] Sovereign Win64 GPU Compute & Live Shader binaries built and audited (zero MSVC/MinGW/nvcc)!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
