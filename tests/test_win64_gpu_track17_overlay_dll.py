#!/usr/bin/env python3
"""
Sovereign Multi-GiB Sealed PE Overlay & Zero-Toolchain Win64 GPU DLL Gate (Track 17)
====================================================================================
Verification suite (ZCC-EF-0 & ZCC-EF-7 Evidence Tier E2):
  1. DLL PE32+ v9 structural integrity & dual-compiler parity (FC /B + SHA-256,
     IMAGE_FILE_DLL, ImageBase=0x180000000, DllMain entry, sorted exact 6 .edata
     exports, strict System32 IAT {msvcrt.dll, nvcuda.dll} and {kernel32.dll, msvcrt.dll}).
  2. Tiny GGUF sealed PE overlay (--embed-overlay): empty-dir execution parity +
     ZCC-EF-0 tamper negative controls T17-NC-1..T17-NC-3 (1-bit weight flip, footer
     magic corruption, 1-byte EOF truncation -> TAMPER_DETECTED before GPU kernel).
  3. Real 986 MB Qwen2.5-1.5B sealed overlay (zkaedi_prime_1_5b_standalone.exe):
     empty-dir standalone run (GPU_LLM_SEAL verified=1, "Paris") + temp weight tamper NC.
  4. Native C host (zcc_gpu_llm_dll_demo.exe) via LoadLibraryA + GetProcAddress +
     live streaming callback + zcc_llm_reset multi-turn on RTX 5070.
  5. C#/.NET P/Invoke host (tools/test_dll_csharp_pinvoke.ps1) calling zcc_gpu_llm.dll
     directly on RTX 5070 (64B/48B sequential structs + delegate callback + reset).
  6. DLL ZCC-EF-0 negative controls (missing GGUF, bad kv_type=9, missing AOT PTX,
     NULL handle generate -> non-crash error return + zcc_llm_last_error()).
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_gpu_llm_dll_win64 import (  # noqa: E402
    DEMO_HOST_OUT,
    DEMO_OUT,
    DLL_HOST_OUT,
    DLL_OUT,
    EXPECTED_DLL_EXPORTS,
    audit_win64_dll_exports,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll  # noqa: E402
from tools.build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_win,
)
from tools.compile_gguf_exe import compile_gguf_exe  # noqa: E402

TINY_GGUF_F16 = os.path.join(BUILD_DIR, "zcc_tiny_llama_f16.gguf")
REAL_1_5B_GGUF = "/mnt/h/models/zkaedi-prime-dpo-q4_k_m.gguf" if sys.platform != "win32" else r"H:\models\zkaedi-prime-dpo-q4_k_m.gguf"
REAL_STANDALONE_EXE = os.path.join(BUILD_DIR, "zkaedi_prime_1_5b_standalone.exe")
PS1_CSHARP = os.path.join(REPO_ROOT, "tools", "test_dll_csharp_pinvoke.ps1")


def _to_win_abs(path: str) -> str:
    ap = os.path.abspath(path)
    if sys.platform == "win32":
        return ap
    if ap.startswith("/mnt/") and len(ap) >= 7 and ap[6] == "/":
        drive = ap[5].upper()
        rest = ap[7:].replace("/", "\\")
        return f"{drive}:\\{rest}"
    res = subprocess.run(["wslpath", "-w", ap], capture_output=True, text=True, check=True)
    return res.stdout.strip()


def _find_real_1_5b_ptx() -> str | None:
    candidates = (
        os.path.join(BUILD_DIR, "gguf_exe_zkaedi_prime_1_5b_standalone", "zkaedi_prime_1_5b_standalone_sm_120.ptx"),
        os.path.join(BUILD_DIR, "zkaedi_prime_1_5b_sm120.ptx"),
    )
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def _run_win_exe_in_empty_dir(exe_path: str, args: list[str], timeout_s: int = 120) -> subprocess.CompletedProcess:
    exe_win = _to_win_abs(exe_path)
    ps_args = ", ".join("'" + a.replace("'", "''") + "'" for a in args)
    ps_cmd = (
        "$d = Join-Path $env:TEMP ('zcc_t17_' + [guid]::NewGuid().ToString('N').Substring(0,8)); "
        "New-Item -ItemType Directory -Path $d | Out-Null; "
        "Push-Location $d; "
        "try { "
        + f"& '{exe_win}' {ps_args}; exit $LASTEXITCODE "
        + "} finally { Pop-Location; Remove-Item -Recurse -Force $d }"
    )
    exe_ps = "powershell.exe" if sys.platform != "win32" else "powershell"
    return subprocess.run(
        [exe_ps, "-NoProfile", "-Command", ps_cmd],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=timeout_s,
    )


def _run_powershell_file(script_path: str, args: list[str], timeout_s: int = 120) -> subprocess.CompletedProcess:
    exe_ps = "powershell.exe" if sys.platform != "win32" else "powershell"
    cmd = [exe_ps, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", _to_win_abs(script_path)] + args
    return subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, timeout=timeout_s)


class TestWin64GpuTrack17OverlayDll(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (ZCC_BIN, ZCC_WIN64, DLL_OUT, DEMO_OUT, PS1_CSHARP, TINY_GGUF_F16)
        missing = [p for p in required if not os.path.isfile(p)]
        if missing:
            raise unittest.SkipTest(
                f"Missing required Track 17 artifacts ({missing}); run "
                "`python3 tools/build_gpu_llm_dll_win64.py` and `python3 tools/build_gpu_llm_win64.py` first."
            )

    def test_01_dll_pe32plus_v9_structural_and_dual_parity(self) -> None:
        if os.path.isfile(DLL_HOST_OUT):
            self.assertEqual(sha256_file(DLL_HOST_OUT), sha256_file(DLL_OUT))
            fc_dll = run_fc_b(DLL_HOST_OUT, DLL_OUT)
            self.assertEqual(fc_dll.returncode, 0, fc_dll.stdout + fc_dll.stderr)
            self.assertIn("no differences encountered", fc_dll.stdout.lower())

        if os.path.isfile(DEMO_HOST_OUT):
            self.assertEqual(sha256_file(DEMO_HOST_OUT), sha256_file(DEMO_OUT))
            fc_demo = run_fc_b(DEMO_HOST_OUT, DEMO_OUT)
            self.assertEqual(fc_demo.returncode, 0, fc_demo.stdout + fc_demo.stderr)
            self.assertIn("no differences encountered", fc_demo.stdout.lower())

        dll_imp = audit_pe32plus_multi_dll(
            DLL_OUT,
            required_dlls=("msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
        )
        self.assertGreater(dll_imp["bytes"], 0)
        exp_info = audit_win64_dll_exports(DLL_OUT, EXPECTED_DLL_EXPORTS)
        self.assertEqual(exp_info["exports"], sorted(EXPECTED_DLL_EXPORTS))
        self.assertNotEqual(exp_info["entry_rva"], 0)
        self.assertEqual(exp_info["image_base"], 0x180000000)

        demo_imp = audit_pe32plus_multi_dll(
            DEMO_OUT,
            required_dlls=("kernel32.dll", "msvcrt.dll"),
            allowed_dlls=("kernel32.dll", "msvcrt.dll"),
        )
        self.assertIn("LoadLibraryA", demo_imp["dlls"]["KERNEL32.dll"])
        self.assertIn("GetProcAddress", demo_imp["dlls"]["KERNEL32.dll"])

    def test_02_tiny_pe_overlay_empty_dir_parity_and_tamper_nc(self) -> None:
        with tempfile.TemporaryDirectory(dir=BUILD_DIR, prefix="_t17_tiny_ov_") as tdir:
            ov_exe = os.path.join(tdir, "tiny_overlay.exe")
            info = compile_gguf_exe(
                model_path=TINY_GGUF_F16,
                output_exe=ov_exe,
                embed_overlay=True,
                timeout_s=180,
            )
            self.assertGreater(info["overlay_offset"], info["pe_base_bytes"] - 1)
            self.assertEqual(info["exe_bytes"] - info["overlay_offset"] - 64, os.path.getsize(TINY_GGUF_F16))

            r = _run_win_exe_in_empty_dir(
                ov_exe,
                ["--prompt", "ZCC compil", "--steps", "16", "--temp", "0", "--seed", "42"],
                timeout_s=60,
            )
            combined = r.stdout + "\n" + r.stderr
            self.assertEqual(r.returncode, 0, combined)
            self.assertIn("GPU_LLM_SEAL verified=1 mode=pe_overlay", combined)
            self.assertIn("mode=aot_overlay", combined)
            self.assertIn("es C99 directly", combined)
            self.assertIn("GPU_LLM_OK", combined)

            # T17-NC-1: Flip 1 bit inside sealed GGUF weight payload
            nc1 = os.path.join(tdir, "tiny_nc1_weight_flip.exe")
            shutil.copyfile(ov_exe, nc1)
            with open(nc1, "r+b") as f:
                flip_pos = info["overlay_offset"] + (os.path.getsize(TINY_GGUF_F16) // 2)
                f.seek(flip_pos)
                b = f.read(1)
                f.seek(flip_pos)
                f.write(bytes([b[0] ^ 0x01]))
            r1 = _run_win_exe_in_empty_dir(nc1, ["--prompt", "ZCC compil", "--steps", "4"], timeout_s=30)
            self.assertNotEqual(r1.returncode, 0)
            self.assertIn("TAMPER_DETECTED", r1.stdout + r1.stderr)
            self.assertNotIn("GPU_LLM_OK", r1.stdout + r1.stderr)

            # T17-NC-2: Corrupt ZCCGGUF1 footer magic
            nc2 = os.path.join(tdir, "tiny_nc2_magic.exe")
            shutil.copyfile(ov_exe, nc2)
            with open(nc2, "r+b") as f:
                f.seek(-64, os.SEEK_END)
                f.write(b"BAD")
            r2 = _run_win_exe_in_empty_dir(nc2, ["--prompt", "ZCC compil", "--steps", "4"], timeout_s=30)
            self.assertNotEqual(r2.returncode, 0)
            self.assertIn("TAMPER_DETECTED", r2.stdout + r2.stderr)

            # T17-NC-3: Truncate 1 byte from EOF
            nc3 = os.path.join(tdir, "tiny_nc3_trunc.exe")
            shutil.copyfile(ov_exe, nc3)
            with open(nc3, "r+b") as f:
                f.truncate(os.path.getsize(nc3) - 1)
            r3 = _run_win_exe_in_empty_dir(nc3, ["--prompt", "ZCC compil", "--steps", "4"], timeout_s=30)
            self.assertNotEqual(r3.returncode, 0)
            self.assertIn("TAMPER_DETECTED", r3.stdout + r3.stderr)

    def test_03_real_1_5b_pe_overlay_empty_dir_and_tamper_nc(self) -> None:
        if not os.path.isfile(REAL_STANDALONE_EXE):
            self.skipTest(f"Missing {REAL_STANDALONE_EXE}")
        r = _run_win_exe_in_empty_dir(
            REAL_STANDALONE_EXE,
            ["--prompt", "What is the capital of France?", "--chat", "--steps", "32", "--temp", "0", "--seed", "42"],
            timeout_s=120,
        )
        combined = r.stdout + "\n" + r.stderr
        self.assertEqual(r.returncode, 0, combined)
        self.assertIn("GPU_LLM_SEAL verified=1 mode=pe_overlay", combined)
        self.assertIn("mode=aot_overlay", combined)
        self.assertIn("Paris", combined)
        self.assertIn("GPU_LLM_OK", combined)

        # Negative control: flip 1 bit inside deep Q4_K weights (~500 MB in)
        tamper_copy = os.path.join(BUILD_DIR, "_tmp_t17_1_5b_tamper.exe")
        try:
            shutil.copyfile(REAL_STANDALONE_EXE, tamper_copy)
            sz = os.path.getsize(tamper_copy)
            with open(tamper_copy, "r+b") as f:
                f.seek(sz // 2)
                b = f.read(1)
                f.seek(sz // 2)
                f.write(bytes([b[0] ^ 0x80]))
            rt = _run_win_exe_in_empty_dir(
                tamper_copy,
                ["--prompt", "What is the capital of France?", "--chat", "--steps", "8", "--temp", "0"],
                timeout_s=120,
            )
            tc = rt.stdout + "\n" + rt.stderr
            self.assertNotEqual(rt.returncode, 0)
            self.assertIn("TAMPER_DETECTED", tc)
            self.assertNotIn("GPU_LLM_OK", tc)
        finally:
            if os.path.exists(tamper_copy):
                os.remove(tamper_copy)

    def test_04_dll_c_host_multi_turn_and_reset(self) -> None:
        res_tiny = run_win_cmd(
            DEMO_OUT,
            [
                "--dll", to_rel_win(DLL_OUT),
                "--model", to_rel_win(TINY_GGUF_F16),
                "--prompt", "ZCC compil",
                "--prompt2", "Blackwell ",
                "--steps", "16",
                "--reset-between",
            ],
            timeout_s=120,
        )
        out_t = res_tiny.stdout + "\n" + res_tiny.stderr
        self.assertEqual(res_tiny.returncode, 0, out_t)
        self.assertIn("DLL_DEMO_RESET pos=0", out_t)
        self.assertIn("es C99 directly", out_t)
        self.assertIn("sm_120 runs live", out_t)
        self.assertIn("DLL_DEMO_OK", out_t)

        ptx_1_5b = _find_real_1_5b_ptx()
        if os.path.isfile(REAL_1_5B_GGUF) and ptx_1_5b:
            res_real = run_win_cmd(
                DEMO_OUT,
                [
                    "--dll", _to_win_abs(DLL_OUT),
                    "--model", _to_win_abs(REAL_1_5B_GGUF),
                    "--aot-ptx", _to_win_abs(ptx_1_5b),
                    "--prompt", "What is the capital of France?",
                    "--prompt2", "What is 2 + 2? Answer with just the number.",
                    "--steps", "24",
                    "--kv-type", "1",
                    "--ctx", "512",
                    "--chat",
                    "--reset-between",
                ],
                timeout_s=120,
            )
            out_r = res_real.stdout + "\n" + res_real.stderr
            self.assertEqual(res_real.returncode, 0, out_r)
            self.assertIn("DLL_DEMO_RESET pos=0", out_r)
            self.assertIn("Paris", out_r)
            self.assertIn("4", out_r)
            self.assertIn("DLL_DEMO_OK", out_r)

    def test_05_dll_csharp_pinvoke_live_streaming(self) -> None:
        ptx_1_5b = _find_real_1_5b_ptx()
        if os.path.isfile(REAL_1_5B_GGUF) and ptx_1_5b:
            args = [
                "-DllPath", _to_win_abs(DLL_OUT),
                "-ModelPath", _to_win_abs(REAL_1_5B_GGUF),
                "-AotPtxPath", _to_win_abs(ptx_1_5b),
                "-Prompt", "What is the capital of France?",
                "-MaxSteps", "24",
                "-KvType", "1",
                "-CtxLen", "512",
                "-Chat",
                "-ResetTest",
            ]
            expected_substr = "Paris"
        else:
            args = [
                "-DllPath", _to_win_abs(DLL_OUT),
                "-ModelPath", _to_win_abs(TINY_GGUF_F16),
                "-Prompt", "ZCC compil",
                "-MaxSteps", "12",
                "-ResetTest",
            ]
            expected_substr = "es C99"
        res = _run_powershell_file(PS1_CSHARP, args, timeout_s=120)
        comb = res.stdout + "\n" + res.stderr
        self.assertEqual(res.returncode, 0, comb)
        self.assertIn("CSHARP_PINVOKE_STATS", comb)
        self.assertIn(expected_substr, comb)
        self.assertIn("CSHARP_PINVOKE_RESET_OK", comb)
        self.assertIn("CSHARP_PINVOKE_OK", comb)

    def test_06_dll_ef0_negative_controls(self) -> None:
        res = run_win_cmd(
            DEMO_OUT,
            [
                "--dll", to_rel_win(DLL_OUT),
                "--model", to_rel_win(TINY_GGUF_F16),
                "--aot-ptx", to_rel_win(os.path.join(BUILD_DIR, "zcc_tiny_llama_f16_sm120.ptx")),
                "--prompt", "ZCC compil",
                "--steps", "8",
                "--test-neg",
            ],
            timeout_s=120,
        )
        comb = res.stdout + "\n" + res.stderr
        self.assertEqual(res.returncode, 0, comb)
        self.assertIn("NEG_CONTROL_1 missing_model", comb)
        self.assertIn("NEG_CONTROL_2 bad_kv", comb)
        self.assertIn("NEG_CONTROL_3 null_handle rc=-1", comb)
        self.assertIn("NEG_CONTROL_4 bad_ptx", comb)
        self.assertIn("DLL_DEMO_OK", comb)


if __name__ == "__main__":
    unittest.main()
