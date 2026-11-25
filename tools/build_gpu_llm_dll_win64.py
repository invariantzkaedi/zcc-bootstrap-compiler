#!/usr/bin/env python3
"""
Sovereign Zero-Toolchain Win64 GPU LLM DLL & C Host Builder (Track 17)
=======================================================================
Compiles `src/win64_gpu_llm_dll.c` + `src/win64_dll_crt.c` -> `build_win64/zcc_gpu_llm.dll`
and `src/win64_gpu_llm_dll_demo.c` + `src/win64_app_crt.c` -> `build_win64/zcc_gpu_llm_dll_demo.exe`
using BOTH host `./zcc` and Win64 `build_win64/zcc_sov_stage3.exe` with ZERO MSVC,
ZERO MinGW, ZERO binutils, ZERO nvcc, and ZERO CUDA Toolkit.

Asserts:
  1. Byte-identical PE32+ DLL and EXE parity (`SHA-256` + Windows `fc /b`).
  2. Strict System32 IAT auditing (`{msvcrt.dll, nvcuda.dll}` for DLL,
     `{kernel32.dll, msvcrt.dll}` for demo host EXE).
  3. PE32+ DLL header & sorted `.edata` export directory invariants:
     exact 6 exports: zcc_llm_create, zcc_llm_destroy, zcc_llm_generate,
     zcc_llm_get_stats, zcc_llm_last_error, zcc_llm_reset.
"""

import argparse
import os
import struct
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from build_gpu_win64 import audit_pe32plus_multi_dll  # noqa: E402
from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    REPO_ROOT,
    ZCC_BIN,
    ZCC_WIN64,
    compile_and_link_win64_units,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
)

EXPECTED_DLL_EXPORTS = [
    "zcc_llm_create",
    "zcc_llm_destroy",
    "zcc_llm_generate",
    "zcc_llm_get_stats",
    "zcc_llm_last_error",
    "zcc_llm_reset",
]

DLL_OUT = os.path.join(BUILD_DIR, "zcc_gpu_llm.dll")
DLL_HOST_OUT = os.path.join(BUILD_DIR, "gpu_llm_dll_host_out", "zcc_gpu_llm.dll")
DEMO_OUT = os.path.join(BUILD_DIR, "zcc_gpu_llm_dll_demo.exe")
DEMO_HOST_OUT = os.path.join(BUILD_DIR, "zcc_gpu_llm_dll_demo_host_zcc.exe")
PTX_HEADER = os.path.join(BUILD_DIR, "gpu_llm_kernels_ptx.h")


def compile_and_link_win64_dll_units(
    units: list[tuple[str, str, list[str]]],
    work_dir: str,
    out_dll: str,
    use_win64_compiler: bool,
    timeout_s: int = 180,
) -> None:
    os.makedirs(work_dir, exist_ok=True)
    os.makedirs(os.path.dirname(out_dll), exist_ok=True)
    s_paths = []
    for s_name, c_src, extra in units:
        s_out = os.path.join(work_dir, s_name)
        args = ["-mabi=ms", "-D_WIN32"] + list(extra) + ["-S", c_src, "-o", to_rel_posix(s_out)]
        if use_win64_compiler:
            res = run_win_cmd(ZCC_WIN64, args, timeout_s=timeout_s)
        else:
            res = subprocess.run([ZCC_BIN] + args, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)
        if res.returncode != 0 or not os.path.isfile(s_out) or os.path.getsize(s_out) == 0:
            raise RuntimeError(f"DLL unit compile failed ({s_name}, rc={res.returncode}):\n{res.stdout}\n{res.stderr}")
        s_paths.append(s_out)

    link_args = (
        ["-target", "win64", "-mabi=ms", "-shared", "--reproducible"]
        + [to_rel_posix(p) for p in s_paths]
        + ["-o", to_rel_posix(out_dll)]
    )
    if use_win64_compiler:
        res = run_win_cmd(ZCC_WIN64, link_args, timeout_s=timeout_s)
    else:
        res = subprocess.run([ZCC_BIN] + link_args, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)
    if res.returncode != 0 or not os.path.isfile(out_dll) or os.path.getsize(out_dll) < 4096:
        raise RuntimeError(f"DLL link failed ({out_dll}, rc={res.returncode}):\n{res.stdout}\n{res.stderr}")


def audit_win64_dll_exports(dll_path: str, expected_exports: list[str]) -> dict:
    with open(dll_path, "rb") as f:
        data = f.read()

    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_off : pe_off + 4] != b"PE\x00\x00":
        raise AssertionError(f"Invalid PE signature in {dll_path}")
    _, num_sec, _, _, _, opt_sz, file_chars = struct.unpack_from("<HHIIIHH", data, pe_off + 4)
    if (file_chars & 0x2000) == 0:
        raise AssertionError(f"Missing IMAGE_FILE_DLL (0x2000) in Characteristics=0x{file_chars:04x}")

    opt_off = pe_off + 24
    entry_rva = struct.unpack_from("<I", data, opt_off + 16)[0]
    image_base = struct.unpack_from("<Q", data, opt_off + 24)[0]
    if entry_rva == 0:
        raise AssertionError("DLL AddressOfEntryPoint is 0 (expected non-zero DllMain)")
    if image_base != 0x180000000:
        raise AssertionError(f"Expected DLL ImageBase 0x180000000, got 0x{image_base:x}")

    dd_off = opt_off + 112
    exp_rva, exp_sz = struct.unpack_from("<II", data, dd_off + 0 * 8)
    imp_rva, imp_sz = struct.unpack_from("<II", data, dd_off + 1 * 8)
    if exp_rva == 0 or exp_sz == 0:
        raise AssertionError("Empty EXPORT directory (.edata)")
    if imp_rva == 0 or imp_sz == 0:
        raise AssertionError("Empty IMPORT directory (.idata)")
    if imp_rva >= exp_rva:
        raise AssertionError(f"Expected .idata (0x{imp_rva:x}) before .edata (0x{exp_rva:x})")

    sec_off = opt_off + opt_sz
    sec_spans = []
    sec_names = []
    for i in range(num_sec):
        s_off = sec_off + i * 40
        name = data[s_off : s_off + 8].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        vsz, va, rsz, rp = struct.unpack_from("<IIII", data, s_off + 8)
        sec_names.append(name)
        sec_spans.append((va, max(vsz, rsz), rp, rsz))

    def rva_to_off(rva: int) -> int:
        for va, span, rp, rsz in sec_spans:
            if va <= rva < va + span:
                off = rp + (rva - va)
                if off < rp + rsz:
                    return off
        raise AssertionError(f"Unmapped PE RVA 0x{rva:x} in {dll_path}")

    def read_cstr_rva(rva: int) -> str:
        off = rva_to_off(rva)
        end = data.index(b"\x00", off)
        return data[off:end].decode("ascii", errors="replace")

    exp_off = rva_to_off(exp_rva)
    (_, _, _, _, name_rva, base, n_funcs, n_names, eat_rva, ent_rva, eot_rva) = struct.unpack_from(
        "<IIHHIIIIIII", data, exp_off
    )
    dll_internal_name = read_cstr_rva(name_rva)
    if n_funcs != len(expected_exports) or n_names != len(expected_exports):
        raise AssertionError(
            f"Unexpected export counts: n_funcs={n_funcs}, n_names={n_names}, expected={len(expected_exports)}"
        )

    eat_off = rva_to_off(eat_rva)
    ent_off = rva_to_off(ent_rva)
    eot_off = rva_to_off(eot_rva)

    names = []
    rva_map = {}
    for idx in range(n_names):
        sym_name_rva = struct.unpack_from("<I", data, ent_off + idx * 4)[0]
        sym_name = read_cstr_rva(sym_name_rva)
        ord_idx = struct.unpack_from("<H", data, eot_off + idx * 2)[0]
        func_rva = struct.unpack_from("<I", data, eat_off + ord_idx * 4)[0]
        if func_rva == 0:
            raise AssertionError(f"Export {sym_name} has zero RVA")
        names.append(sym_name)
        rva_map[sym_name] = func_rva

    expected_sorted = sorted(expected_exports)
    if names != sorted(names):
        raise AssertionError(f"DLL exports not sorted lexicographically for PE binary search: {names}")
    if names != expected_sorted:
        raise AssertionError(f"DLL exports mismatch: got={names}, expected={expected_sorted}")

    return {
        "dll_name": dll_internal_name,
        "entry_rva": entry_rva,
        "image_base": image_base,
        "sections": sec_names,
        "base": base,
        "exports": names,
        "export_rvas": rva_map,
    }


def ensure_generic_ptx_header() -> None:
    if os.path.isfile(PTX_HEADER) and os.path.getsize(PTX_HEADER) > 1000:
        return
    build_script = os.path.join(SCRIPT_DIR, "build_gpu_llm_win64.py")
    res = subprocess.run([sys.executable, build_script], cwd=REPO_ROOT, capture_output=False, timeout=240)
    if res.returncode != 0 or not os.path.isfile(PTX_HEADER):
        raise RuntimeError(f"Failed generating {PTX_HEADER} via build_gpu_llm_win64.py")


def build_dll_and_demo(skip_win64_selfcompile: bool = False, timeout_s: int = 180) -> dict:
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    if not skip_win64_selfcompile and not os.path.isfile(ZCC_WIN64):
        raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")

    ensure_generic_ptx_header()

    dll_units = [
        ("win64_dll_crt.s", "src/win64_dll_crt.c", ["-Iinclude", "-I."]),
        ("win64_gpu_llm_dll.s", "src/win64_gpu_llm_dll.c", ["-Ibuild_win64", "-Iinclude", "-Isrc", "-I."]),
    ]
    eff_dll_host = DLL_OUT if skip_win64_selfcompile else DLL_HOST_OUT
    print(f"[BUILD-HOST:DLL] ./zcc -shared -mabi=ms -> {to_rel_posix(eff_dll_host)}", flush=True)
    compile_and_link_win64_dll_units(
        dll_units, os.path.join(BUILD_DIR, "gpu_llm_dll_host_asm"), eff_dll_host,
        use_win64_compiler=False, timeout_s=timeout_s
    )
    sha_dll_host = sha256_file(eff_dll_host)
    print(f"[HOST-DLL] {eff_dll_host}: {os.path.getsize(eff_dll_host)}B SHA256={sha_dll_host}", flush=True)

    if not skip_win64_selfcompile:
        print(f"[BUILD-WIN64:DLL] zcc_sov_stage3.exe -shared -mabi=ms -> {to_rel_posix(DLL_OUT)}", flush=True)
        compile_and_link_win64_dll_units(
            dll_units, os.path.join(BUILD_DIR, "gpu_llm_dll_win64_asm"), DLL_OUT,
            use_win64_compiler=True, timeout_s=timeout_s
        )
        sha_dll_win = sha256_file(DLL_OUT)
        fc_dll = run_fc_b(eff_dll_host, DLL_OUT)
        print(f"[FC /B:DLL] {os.path.getsize(DLL_OUT)}B SHA256={sha_dll_win} {fc_dll.stdout.strip()}", flush=True)
        if sha_dll_host != sha_dll_win or fc_dll.returncode != 0 or "no differences encountered" not in fc_dll.stdout.lower():
            raise AssertionError(f"DLL PE32+ parity mismatch: {sha_dll_host} vs {sha_dll_win}")

    dll_imports = audit_pe32plus_multi_dll(
        DLL_OUT,
        required_dlls=("msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
    )
    dll_exports = audit_win64_dll_exports(DLL_OUT, EXPECTED_DLL_EXPORTS)
    print(
        f"[PE-AUDIT:DLL] bytes={dll_imports['bytes']} imports={sorted(dll_imports['dlls'].keys())} "
        f"entry=0x{dll_exports['entry_rva']:x} exports={dll_exports['exports']}",
        flush=True,
    )

    demo_units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_gpu_llm_dll_demo.s", "src/win64_gpu_llm_dll_demo.c", ["-Iinclude", "-I."]),
    ]
    eff_demo_host = DEMO_OUT if skip_win64_selfcompile else DEMO_HOST_OUT
    print(f"[BUILD-HOST:DEMO] ./zcc -mabi=ms -> {to_rel_posix(eff_demo_host)}", flush=True)
    compile_and_link_win64_units(
        demo_units, [], os.path.join(BUILD_DIR, "gpu_llm_demo_host_asm"), eff_demo_host,
        use_win64_compiler=False, timeout_s=timeout_s
    )
    sha_demo_host = sha256_file(eff_demo_host)

    if not skip_win64_selfcompile:
        print(f"[BUILD-WIN64:DEMO] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(DEMO_OUT)}", flush=True)
        compile_and_link_win64_units(
            demo_units, [], os.path.join(BUILD_DIR, "gpu_llm_demo_win64_asm"), DEMO_OUT,
            use_win64_compiler=True, timeout_s=timeout_s
        )
        sha_demo_win = sha256_file(DEMO_OUT)
        fc_demo = run_fc_b(eff_demo_host, DEMO_OUT)
        print(f"[FC /B:DEMO] {os.path.getsize(DEMO_OUT)}B SHA256={sha_demo_win} {fc_demo.stdout.strip()}", flush=True)
        if sha_demo_host != sha_demo_win or fc_demo.returncode != 0 or "no differences encountered" not in fc_demo.stdout.lower():
            raise AssertionError(f"Demo EXE PE32+ parity mismatch: {sha_demo_host} vs {sha_demo_win}")

    demo_imports = audit_pe32plus_multi_dll(
        DEMO_OUT,
        required_dlls=("kernel32.dll", "msvcrt.dll"),
        allowed_dlls=("kernel32.dll", "msvcrt.dll"),
    )
    print(
        f"[PE-AUDIT:DEMO] bytes={demo_imports['bytes']} imports={sorted(demo_imports['dlls'].keys())}",
        flush=True,
    )

    return {
        "dll": DLL_OUT,
        "dll_sha256": sha256_file(DLL_OUT),
        "dll_bytes": os.path.getsize(DLL_OUT),
        "dll_imports": dll_imports,
        "dll_exports": dll_exports,
        "demo": DEMO_OUT,
        "demo_sha256": sha256_file(DEMO_OUT),
        "demo_bytes": os.path.getsize(DEMO_OUT),
        "demo_imports": demo_imports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 zcc_gpu_llm.dll + C host demo via ZCC.")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    build_dll_and_demo(skip_win64_selfcompile=args.skip_win64_selfcompile, timeout_s=args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
