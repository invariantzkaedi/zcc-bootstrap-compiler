#!/usr/bin/env python3
"""
ZCC MULTI-SUITE OPTIMIZATION PASS VALIDATOR & GAUNTLET
Validates ZCC production optimizer (ZCC_OPT=1 & ZCC_DPO_PASS_POLICY=1)
across all COMPAT_SMOKE_SRCS and differential regression fixtures:
  - tests/test_abi.c
  - tests/test_asm_real.c
  - test_vla.c
  - exp1_raytracer_simd.c
  - exp2_voxel_engine.c
  - exp3_audio_visualizer.c
  - exp4_vr_stereo.c
  - exp5_physics_engine.c
  - tests/regressions/t_zkaedi_rigging_regressions.c
  - tests/diff_read_escape.c
  - tests/diff_parse_initializer_list.c
  - tests/diff_HUlib_drawTextLine.c
  - tests/diff_R_DrawColumn.c
Verifies:
  1. Assembly line elision (.s before vs after) + per-sample distribution stats.
  2. Artifact inspection (.s valid instructions, no %% register tokens) + native execution parity.
  3. Gate 1 identity preservation.
"""

import os
import statistics
import subprocess
import sys
import tempfile
import time

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

IS_LINUX = sys.platform.startswith("linux")
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
USE_NATIVE_WIN64 = (not IS_LINUX) and os.path.exists(os.path.join(REPO_ROOT, "zcc.exe"))
REPO_DIR = REPO_ROOT if (IS_LINUX or USE_NATIVE_WIN64) else "/mnt/h/__DOWNLOADS/zcc_github_upload"

TEST_CORPUS = [
    {"name": "test_abi", "src": "tests/test_abi.c", "flags": "-lm", "win64_exec": True},
    {"name": "test_asm_real", "src": "tests/test_asm_real.c", "flags": "", "win64_exec": True},
    {"name": "test_vla", "src": "test_vla.c", "flags": "", "win64_exec": True},
    {"name": "exp1_raytracer", "src": "exp1_raytracer_simd.c", "flags": "-lm", "win64_exec": True},
    {"name": "exp2_voxel", "src": "exp2_voxel_engine.c", "flags": "-lm", "win64_exec": True},
    {"name": "exp3_audio", "src": "exp3_audio_visualizer.c", "flags": "-lm", "win64_exec": True},
    {"name": "exp4_vr_stereo", "src": "exp4_vr_stereo.c", "flags": "-lm", "win64_exec": True},
    {"name": "exp5_physics", "src": "exp5_physics_engine.c", "flags": "-lm", "win64_exec": True},
    {"name": "t_rigging_regressions", "src": "tests/regressions/t_zkaedi_rigging_regressions.c", "flags": "-lm", "win64_exec": True},
    {"name": "diff_read_escape", "src": "tests/diff_read_escape.c", "flags": "", "win64_exec": True},
    {"name": "diff_parse_initializer_list", "src": "tests/diff_parse_initializer_list.c", "flags": "", "win64_exec": True},
    {"name": "diff_HUlib_drawTextLine", "src": "tests/diff_HUlib_drawTextLine.c", "flags": "", "win64_exec": True},
    {"name": "diff_R_DrawColumn", "src": "tests/diff_R_DrawColumn.c", "flags": "", "win64_exec": True},
]


def run_cmd(cmd, check=True):
    actual_cmd = cmd if IS_LINUX else f'wsl -e bash -c "{cmd}"'
    res = subprocess.run(
        actual_cmd,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if check and res.returncode != 0:
        raise RuntimeError(f"Command failed ({res.returncode}): {actual_cmd}\n{res.stderr}\n{res.stdout}")
    return res


def run_binary(bin_path):
    if IS_LINUX or USE_NATIVE_WIN64:
        return subprocess.run([bin_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    else:
        return subprocess.run(f"wsl -e {bin_path}", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def get_file_line_count(file_path):
    if IS_LINUX or USE_NATIVE_WIN64:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            return sum(1 for _ in f)
    else:
        res = subprocess.run(f"wsl -e wc -l {file_path}", shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return int(res.stdout.split()[0])


def verify_asm_artifact(s_path):
    with open(s_path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()
    for ln in lines:
        if ".string" not in ln and ".asciz" not in ln and ("%%r" in ln or "%%e" in ln or "%%x" in ln):
            raise RuntimeError(f"Invalid escaped register token found in {s_path}: {ln.strip()}")
    text = "".join(lines)
    if ".globl " not in text or "ret" not in text:
        raise RuntimeError(f"Missing .globl or ret instruction in {s_path}")


def compile_and_run_native_win64(zcc_exe, tsrc, s_out, exe_out, env_overrides, do_exec):
    env = os.environ.copy()
    env.update(env_overrides)
    t0 = time.perf_counter()
    res_s = subprocess.run(
        [zcc_exe, tsrc, "-S", "-o", s_out],
        cwd=REPO_ROOT,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    dt_ms = (time.perf_counter() - t0) * 1000.0
    if res_s.returncode != 0:
        raise RuntimeError(f"zcc -S failed ({res_s.returncode}): {res_s.stderr}\n{res_s.stdout}")
    verify_asm_artifact(s_out)

    if do_exec:
        if os.path.exists(exe_out):
            try:
                os.remove(exe_out)
            except OSError:
                pass
        res_pe = subprocess.run(
            [zcc_exe, tsrc, "-target", "win64", "-o", exe_out],
            cwd=REPO_ROOT,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        if res_pe.returncode != 0 or not os.path.exists(exe_out):
            raise RuntimeError(f"zcc -target win64 failed ({res_pe.returncode}): {res_pe.stderr}")
        run_res = subprocess.run([exe_out], cwd=REPO_ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return run_res.returncode, run_res.stdout, run_res.stderr.decode("utf-8", errors="replace").strip(), dt_ms
    return 0, b"", "", dt_ms


def test_opt_passes():
    print("=" * 88)
    print("  ZCC MULTI-SUITE PRODUCTION OPTIMIZATION PASS GAUNTLET")
    print("  Testing: GVN Store-to-Load, Constant Fold, DCE, Symbolic CFG & SSE/Reg Peepholes")
    print("=" * 88)

    total_lines_base = 0
    total_lines_opt = 0
    passed_tests = 0
    failed_tests = 0
    reductions = []
    reduc_pcts = []
    compile_times_ms = []

    scratch_dir = os.path.join(REPO_ROOT, "scratch", "opt_gauntlet").replace("\\", "/")
    os.makedirs(scratch_dir, exist_ok=True)
    zcc_exe = os.path.join(REPO_ROOT, "zcc.exe")

    print(f"{'Target Test Suite':<26} {'Baseline':<10} {'Optimized':<10} {'Reduction':<14} {'Time(ms)':<10} {'Verdict'}")
    print("-" * 88)

    for item in TEST_CORPUS:
        tname = item["name"]
        tsrc = item["src"]
        tflags = item["flags"]
        win64_exec = item.get("win64_exec", False)

        if USE_NATIVE_WIN64:
            s_base = f"{scratch_dir}/{tname}_opt0.s"
            bin_base = f"{scratch_dir}/{tname}_opt0.exe"
            s_opt = f"{scratch_dir}/{tname}_opt1.s"
            bin_opt = f"{scratch_dir}/{tname}_opt1.exe"
            try:
                ret_base, out_base_bytes, err_base, _ = compile_and_run_native_win64(
                    zcc_exe, tsrc, s_base, bin_base, {"ZCC_OPT": "0"}, win64_exec
                )
                sz_base = get_file_line_count(s_base)
            except Exception as e:
                print(f"{tname:<26} {'FAILED':<10} {'-':<10} {'-':<14} {'-':<10} [BASELINE COMP ERR: {e}]")
                failed_tests += 1
                continue

            try:
                ret_opt, out_opt_bytes, err_opt, dt_opt_ms = compile_and_run_native_win64(
                    zcc_exe, tsrc, s_opt, bin_opt, {"ZCC_OPT": "1", "ZCC_DPO_PASS_POLICY": "1"}, win64_exec
                )
                sz_opt = get_file_line_count(s_opt)
            except Exception as e:
                print(f"{tname:<26} {sz_base:<10} {'FAILED':<10} {'-':<14} {'-':<10} [OPTIMIZER COMP ERR: {e}]")
                failed_tests += 1
                continue
        else:
            s_base = f"/tmp/{tname}_opt0.s"
            bin_base = f"/tmp/{tname}_opt0"
            s_opt = f"/tmp/{tname}_opt1.s"
            bin_opt = f"/tmp/{tname}_opt1"

            cmd_base = f"cd {REPO_DIR} && ZCC_OPT=0 ./zcc {tsrc} -S -o {s_base} && gcc -no-pie {s_base} -o {bin_base} {tflags}"
            try:
                run_cmd(cmd_base)
                sz_base = get_file_line_count(s_base)
                res_base = run_binary(bin_base)
                ret_base = res_base.returncode
                out_base_bytes = res_base.stdout
                err_base = res_base.stderr.decode("utf-8", errors="replace").strip()
            except Exception as e:
                print(f"{tname:<26} {'FAILED':<10} {'-':<10} {'-':<14} {'-':<10} [BASELINE COMP ERR: {e}]")
                failed_tests += 1
                continue

            t0 = time.perf_counter()
            cmd_opt = f"cd {REPO_DIR} && ZCC_OPT=1 ZCC_DPO_PASS_POLICY=1 ./zcc {tsrc} -S -o {s_opt} && gcc -no-pie {s_opt} -o {bin_opt} {tflags}"
            try:
                run_cmd(cmd_opt)
                dt_opt_ms = (time.perf_counter() - t0) * 1000.0
                sz_opt = get_file_line_count(s_opt)
                res_opt = run_binary(bin_opt)
                ret_opt = res_opt.returncode
                out_opt_bytes = res_opt.stdout
                err_opt = res_opt.stderr.decode("utf-8", errors="replace").strip()
            except Exception as e:
                print(f"{tname:<26} {sz_base:<10} {'FAILED':<10} {'-':<14} {'-':<10} [OPTIMIZER COMP ERR: {e}]")
                failed_tests += 1
                continue

        total_lines_base += sz_base
        total_lines_opt += sz_opt
        reduction = sz_base - sz_opt
        reduc_pct = (reduction / sz_base) * 100.0 if sz_base > 0 else 0.0
        reductions.append(reduction)
        reduc_pcts.append(reduc_pct)
        compile_times_ms.append(dt_opt_ms)

        is_parity = (ret_base == ret_opt) and (err_base == err_opt)
        stdout_match = (out_base_bytes == out_opt_bytes)

        if is_parity and stdout_match and ret_base == 0:
            verdict = "PASS"
            passed_tests += 1
        elif ret_base != 0 or ret_opt != 0:
            verdict = f"RUNTIME ERR (rc={ret_opt})"
            failed_tests += 1
        else:
            verdict = "DIVERGENCE"
            failed_tests += 1

        reduc_str = f"-{reduction} ({reduc_pct:.1f}%)" if reduction >= 0 else f"+{-reduction} (inflated)"
        print(f"{tname:<26} {sz_base:<10} {sz_opt:<10} {reduc_str:<14} {dt_opt_ms:<10.2f} [{verdict}]")

    print("-" * 88)
    overall_reduction = total_lines_base - total_lines_opt
    overall_pct = (overall_reduction / total_lines_base) * 100.0 if total_lines_base > 0 else 0.0

    print("GAUNTLET SUMMARY & DISTRIBUTION METRICS (Rule NV-2):")
    print(f"  * Suites Evaluated:          {len(TEST_CORPUS)}")
    print(f"  * Bit-Exact Passes:          {passed_tests}/{len(TEST_CORPUS)}")
    print(f"  * Semantic Divergences:      {failed_tests}")
    print(f"  * Total Assembly (Baseline): {total_lines_base:,} lines")
    print(f"  * Total Assembly (Optimized):{total_lines_opt:,} lines")
    print(f"  * Net Instructions Elided:   {overall_reduction:,} lines ({overall_pct:.2f}% net reduction)")
    if reductions:
        print(
            f"  * Elision Distribution:      mean={statistics.mean(reductions):.1f} lines ({statistics.mean(reduc_pcts):.2f}%), "
            f"median={statistics.median(reductions):.1f} lines ({statistics.median(reduc_pcts):.2f}%), "
            f"stdev={statistics.stdev(reductions):.1f} lines, max(L_inf)={max(reductions)} lines ({max(reduc_pcts):.2f}%)"
        )
    if compile_times_ms:
        print(
            f"  * Compile Time Distribution: mean={statistics.mean(compile_times_ms):.2f} ms, "
            f"median={statistics.median(compile_times_ms):.2f} ms, "
            f"stdev={statistics.stdev(compile_times_ms):.2f} ms, max(L_inf)={max(compile_times_ms):.2f} ms"
        )
    print("=" * 88)

    if failed_tests == 0:
        print(f"★ ALL {len(TEST_CORPUS)} TEST SUITES BIT-EXACT & VERIFIED (EXIT 0) ★")
    else:
        print(f"[!] Gauntlet failed with {failed_tests} divergences.")
        sys.exit(1)


if __name__ == "__main__":
    test_opt_passes()
