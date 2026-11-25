#!/usr/bin/env python3
"""Sovereign Win64 Hybrid Quantum-Classical Transformer Builder (Track 13)
===========================================================================
1. Compiles `src/win64_gpu_qllm_kernels.c` -> Blackwell `sm_120` PTX using both:
     - host `./zcc --target=ptx --ptx-arch=sm_120`
     - sovereign Win64 `build_win64/zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120`
   Verifies byte-for-byte PTX identity (`SHA-256` + `fc /b`) and generates
   `build_win64/gpu_qllm_kernels_ptx.h` (`g_gpu_qllm_kernels_ptx[]`).
2. Compiles & links `src/win64_gpu_qllm.c` (+ Win64 CRT/runtime) into
   `build_win64/zcc_gpu_qllm_host_zcc.exe` and `build_win64/zcc_gpu_qllm.exe`,
   asserting sovereign PE32+ byte-identity (`SHA-256` + `fc /b`).
3. Audits `build_win64/zcc_gpu_qllm.exe` PE32+ import table against live
   `C:\\Windows\\System32\\{msvcrt,nvcuda}.dll` exports only.
"""

from __future__ import annotations

import argparse
import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    sha256_file,
    to_rel_posix,
)
from tools.build_gpu_win64 import (  # noqa: E402
    audit_pe32plus_multi_dll,
    build_dual_win64_target,
    emit_ptx,
    write_ptx_header,
)


def build_gpu_qllm_artifacts(skip_win64_selfcompile: bool = False, timeout_s: int = 180) -> dict:
    os.makedirs(BUILD_DIR, exist_ok=True)
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")

    kernels_src = os.path.join(REPO_ROOT, "src", "win64_gpu_qllm_kernels.c")
    ptx_host = os.path.join(BUILD_DIR, "gpu_qllm_kernels_host.ptx")
    ptx_win64 = os.path.join(BUILD_DIR, "gpu_qllm_kernels.ptx")
    ptx_header = os.path.join(BUILD_DIR, "gpu_qllm_kernels_ptx.h")

    print(f"[PTX-HOST:QLLM] ./zcc --target=ptx --ptx-arch=sm_120 -> {to_rel_posix(ptx_host)}", flush=True)
    emit_ptx(False, kernels_src, ptx_host, arch="sm_120", timeout_s=timeout_s)
    host_ptx_sha = sha256_file(ptx_host)
    print(f"[PTX-HOST:QLLM] {ptx_host}: {os.path.getsize(ptx_host)} bytes, SHA256={host_ptx_sha}", flush=True)

    active_ptx = ptx_host
    if not skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        print(f"[PTX-WIN64:QLLM] zcc_sov_stage3.exe --target=ptx --ptx-arch=sm_120 -> {to_rel_posix(ptx_win64)}", flush=True)
        emit_ptx(True, kernels_src, ptx_win64, arch="sm_120", timeout_s=timeout_s)
        win_ptx_sha = sha256_file(ptx_win64)
        print(f"[PTX-WIN64:QLLM] {ptx_win64}: {os.path.getsize(ptx_win64)} bytes, SHA256={win_ptx_sha}", flush=True)
        fc_ptx = run_fc_b(ptx_host, ptx_win64)
        print(f"[FC /B:PTX:QLLM] {fc_ptx.stdout.strip()}", flush=True)
        if host_ptx_sha != win_ptx_sha or fc_ptx.returncode != 0 or "no differences encountered" not in fc_ptx.stdout.lower():
            raise AssertionError(f"QLLM PTX parity mismatch: {host_ptx_sha} vs {win_ptx_sha}")
        active_ptx = ptx_win64
    else:
        with open(ptx_host, "rb") as fin, open(ptx_win64, "wb") as fout:
            fout.write(fin.read())
        active_ptx = ptx_win64

    ptx_len = write_ptx_header(
        active_ptx,
        ptx_header,
        guard="ZCC_GPU_QLLM_KERNELS_PTX_H",
        var_name="g_gpu_qllm_kernels_ptx",
    )
    print(f"[PTX-HEADER:QLLM] {ptx_header}: embedded {ptx_len} bytes -> g_gpu_qllm_kernels_ptx", flush=True)

    qllm_src = os.path.join(REPO_ROOT, "src", "win64_gpu_qllm.c")
    qllm_host = os.path.join(BUILD_DIR, "zcc_gpu_qllm_host_zcc.exe")
    qllm_win = os.path.join(BUILD_DIR, "zcc_gpu_qllm.exe")
    build_dual_win64_target(
        "GPU_QLLM",
        qllm_src,
        "gpu_qllm",
        qllm_host,
        qllm_win,
        skip_win64_selfcompile=skip_win64_selfcompile,
        timeout_s=timeout_s,
    )

    qllm_imports = audit_pe32plus_multi_dll(
        qllm_win,
        required_dlls=("msvcrt.dll", "nvcuda.dll"),
        allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
    )
    print(f"[PE-AUDIT:GPU_QLLM] imports={qllm_imports}", flush=True)

    return {
        "ptx_path": active_ptx,
        "ptx_sha256": sha256_file(active_ptx),
        "ptx_bytes": ptx_len,
        "qllm_exe": qllm_win,
        "qllm_sha256": sha256_file(qllm_win),
        "qllm_imports": qllm_imports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build sovereign Win64 Hybrid Quantum Transformer trainer + inference .exe.")
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    build_gpu_qllm_artifacts(skip_win64_selfcompile=args.skip_win64_selfcompile, timeout_s=args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
