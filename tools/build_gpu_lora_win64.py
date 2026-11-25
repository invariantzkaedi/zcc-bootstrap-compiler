#!/usr/bin/env python3
"""
Sovereign Zero-Toolchain Win64 + Live NVIDIA RTX 5070 LoRA Fine-Tuner Builder (Track 19)
==========================================================================================
Compiles `src/win64_gpu_lora_kernels.c` -> Blackwell PTX ISA 8.7 (`sm_120`) using BOTH
host `./zcc` and Win64 `build_win64/zcc_sov_stage3.exe`, asserts byte-identical PTX parity
(`SHA-256` + `fc /b`), embeds `gpu_lora_kernels_ptx.h` (alongside `gpu_llm_kernels_ptx.h`),
dual-compiles `src/win64_gpu_lora.c` -> `build_win64/zcc_gpu_lora.exe`, asserts PE32+
byte identity, and audits strict System32 imports (`msvcrt.dll`, `nvcuda.dll` ONLY).
"""

import argparse
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_gpu_win64 import audit_pe32plus_multi_dll, write_ptx_header  # noqa: E402
from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    compile_and_link_win64_units,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)

LORA_KERNEL_SRC = os.path.join(REPO_ROOT, "src", "win64_gpu_lora_kernels.c")
PTX_HOST_OUT = os.path.join(BUILD_DIR, "gpu_lora_kernels_host.ptx")
PTX_WIN64_OUT = os.path.join(BUILD_DIR, "gpu_lora_kernels.ptx")
PTX_HEADER_OUT = os.path.join(BUILD_DIR, "gpu_lora_kernels_ptx.h")
LLM_PTX_HEADER = os.path.join(BUILD_DIR, "gpu_llm_kernels_ptx.h")

LORA_EXE_OUT = os.path.join(BUILD_DIR, "zcc_gpu_lora.exe")
LORA_EXE_HOST_OUT = os.path.join(BUILD_DIR, "zcc_gpu_lora_host_zcc.exe")


def ensure_llm_ptx_header() -> None:
    if os.path.isfile(LLM_PTX_HEADER) and os.path.getsize(LLM_PTX_HEADER) > 1000:
        return
    res = subprocess.run([sys.executable, os.path.join(SCRIPT_DIR, "build_gpu_llm_win64.py")], cwd=REPO_ROOT, timeout=240)
    if res.returncode != 0 or not os.path.isfile(LLM_PTX_HEADER):
        raise RuntimeError(f"Failed generating {LLM_PTX_HEADER}")


def build_lora(skip_win64_selfcompile: bool = False, arch: str = "sm_120", timeout_s: int = 180) -> dict:
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found — run `make zcc` first.")
    if not skip_win64_selfcompile and not os.path.isfile(ZCC_WIN64):
        raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")

    os.makedirs(BUILD_DIR, exist_ok=True)
    ensure_llm_ptx_header()

    ptx_host_target = PTX_WIN64_OUT if skip_win64_selfcompile else PTX_HOST_OUT
    print(f"[PTX-HOST:LORA] ./zcc --target=ptx --ptx-arch={arch} -> {to_rel_posix(ptx_host_target)}", flush=True)
    r_host = subprocess.run(
        [ZCC_BIN, "--target=ptx", f"--ptx-arch={arch}", to_rel_posix(LORA_KERNEL_SRC), "-o", to_rel_posix(ptx_host_target)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=timeout_s,
    )
    if r_host.returncode != 0 or not os.path.isfile(ptx_host_target):
        raise RuntimeError(f"Host LoRA PTX compile failed:\n{r_host.stdout}\n{r_host.stderr}")
    sha_ptx_host = sha256_file(ptx_host_target)
    print(f"[PTX-HOST:LORA] {ptx_host_target}: {os.path.getsize(ptx_host_target)}B SHA256={sha_ptx_host}", flush=True)

    if not skip_win64_selfcompile:
        print(f"[PTX-WIN64:LORA] zcc_sov_stage3.exe --target=ptx --ptx-arch={arch} -> {to_rel_posix(PTX_WIN64_OUT)}", flush=True)
        r_win = run_win_cmd(
            ZCC_WIN64,
            ["--target=ptx", f"--ptx-arch={arch}", to_rel_win(LORA_KERNEL_SRC), "-o", to_rel_win(PTX_WIN64_OUT)],
            timeout_s=timeout_s,
        )
        if r_win.returncode != 0 or not os.path.isfile(PTX_WIN64_OUT):
            raise RuntimeError(f"Win64 LoRA PTX compile failed:\n{r_win.stdout}\n{r_win.stderr}")
        sha_ptx_win = sha256_file(PTX_WIN64_OUT)
        fc_ptx = run_fc_b(ptx_host_target, PTX_WIN64_OUT)
        print(f"[FC /B:PTX:LORA] {os.path.getsize(PTX_WIN64_OUT)}B SHA256={sha_ptx_win} {fc_ptx.stdout.strip()}", flush=True)
        if sha_ptx_host != sha_ptx_win or fc_ptx.returncode != 0 or "no differences encountered" not in fc_ptx.stdout.lower():
            raise AssertionError(f"LoRA PTX parity mismatch: {sha_ptx_host} vs {sha_ptx_win}")

    ptx_bytes = write_ptx_header(
        PTX_WIN64_OUT,
        PTX_HEADER_OUT,
        guard="ZCC_GPU_LORA_KERNELS_PTX_H",
        var_name="g_gpu_lora_kernels_ptx",
    )
    print(f"[PTX-HEADER:LORA] {PTX_HEADER_OUT}: embedded {ptx_bytes} bytes -> g_gpu_lora_kernels_ptx", flush=True)

    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        ("win64_gpu_lora.s", "src/win64_gpu_lora.c", ["-Ibuild_win64", "-Iinclude", "-Isrc", "-I."]),
    ]
    eff_host = LORA_EXE_OUT if skip_win64_selfcompile else LORA_EXE_HOST_OUT
    print(f"[BUILD-HOST:LORA] ./zcc -mabi=ms -> {to_rel_posix(eff_host)}", flush=True)
    compile_and_link_win64_units(
        units, [], os.path.join(BUILD_DIR, "gpu_lora_host_asm"), eff_host,
        use_win64_compiler=False, timeout_s=timeout_s
    )
    sha_exe_host = sha256_file(eff_host)
    print(f"[HOST-PE:LORA] {eff_host}: {os.path.getsize(eff_host)}B SHA256={sha_exe_host}", flush=True)

    if not skip_win64_selfcompile:
        print(f"[BUILD-WIN64:LORA] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(LORA_EXE_OUT)}", flush=True)
        compile_and_link_win64_units(
            units, [], os.path.join(BUILD_DIR, "gpu_lora_win64_asm"), LORA_EXE_OUT,
            use_win64_compiler=True, timeout_s=timeout_s
        )
        sha_exe_win = sha256_file(LORA_EXE_OUT)
        fc_exe = run_fc_b(eff_host, LORA_EXE_OUT)
        print(f"[FC /B:LORA] {os.path.getsize(LORA_EXE_OUT)}B SHA256={sha_exe_win} {fc_exe.stdout.strip()}", flush=True)
        if sha_exe_host != sha_exe_win or fc_exe.returncode != 0 or "no differences encountered" not in fc_exe.stdout.lower():
            raise AssertionError(f"LoRA PE32+ parity mismatch: {sha_exe_host} vs {sha_exe_win}")

    imports = audit_pe32plus_multi_dll(
        LORA_EXE_OUT,
        required_dlls=("msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
    )
    print(f"[PE-AUDIT:LORA] bytes={imports['bytes']} imports={sorted(imports['dlls'].keys())}", flush=True)
    return {
        "exe": LORA_EXE_OUT,
        "exe_sha256": sha256_file(LORA_EXE_OUT),
        "exe_bytes": os.path.getsize(LORA_EXE_OUT),
        "ptx": PTX_WIN64_OUT,
        "ptx_sha256": sha256_file(PTX_WIN64_OUT),
        "ptx_bytes": os.path.getsize(PTX_WIN64_OUT),
        "imports": imports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 GPU LoRA fine-tuner via ZCC.")
    parser.add_argument("--arch", default="sm_120")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    build_lora(skip_win64_selfcompile=args.skip_win64_selfcompile, arch=args.arch, timeout_s=args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
