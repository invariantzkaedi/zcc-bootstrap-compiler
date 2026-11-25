#!/usr/bin/env python3
"""
Sovereign Win64 GPU AI Inference Builder (ZCC C99 -> PTX sm_120 + Win64 PE32+)
================================================================================
1. Compiles `src/win64_gpu_llm_kernels.c` -> Blackwell `sm_120` PTX using both:
     - host `./zcc --target=ptx --ptx-arch=sm_120`
     - sovereign Win64 `build_win64/zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120`
   Verifies byte-for-byte PTX identity (`SHA-256` + `fc /b`) and generates
   `build_win64/gpu_llm_kernels_ptx.h` (`g_gpu_llm_kernels_ptx[]`).
2. Compiles & links `src/win64_gpu_llm.c` (+ Win64 CRT/runtime) into
   `build_win64/zcc_gpu_llm_host_zcc.exe` and `build_win64/zcc_gpu_llm.exe`,
   asserting sovereign PE32+ byte-identity (`SHA-256` + `fc /b`).
3. Audits `build_win64/zcc_gpu_llm.exe` PE32+ import table against live
   `C:\\Windows\\System32\\{msvcrt,nvcuda}.dll` exports only.
4. Trains & exports sovereign 2-layer Llama GQA models (`F32`, `F16`, `Q8_0`)
   via `tools/generate_sovereign_gguf.py` into `build_win64/zcc_tiny_llama_*.gguf`.
"""

import argparse
import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import (
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)
from tools.build_gpu_win64 import (
    audit_pe32plus_multi_dll,
    build_dual_win64_target,
    emit_ptx,
    write_ptx_header,
)


def _find_numpy_python() -> str:
    candidates = [
        "/home/ZKAEDI/.venv/bin/python",
        sys.executable,
    ]
    for py in candidates:
        if os.path.isfile(py):
            probe = subprocess.run(
                [py, "-c", "import numpy"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if probe.returncode == 0:
                return py
    raise RuntimeError(
        "NumPy Python interpreter not found (checked /home/ZKAEDI/.venv/bin/python and sys.executable)"
    )


def build_gpu_llm_artifacts(skip_win64_selfcompile: bool = False, timeout_s: int = 180) -> dict:
    os.makedirs(BUILD_DIR, exist_ok=True)
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")

    kernels_src = os.path.join(REPO_ROOT, "src", "win64_gpu_llm_kernels.c")
    ptx_host = os.path.join(BUILD_DIR, "gpu_llm_kernels_host.ptx")
    ptx_win64 = os.path.join(BUILD_DIR, "gpu_llm_kernels.ptx")
    ptx_header = os.path.join(BUILD_DIR, "gpu_llm_kernels_ptx.h")

    print(f"[PTX-HOST:LLM] ./zcc --target=ptx --ptx-arch=sm_120 -> {to_rel_posix(ptx_host)}", flush=True)
    emit_ptx(False, kernels_src, ptx_host, arch="sm_120", timeout_s=timeout_s)
    host_ptx_sha = sha256_file(ptx_host)
    print(f"[PTX-HOST:LLM] {ptx_host}: {os.path.getsize(ptx_host)} bytes, SHA256={host_ptx_sha}", flush=True)

    active_ptx = ptx_host
    if not skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        print(f"[PTX-WIN64:LLM] zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120 -> {to_rel_posix(ptx_win64)}", flush=True)
        emit_ptx(True, kernels_src, ptx_win64, arch="sm_120", timeout_s=timeout_s)
        win_ptx_sha = sha256_file(ptx_win64)
        print(f"[PTX-WIN64:LLM] {ptx_win64}: {os.path.getsize(ptx_win64)} bytes, SHA256={win_ptx_sha}", flush=True)
        fc_ptx = run_fc_b(ptx_host, ptx_win64)
        print(f"[FC /B:PTX:LLM] {fc_ptx.stdout.strip()}", flush=True)
        if host_ptx_sha != win_ptx_sha or fc_ptx.returncode != 0 or "no differences encountered" not in fc_ptx.stdout.lower():
            raise AssertionError(f"PTX parity mismatch: {host_ptx_sha} vs {win_ptx_sha}")
        active_ptx = ptx_win64
    else:
        with open(ptx_host, "rb") as fin, open(ptx_win64, "wb") as fout:
            fout.write(fin.read())
        active_ptx = ptx_win64

    ptx_len = write_ptx_header(
        active_ptx,
        ptx_header,
        guard="ZCC_GPU_LLM_KERNELS_PTX_H",
        var_name="g_gpu_llm_kernels_ptx",
    )
    print(f"[PTX-HEADER:LLM] {ptx_header}: embedded {ptx_len} bytes -> g_gpu_llm_kernels_ptx", flush=True)

    llm_src = os.path.join(REPO_ROOT, "src", "win64_gpu_llm.c")
    llm_host = os.path.join(BUILD_DIR, "zcc_gpu_llm_host_zcc.exe")
    llm_win = os.path.join(BUILD_DIR, "zcc_gpu_llm.exe")
    build_dual_win64_target(
        "GPU_LLM",
        llm_src,
        "gpu_llm",
        llm_host,
        llm_win,
        skip_win64_selfcompile=skip_win64_selfcompile,
        timeout_s=timeout_s,
    )

    llm_imports = audit_pe32plus_multi_dll(
        llm_win,
        required_dlls=("msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
    )
    print(f"[PE-AUDIT:GPU_LLM] imports={llm_imports}", flush=True)

    py_np = _find_numpy_python()
    gen_script = os.path.join(REPO_ROOT, "tools", "generate_sovereign_gguf.py")
    print(f"[GGUF-GEN] {py_np} {to_rel_posix(gen_script)} --out-dir {to_rel_posix(BUILD_DIR)}", flush=True)
    res = subprocess.run(
        [py_np, gen_script, "--out-dir", BUILD_DIR],
        cwd=REPO_ROOT,
        timeout=timeout_s,
    )
    if res.returncode != 0:
        raise RuntimeError(f"generate_sovereign_gguf.py failed with exit {res.returncode}")

    aot_ptx_paths = {}
    for tag in ("f32", "f16", "q8_0"):
        gguf_p = os.path.join(BUILD_DIR, f"zcc_tiny_llama_{tag}.gguf")
        ptx_h = os.path.join(BUILD_DIR, f"zcc_tiny_llama_{tag}_sm120_host.ptx")
        ptx_w = os.path.join(BUILD_DIR, f"zcc_tiny_llama_{tag}_sm120.ptx")
        print(f"[AOT-PTX-HOST:{tag.upper()}] ./zcc --target=ptx --gpu-arch=sm_120 --model {to_rel_posix(gguf_p)} -> {to_rel_posix(ptx_h)}", flush=True)
        h_res = subprocess.run(
            [ZCC_BIN, "--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_posix(gguf_p), "-o", to_rel_posix(ptx_h)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout_s,
        )
        if h_res.returncode != 0:
            raise RuntimeError(f"Host AOT GGUF->PTX failed for {tag}: {h_res.stdout}\n{h_res.stderr}")
        h_sha = sha256_file(ptx_h)
        if not skip_win64_selfcompile:
            print(f"[AOT-PTX-WIN64:{tag.upper()}] zcc_sov_stage3.exe --target=ptx --gpu-arch=sm_120 --model {to_rel_win(gguf_p)} -> {to_rel_win(ptx_w)}", flush=True)
            w_res = run_win_cmd(
                ZCC_WIN64,
                ["--target=ptx", "--gpu-arch=sm_120", "--model", to_rel_win(gguf_p), "-o", to_rel_win(ptx_w)],
                timeout_s=timeout_s,
            )
            if w_res.returncode != 0:
                raise RuntimeError(f"Win64 AOT GGUF->PTX failed for {tag}: {w_res.stdout}\n{w_res.stderr}")
            w_sha = sha256_file(ptx_w)
            fc_a = run_fc_b(ptx_h, ptx_w)
            print(f"[AOT-PTX:{tag.upper()}] {os.path.getsize(ptx_w)}B SHA256={w_sha} fc={fc_a.stdout.strip()}", flush=True)
            if h_sha != w_sha or fc_a.returncode != 0 or "no differences encountered" not in fc_a.stdout.lower():
                raise AssertionError(f"AOT PTX parity mismatch for {tag}: {h_sha} vs {w_sha}")
        else:
            with open(ptx_h, "rb") as fin, open(ptx_w, "wb") as fout:
                fout.write(fin.read())
        aot_ptx_paths[tag] = ptx_w

    return {
        "ptx_path": active_ptx,
        "ptx_sha256": sha256_file(active_ptx),
        "ptx_bytes": ptx_len,
        "llm_exe": llm_win,
        "llm_sha256": sha256_file(llm_win),
        "llm_imports": llm_imports,
        "gguf_f32": os.path.join(BUILD_DIR, "zcc_tiny_llama_f32.gguf"),
        "gguf_f16": os.path.join(BUILD_DIR, "zcc_tiny_llama_f16.gguf"),
        "gguf_q8_0": os.path.join(BUILD_DIR, "zcc_tiny_llama_q8_0.gguf"),
        "aot_ptx": aot_ptx_paths,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 GPU GGUF inference executable + GGUF models.")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    build_gpu_llm_artifacts(skip_win64_selfcompile=args.skip_win64_selfcompile, timeout_s=args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
