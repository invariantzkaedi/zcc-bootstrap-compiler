#!/usr/bin/env python3
"""ZCC Track 21B — Reproducible Hardware GPU Vulkan Verification (Mesa Dozen D3D12 / WSL2 /dev/dxg).

Independently verifies ZCC SPIR-V 1.0 + Vulkan Compute v2 execution against real physical
NVIDIA (0x10de, DISCRETE_GPU) and AMD (0x1002, INTEGRATED_GPU) GPUs exposed in WSL2
through Mesa Dozen (`dzn`) -> D3D12 (`/dev/dxg`) without bypassing Windows Smart App Control.

Preserves ZCC-EF-0 / ZCC-EF-7 epistemic honesty:
- Missing hardware Vulkan ICD / /dev/dxg / spirv-val exits 3 (BLOCKED), never PASS.
- Records non-conformant ICD status (`conformanceVersion 0.0.0.0`) and ASan LSan scope note
  for proprietary Windows D3D12 UMD allocations (`libd3d12core.so`).
- Keeps direct unsigned Win64 native proprietary ICDs (`T21B-GPU-NV-NATIVE`, `T21B-GPU-AMD-NATIVE`)
  explicitly DEFERRED under enforced Windows Smart App Control.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = Path(os.environ.get("ZCC_REPO_ROOT") or Path(__file__).resolve().parents[2]).resolve()
sys.path.insert(0, str(REPO_ROOT))


GPU_TARGETS = [
    {
        "key": "nvidia_rtx5070",
        "gate_id": "T21B-GPU-NV-DZN",
        "vendor_id": "0x10de",
        "expected_device_type": "DISCRETE_GPU",
    },
    {
        "key": "amd_radeon",
        "gate_id": "T21B-GPU-AMD-DZN",
        "vendor_id": "0x1002",
        "expected_device_type": "INTEGRATED_GPU",
    },
]


def resolve_hw_icd(explicit_icd: Optional[str]) -> Optional[Path]:
    if explicit_icd is not None:
        cand = Path(explicit_icd)
        return cand if cand.is_file() else None
    env_icd = os.environ.get("VK_ICD_FILENAMES", "").strip()
    if env_icd:
        for tok in env_icd.split(":"):
            if tok and Path(tok).is_file():
                return Path(tok)
        return None
    for cand in (
        Path("/tmp/dzn_build/build/src/microsoft/vulkan/dzn_devenv_icd.x86_64.json"),
        Path("/usr/share/vulkan/icd.d/dzn_icd.x86_64.json"),
        Path("/etc/vulkan/icd.d/dzn_icd.x86_64.json"),
    ):
        if cand.is_file():
            return cand
    return None


def parse_c_counts(text: str) -> Tuple[int, int]:
    m = re.findall(r"TRACK21B_C_TESTS\s+pass=(\d+)\s+fail=(\d+)", text)
    if not m:
        return 0, -1
    p, f = m[-1]
    return int(p), int(f)


def build_binaries(work_dir: Path) -> Dict[str, Path]:
    so_path = Path("/tmp/libzcc_compute_v2.so")
    cli_path = Path("/tmp/zcc-spirv")
    subprocess.run(
        ["bash", "tests/track21b/build_runtime.sh", "/tmp/t21b_build", str(so_path), str(cli_path)],
        cwd=str(REPO_ROOT),
        check=True,
    )

    ub_dir = work_dir / "ubsan"
    as_dir = work_dir / "asan"
    ub_dir.mkdir(parents=True, exist_ok=True)
    as_dir.mkdir(parents=True, exist_ok=True)

    srcs = [
        "runtime/compute_sha256.c",
        "runtime/cpu_capabilities.c",
        "runtime/compute_scalar.c",
        "runtime/compute_ptx.c",
        "runtime/compute_dispatch.c",
        "runtime/execution_receipt.c",
        "src/part6_spirv.c",
        "runtime/compute_vulkan.c",
        "runtime/compute_dispatch_v2.c",
        "tests/track21b/test_track21b_vulkan.c",
    ]
    pflags_ub = [
        "-std=c99", "-O1", "-g", "-mno-avx", "-mno-avx2", "-mno-fma",
        "-fno-unsafe-math-optimizations", "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-Wall", "-Wextra", "-Werror", "-Iinclude",
    ]
    aflags_ub = [
        "-std=c99", "-O1", "-g", "-mavx2", "-mfma",
        "-fno-unsafe-math-optimizations", "-fsanitize=undefined", "-fno-sanitize-recover=all",
        "-Wall", "-Wextra", "-Werror", "-Iinclude",
    ]
    ub_objs: List[str] = []
    for src in srcs:
        obj = str(ub_dir / f"{Path(src).stem}.o")
        subprocess.run(["gcc", *pflags_ub, "-c", src, "-o", obj], cwd=str(REPO_ROOT), check=True)
        ub_objs.append(obj)
    ub_avx = str(ub_dir / "compute_avx2.o")
    subprocess.run(["gcc", *aflags_ub, "-c", "runtime/compute_avx2.c", "-o", ub_avx], cwd=str(REPO_ROOT), check=True)
    ub_bin = work_dir / "test_track21b_ubsan"
    subprocess.run(["gcc", *pflags_ub, *ub_objs, ub_avx, "-o", str(ub_bin), "-lm", "-ldl"], cwd=str(REPO_ROOT), check=True)

    pflags_as = [
        "-std=c99", "-O1", "-g", "-mno-avx", "-mno-avx2", "-mno-fma",
        "-fno-unsafe-math-optimizations", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-Wall", "-Wextra", "-Werror", "-Iinclude",
    ]
    aflags_as = [
        "-std=c99", "-O1", "-g", "-mavx2", "-mfma",
        "-fno-unsafe-math-optimizations", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-Wall", "-Wextra", "-Werror", "-Iinclude",
    ]
    as_objs: List[str] = []
    for src in srcs:
        obj = str(as_dir / f"{Path(src).stem}.o")
        subprocess.run(["gcc", *pflags_as, "-c", src, "-o", obj], cwd=str(REPO_ROOT), check=True)
        as_objs.append(obj)
    as_avx = str(as_dir / "compute_avx2.o")
    subprocess.run(["gcc", *aflags_as, "-c", "runtime/compute_avx2.c", "-o", as_avx], cwd=str(REPO_ROOT), check=True)
    as_bin = work_dir / "test_track21b_asan"
    subprocess.run(["gcc", *pflags_as, *as_objs, as_avx, "-o", str(as_bin), "-lm", "-ldl"], cwd=str(REPO_ROOT), check=True)

    o2_bin = work_dir / "test_track21b_o2"
    subprocess.run(
        [
            "gcc", "-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-Iinclude",
            "tests/track21b/test_track21b_vulkan.c", str(so_path), "-Wl,-rpath,/tmp",
            "-o", str(o2_bin), "-lm", "-ldl",
        ],
        cwd=str(REPO_ROOT),
        check=True,
    )
    return {"so": so_path, "cli": cli_path, "ubsan": ub_bin, "asan": as_bin, "o2": o2_bin}


def extract_fail_lines(text: str) -> List[str]:
    return [
        ln.strip()
        for ln in text.splitlines()
        if "FAIL[" in ln or "ERROR:" in ln or "DIFF21B_FAIL" in ln or "VK_UNEXPECTED_ERR" in ln
    ][:20]


def verify_target(
    spec: Dict[str, str],
    icd_path: Path,
    bins: Dict[str, Path],
    work_dir: Path,
) -> Dict[str, Any]:
    env = {
        **os.environ,
        "VK_ICD_FILENAMES": str(icd_path),
        "ZCC_VK_VENDOR_ID": spec["vendor_id"],
    }

    print(f"[hw-dzn] {spec['key']} ({spec['vendor_id']}) running UBSan...", flush=True)
    p_ub = subprocess.run([str(bins["ubsan"])], cwd=str(REPO_ROOT), env=env, capture_output=True, text=True)
    ub_p, ub_f = parse_c_counts(p_ub.stdout + "\n" + p_ub.stderr)
    time.sleep(5.0)

    # Proprietary WSL2 D3D12 UMDs retain process-lifetime globals after dlclose; verify ASan memory safety via persistent Vulkan ctx + detect_leaks=0
    print(f"[hw-dzn] {spec['key']} ({spec['vendor_id']}) running ASan...", flush=True)
    p_as = subprocess.run(
        [str(bins["asan"]), "--skip-ptx", "--prefer-ctx"],
        cwd=str(REPO_ROOT),
        env={**env, "ASAN_OPTIONS": "detect_leaks=0"},
        capture_output=True,
        text=True,
    )
    as_p, as_f = parse_c_counts(p_as.stdout + "\n" + p_as.stderr)
    time.sleep(5.0)

    print(f"[hw-dzn] {spec['key']} ({spec['vendor_id']}) running O2 + bench...", flush=True)
    p_o2 = subprocess.run([str(bins["o2"])], cwd=str(REPO_ROOT), env=env, capture_output=True, text=True)
    o2_p, o2_f = parse_c_counts(p_o2.stdout + "\n" + p_o2.stderr)
    # Allow WSL2 /dev/dxg WDDM D3D12 KMD device teardown to drain after high-churn O2 fault/churn tests.
    time.sleep(5.0)

    bench_path = work_dir / f"{spec['key']}_bench.json"
    p_bench = subprocess.run([str(bins["o2"]), "--bench", str(bench_path)], cwd=str(REPO_ROOT), env=env, capture_output=True, text=True)
    bench_data = {}
    if p_bench.returncode == 0 and bench_path.is_file():
        bench_data = json.loads(bench_path.read_text(encoding="utf-8"))
    time.sleep(5.0)

    print(f"[hw-dzn] {spec['key']} ({spec['vendor_id']}) running differential oracle...", flush=True)
    diff_path = work_dir / f"{spec['key']}_diff.json"
    perf_path = work_dir / f"{spec['key']}_perf.json"
    p_diff = subprocess.run(
        [
            sys.executable,
            "tests/track21b/differential_oracle_21b.py",
            "--lib-path", str(bins["so"]),
            "--cli-path", str(bins["cli"]),
            "--report-out", str(diff_path),
            "--perf-out", str(perf_path),
        ],
        cwd=str(REPO_ROOT),
        env=env,
        capture_output=True,
        text=True,
    )
    diff_data = json.loads(diff_path.read_text(encoding="utf-8")) if diff_path.is_file() else {}

    hw = diff_data.get("hardware", {})
    ok = (
        p_ub.returncode == 0 and ub_f == 0 and ub_p >= 100
        and p_as.returncode == 0 and as_f == 0 and as_p >= 100
        and p_o2.returncode == 0 and o2_f == 0 and o2_p >= 100
        and p_bench.returncode == 0
        and p_diff.returncode == 0
        and diff_data.get("verdict") == "PASS"
        and diff_data.get("failed", 1) == 0
        and diff_data.get("replay_13_ops_count") == 13
        and hw.get("vulkan_device_type") == spec["expected_device_type"]
        and str(hw.get("vendor_id", hw.get("vulkan_vendor_id", ""))).lower() == spec["vendor_id"].lower()
        and bool(hw.get("vulkan_validation_enabled"))
    )

    bench_summary = {
        f"{r['backend']}:{r['workload']}": {"avg_us": r["avg_us"], "sha256": r["last_output_sha256"]}
        for r in bench_data.get("results", [])
    }

    fail_lines = (
        extract_fail_lines(p_ub.stdout + "\n" + p_ub.stderr)
        + extract_fail_lines(p_as.stdout + "\n" + p_as.stderr)
        + extract_fail_lines(p_o2.stdout + "\n" + p_o2.stderr)
        + ([f"bench_rc={p_bench.returncode}"] + extract_fail_lines(p_bench.stdout + "\n" + p_bench.stderr)
           if p_bench.returncode != 0 else [])
        + extract_fail_lines(p_diff.stdout + "\n" + p_diff.stderr)
    )

    return {
        "gate_id": spec["gate_id"],
        "target_key": spec["key"],
        "status": "PASS" if ok else "FAIL",
        "driver_bridge": "Mesa Dozen (dzn) SPIR-V->DXIL->D3D12 over /dev/dxg",
        "conformance_version": "0.0.0.0 (non-conformant Dozen testing ICD)",
        "hardware": hw,
        "ubsan": {"rc": p_ub.returncode, "pass": ub_p, "fail": ub_f},
        "asan_no_lsan_external_umd": {"rc": p_as.returncode, "pass": as_p, "fail": as_f},
        "o2": {"rc": p_o2.returncode, "pass": o2_p, "fail": o2_f},
        "fail_lines": fail_lines,
        "differential": {
            "rc": p_diff.returncode,
            "verdict": diff_data.get("verdict"),
            "total_checks": diff_data.get("total_checks"),
            "passed": diff_data.get("passed"),
            "failed": diff_data.get("failed"),
            "replay_13_ops_count": diff_data.get("replay_13_ops_count"),
        },
        "benchmark_highlights": {
            k: v for k, v in bench_summary.items()
            if k.startswith("VULKAN_SPV") or k.startswith("SCALAR_C99:GEMM") or k.startswith("AVX2_FMA:GEMM") or k.startswith("PTX_SM120:GEMM")
        },
    }


def validate_existing_report(report_path: Path) -> Tuple[bool, str]:
    if not report_path.is_file():
        return False, f"missing_hw_report:{report_path}"
    data = json.loads(report_path.read_text(encoding="utf-8"))
    if data.get("schema_version") != "track21b.hw_vulkan_report.v1":
        return False, "hw_report_schema_mismatch"
    if data.get("verdict") != "PASS":
        return False, f"hw_report_verdict:{data.get('verdict')}"
    devs = data.get("devices", {})
    for spec in GPU_TARGETS:
        entry = devs.get(spec["key"])
        if not isinstance(entry, dict) or entry.get("status") != "PASS":
            return False, f"hw_device_not_pass:{spec['key']}"
        hw = entry.get("hardware", {})
        if hw.get("vulkan_device_type") != spec["expected_device_type"]:
            return False, f"hw_device_type_mismatch:{spec['key']}:{hw.get('vulkan_device_type')}"
        if str(hw.get("vulkan_vendor_id", "")).lower() != spec["vendor_id"].lower():
            return False, f"hw_vendor_mismatch:{spec['key']}:{hw.get('vulkan_vendor_id')}"
        diff = entry.get("differential", {})
        if diff.get("verdict") != "PASS" or int(diff.get("failed", 1)) != 0 or int(diff.get("replay_13_ops_count", 0)) != 13:
            return False, f"hw_diff_invalid:{spec['key']}"
    native_gates = data.get("native_windows_gates", {})
    if native_gates.get("T21B-GPU-NV-NATIVE", {}).get("status") != "DEFERRED":
        return False, "native_nv_gate_not_deferred"
    if native_gates.get("T21B-GPU-AMD-NATIVE", {}).get("status") != "DEFERRED":
        return False, "native_amd_gate_not_deferred"
    return True, "ok"


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify Hardware GPUs over Mesa Dozen (dzn) D3D12 Vulkan Bridge")
    parser.add_argument("--icd", default=None, help="Path to dzn ICD json (auto-detected if omitted)")
    parser.add_argument("--out", default=str(REPO_ROOT / "TRACK21B_HW_VULKAN_REPORT.json"))
    parser.add_argument("--check-only", action="store_true", help="Validate existing report without requiring /dev/dxg")
    args = parser.parse_args()

    out_path = Path(args.out)
    if args.check_only:
        ok, msg = validate_existing_report(out_path)
        print(f"TRACK21B_HW_VULKAN_CHECK verdict={'PASS' if ok else 'FAIL'} detail={msg}")
        return 0 if ok else 1

    spirv_val = shutil.which("spirv-val")
    glslang = shutil.which("glslangValidator")
    icd_path = resolve_hw_icd(args.icd)
    if not spirv_val or not glslang or not Path("/dev/dxg").exists() or icd_path is None:
        print(
            f"HW_VULKAN_BLOCKED: spirv_val={bool(spirv_val)} glslang={bool(glslang)} "
            f"dxg={Path('/dev/dxg').exists()} icd={icd_path}",
            file=sys.stderr,
        )
        return 3

    icd_sha256 = hashlib.sha256(icd_path.read_bytes()).hexdigest()
    git_head = (
        subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=str(REPO_ROOT), text=True).strip()
        if (REPO_ROOT / ".git").exists()
        else "UNKNOWN"
    )

    with tempfile.TemporaryDirectory(prefix="t21b_hw_dzn_") as td:
        work_dir = Path(td)
        bins = build_binaries(work_dir)
        devices: Dict[str, Any] = {}
        for spec in GPU_TARGETS:
            devices[spec["key"]] = verify_target(spec, icd_path, bins, work_dir)

    all_ok = all(devices[s["key"]]["status"] == "PASS" for s in GPU_TARGETS)
    nv_gemm_sha = (
        devices["nvidia_rtx5070"]["benchmark_highlights"]
        .get("VULKAN_SPV_CTX:GEMM_F32_128x512x128", {})
        .get("sha256", "")
    )
    amd_gemm_sha = (
        devices["amd_radeon"]["benchmark_highlights"]
        .get("VULKAN_SPV_CTX:GEMM_F32_128x512x128", {})
        .get("sha256", "")
    )
    cross_gpu_gemm_parity = bool(nv_gemm_sha and nv_gemm_sha == amd_gemm_sha)

    report = {
        "schema_version": "track21b.hw_vulkan_report.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "governing_rule_version": "ZCC-EF-7 v1.0.0 + ZCC-EF-0",
        "git_head_sha": git_head,
        "bridge_icd_path": str(icd_path),
        "bridge_icd_sha256": icd_sha256,
        "cross_gpu_gemm_sha256_parity": cross_gpu_gemm_parity,
        "gemm_f32_canonical_sha256": nv_gemm_sha,
        "devices": devices,
        "native_windows_gates": {
            "T21B-GPU-NV-NATIVE": {
                "status": "DEFERRED",
                "target": "Native Win64 NVIDIA RTX 5070 Vulkan ICD 1.4.303 (0x10de)",
                "reason": "Unsigned native Win64 binaries are blocked by enforced Windows Smart App Control (VerifiedAndReputablePolicyState=1).",
            },
            "T21B-GPU-AMD-NATIVE": {
                "status": "DEFERRED",
                "target": "Native Win64 AMD Radeon Vulkan ICD 1.4.329 (0x1002)",
                "reason": "Unsigned native Win64 binaries are blocked by enforced Windows Smart App Control (VerifiedAndReputablePolicyState=1).",
            },
        },
        "limitations": [
            "Mesa Dozen (`dzn`) translates Vulkan SPIR-V 1.0 to DXIL over D3D12 (`/dev/dxg`) and self-reports conformanceVersion 0.0.0.0 (non-conformant testing ICD).",
            "ASan memory-safety execution against D3D12 UMDs (`libd3d12core.so`) uses ASAN_OPTIONS=detect_leaks=0 because Microsoft/IHV D3D12 user-mode drivers retain global driver allocations across dlclose; strict LSan leak checks execute on Mesa llvmpipe.",
            "Direct Win64 proprietary ICD execution remains DEFERRED until binaries carry an Authenticode signature or Smart App Control policy permits native execution.",
        ],
        "verdict": "PASS" if (all_ok and cross_gpu_gemm_parity) else "FAIL",
    }

    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"TRACK21B_HW_VULKAN verdict={report['verdict']} "
        f"nv={devices['nvidia_rtx5070']['status']}({devices['nvidia_rtx5070']['differential']['passed']}/{devices['nvidia_rtx5070']['differential']['total_checks']}) "
        f"amd={devices['amd_radeon']['status']}({devices['amd_radeon']['differential']['passed']}/{devices['amd_radeon']['differential']['total_checks']}) "
        f"gemm_parity={cross_gpu_gemm_parity}"
    )
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
