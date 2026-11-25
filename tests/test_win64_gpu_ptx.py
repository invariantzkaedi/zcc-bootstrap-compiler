#!/usr/bin/env python3
"""
Sovereign Zero-Toolchain Win64 + Live NVIDIA RTX 5070 PTX GPU Verification Suite
==================================================================================
Tests:
  1. Triple-compiler PTX byte-identity: `./zcc`, `build_win64/zcc_sov_stage3.exe`,
     and `build_wasm/zcc.wasm` (Node 22 WASI) emit byte-identical PTX ISA 8.7
     (`sm_120`) from `src/win64_gpu_kernels.c`, containing all 5 kernels and
     Blackwell hardware instructions (`lop3.b32`, `shf.r.wrap.b32`, `popc.b32`,
     `clz.b32`, `mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32`).
  2. Multi-DLL PE32+ structural & System32 export audit (`msvcrt.dll`, `nvcuda.dll`,
     `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`) + `fc /b` / SHA-256 parity vs
     `*_host_zcc.exe` + unmodified upstream & `part0..4` check.
  3. Live RTX 5070 Compute + Tensor Core differential verification (`cuModuleLoadDataEx`,
     1M SAXPY FMA, 256x256 FP32 MatMul, 65536 Blackwell bitops `0` mismatches,
     SM 12.0 Tensor Core `m16n8k16` MMA, and 3 shader modes).
  4. Live RTX 5070 headless shader rendering across all 3 modes (valid P6 PPM,
     diversity, pairwise distinct FNV hashes, and temporal frame divergence).
  5. Real Win32 GUI `HWND` + `StretchDIBits` GPU blit smoke check (`hwnd_ok=1`, `blit_ok=1`).
  6. ZCC-EF-0 negative controls (`NC-1..6`).
"""

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS_DIR = os.path.join(REPO_ROOT, "tools")
TESTS_DIR = os.path.join(REPO_ROOT, "tests")
for p in (TOOLS_DIR, TESTS_DIR):
    if p not in sys.path:
        sys.path.insert(0, p)

from build_gpu_win64 import (  # noqa: E402
    COMPUTE_HOST_OUT,
    COMPUTE_OUT,
    KERNEL_SRC,
    LIVE_HOST_OUT,
    LIVE_OUT,
    PTX_HOST_OUT,
    PTX_WIN64_OUT,
    audit_pe32plus_multi_dll,
)
from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)
from test_wasm_wasi_selfhost import (  # noqa: E402
    ZCC_WASM,
    find_node_bin,
    run_node_wasi,
)


def parse_kv_line(text: str, marker: str) -> dict[str, str]:
    for line in text.replace("\r\n", "\n").splitlines():
        if marker in line:
            vals = {}
            # Strip quoted device name containing spaces so token splitting stays clean
            cleaned = re.sub(r'name="[^"]*"|gpu="[^"]*"', "", line)
            for tok in cleaned.split():
                if "=" in tok:
                    k, v = tok.split("=", 1)
                    vals[k] = v
            return vals
    raise AssertionError(f"Missing marker {marker!r} in output:\n{text}")


def validate_ppm_dims(ppm_path: str, width: int, height: int) -> str:
    with open(ppm_path, "rb") as f:
        raw = f.read()
    header = f"P6\n{width} {height}\n255\n".encode("ascii")
    if not raw.startswith(header):
        raise AssertionError(f"Invalid PPM header in {ppm_path}: {raw[:32]!r}")
    expected_len = len(header) + width * height * 3
    if len(raw) != expected_len:
        raise AssertionError(f"Unexpected PPM size {len(raw)} != {expected_len}")
    return hashlib.sha256(raw).hexdigest()


class TestWin64GpuPtx(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for req in (
            ZCC_BIN,
            ZCC_WIN64,
            ZCC_WASM,
            PTX_HOST_OUT,
            PTX_WIN64_OUT,
            COMPUTE_OUT,
            COMPUTE_HOST_OUT,
            LIVE_OUT,
            LIVE_HOST_OUT,
        ):
            if not os.path.isfile(req):
                raise RuntimeError(f"Missing required artifact {req}; build sovereign Win64, WASI, and GPU targets first.")
        cls.node_bin = find_node_bin()
        cls.tmp_dir = tempfile.mkdtemp(prefix="_test_gpu_ptx_", dir=BUILD_DIR)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp_dir, ignore_errors=True)

    def test_01_ptx_triple_compiler_byte_identity(self):
        wasm_ptx = os.path.join(self.tmp_dir, "gpu_kernels_wasm.ptx")
        res = run_node_wasi(
            self.node_bin,
            ZCC_WASM,
            ["zcc.wasm", "--target=ptx", "--ptx-arch=sm_120", to_rel_posix(KERNEL_SRC), "-o", to_rel_posix(wasm_ptx)],
        )
        self.assertEqual(res.returncode, 0, f"zcc.wasm PTX compile failed:\n{res.stdout}\n{res.stderr}")
        self.assertTrue(os.path.isfile(wasm_ptx))

        sha_host = sha256_file(PTX_HOST_OUT)
        sha_win64 = sha256_file(PTX_WIN64_OUT)
        sha_wasm = sha256_file(wasm_ptx)
        self.assertEqual(sha_host, sha_win64, "./zcc PTX != zcc_sov_stage3.exe PTX")
        self.assertEqual(sha_host, sha_wasm, "./zcc PTX != zcc.wasm PTX")

        for label, p in (("win64", PTX_WIN64_OUT), ("wasm", wasm_ptx)):
            fc = run_fc_b(PTX_HOST_OUT, p)
            self.assertEqual(fc.returncode, 0, f"fc /b mismatch ({label}): {fc.stdout}")
            self.assertIn("no differences encountered", fc.stdout.lower())

        with open(PTX_WIN64_OUT, "r", encoding="utf-8") as f:
            ptx = f.read()
        for needle in (
            ".version 8.7",
            ".target sm_120",
            ".address_size 64",
            ".visible .entry gpu_saxpy_fma(",
            ".visible .entry gpu_matmul_f32(",
            ".visible .entry gpu_blackwell_bitops(",
            ".visible .entry gpu_tensor_mma_f16(",
            ".visible .entry gpu_render_scene(",
            "lop3.b32",
            "shf.r.wrap.b32",
            "popc.b32",
            "clz.b32",
            "mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32",
        ):
            self.assertIn(needle, ptx, f"Expected PTX construct missing: {needle!r}")

    def test_02_pe32plus_multi_dll_nvcuda_audit_and_sovereign_parity(self):
        audit_c = audit_pe32plus_multi_dll(
            COMPUTE_OUT,
            required_dlls=("msvcrt.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "nvcuda.dll"),
        )
        self.assertIn("cuModuleLoadDataEx", audit_c["dlls"]["nvcuda.dll"])
        self.assertIn("cuLaunchKernel", audit_c["dlls"]["nvcuda.dll"])

        audit_l = audit_pe32plus_multi_dll(
            LIVE_OUT,
            required_dlls=("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll", "nvcuda.dll"),
            allowed_dlls=("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll", "nvcuda.dll"),
        )
        self.assertIn("CreateWindowExA", audit_l["dlls"]["USER32.dll"])
        self.assertIn("StretchDIBits", audit_l["dlls"]["GDI32.dll"])
        self.assertIn("cuLaunchKernel", audit_l["dlls"]["nvcuda.dll"])

        for label, host_p, win_p in (
            ("compute", COMPUTE_HOST_OUT, COMPUTE_OUT),
            ("live", LIVE_HOST_OUT, LIVE_OUT),
        ):
            self.assertEqual(sha256_file(host_p), sha256_file(win_p), f"SHA mismatch on {label}")
            fc = run_fc_b(host_p, win_p)
            self.assertEqual(fc.returncode, 0, f"fc /b failed ({label}): {fc.stdout}")
            self.assertIn("no differences encountered", fc.stdout.lower())

        protected = [
            "part0_pp.c",
            "part1.c",
            "part2.c",
            "part3.c",
            "part4.c",
            "part6_arm.c",
            "part6_riscv.c",
            "part6_wasm.c",
            "doom/",
            "src/sqlite3.c",
        ] + [os.path.join("tests", d) for d in os.listdir(TESTS_DIR) if d.startswith("embedded_")]
        gd = subprocess.run(
            ["git", "diff", "--name-only", "--"] + protected,
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertEqual(gd.stdout.strip(), "", f"Protected files modified:\n{gd.stdout}")

    def test_03_live_rtx5070_compute_and_tensor_core_differential(self):
        res = run_win_cmd(COMPUTE_OUT, [], timeout_s=120)
        self.assertEqual(res.returncode, 0, f"zcc_gpu_compute.exe failed:\n{res.stdout}\n{res.stderr}")
        out = res.stdout

        kv_dev = parse_kv_line(out, "GPU_DEVICE")
        self.assertEqual(kv_dev["cc"], "12.0")

        kv_sax = parse_kv_line(out, "GPU_SAXPY_OK")
        self.assertEqual(kv_sax["n"], "1048576")
        self.assertLess(float(kv_sax["max_err"]), 1e-4)

        kv_mm = parse_kv_line(out, "GPU_MATMUL_OK")
        self.assertEqual(kv_mm["N"], "256")
        self.assertLess(float(kv_mm["max_err"]), 1e-2)

        kv_bit = parse_kv_line(out, "GPU_BITOPS_OK")
        self.assertEqual(kv_bit["n"], "65536")
        self.assertEqual(kv_bit["mismatches"], "0")

        kv_mma = parse_kv_line(out, "GPU_TENSOR_MMA_OK")
        self.assertEqual(kv_mma["shape"], "16x8x16")
        self.assertLess(float(kv_mma["max_err"]), 1e-2)

        kv_sh = parse_kv_line(out, "GPU_SHADERS_OK")
        fnvs = []
        for k in ("m0", "m1", "m2"):
            m = re.fullmatch(r"\((\d+),([0-9a-fA-F]{8})\)", kv_sh[k])
            self.assertIsNotNone(m, f"Malformed shader summary {k}={kv_sh[k]}")
            self.assertGreaterEqual(int(m.group(1)), 32)
            fnvs.append(m.group(2))
        self.assertEqual(len(set(fnvs)), 3, f"Expected 3 distinct shader FNVs, got {fnvs}")
        self.assertIn("GPU_COMPUTE_ALL_OK", out)

    def test_04_live_rtx5070_headless_shaders_and_ppm_divergence(self):
        mode_fnvs = []
        mode_shas = []
        for mode in (0, 1, 2):
            ppm_path = os.path.join(self.tmp_dir, f"shader_m{mode}.ppm")
            res = run_win_cmd(
                LIVE_OUT,
                ["--headless", "--mode", str(mode), "--frames", "5", "--width", "320", "--height", "200", "--dump-ppm", to_rel_win(ppm_path)],
                timeout_s=90,
            )
            self.assertEqual(res.returncode, 0, f"Headless mode {mode} failed:\n{res.stdout}\n{res.stderr}")
            kv = parse_kv_line(res.stdout, "GPU_LIVE_OK")
            self.assertEqual(kv["cc"], "12.0")
            self.assertEqual(kv["mode"], str(mode))
            self.assertEqual(kv["frames"], "5")
            self.assertGreaterEqual(int(kv["unique_colors"]), 32)
            mode_fnvs.append(kv["fnv"])
            mode_shas.append(validate_ppm_dims(ppm_path, 320, 200))

        self.assertEqual(len(set(mode_fnvs)), 3, f"Mode FNVs collided: {mode_fnvs}")
        self.assertEqual(len(set(mode_shas)), 3, f"Mode PPM hashes collided: {mode_shas}")

        ppm_f1 = os.path.join(self.tmp_dir, "m1_f1.ppm")
        ppm_f8 = os.path.join(self.tmp_dir, "m1_f8.ppm")
        r1 = run_win_cmd(
            LIVE_OUT,
            ["--headless", "--mode", "1", "--frames", "1", "--width", "320", "--height", "200", "--dump-ppm", to_rel_win(ppm_f1)],
            timeout_s=60,
        )
        r8 = run_win_cmd(
            LIVE_OUT,
            ["--headless", "--mode", "1", "--frames", "8", "--width", "320", "--height", "200", "--dump-ppm", to_rel_win(ppm_f8)],
            timeout_s=60,
        )
        self.assertEqual(r1.returncode, 0, r1.stderr)
        self.assertEqual(r8.returncode, 0, r8.stderr)
        kv1 = parse_kv_line(r1.stdout, "GPU_LIVE_OK")
        kv8 = parse_kv_line(r8.stdout, "GPU_LIVE_OK")
        self.assertNotEqual(kv1["fnv"], kv8["fnv"], "Temporal frame animation did not advance on GPU")
        self.assertNotEqual(validate_ppm_dims(ppm_f1, 320, 200), validate_ppm_dims(ppm_f8, 320, 200))

    def test_05_real_win32_gui_hwnd_gpu_blit_smoke(self):
        gui_ppm = os.path.join(self.tmp_dir, "gui_m1.ppm")
        res = run_win_cmd(
            LIVE_OUT,
            ["--auto-close-frames", "8", "--mode", "1", "--dump-ppm", to_rel_win(gui_ppm)],
            timeout_s=90,
        )
        self.assertEqual(res.returncode, 0, f"Win32 GPU GUI smoke failed:\n{res.stdout}\n{res.stderr}")
        kv = parse_kv_line(res.stdout, "GPU_LIVE_GUI_OK")
        self.assertEqual(kv["hwnd_ok"], "1")
        self.assertEqual(kv["blit_ok"], "1")
        self.assertGreaterEqual(int(kv["frames"]), 8)
        self.assertGreaterEqual(int(kv["unique_colors"]), 32)
        validate_ppm_dims(gui_ppm, 640, 400)

    def test_06_ef0_negative_controls(self):
        # NC-1: Invalid C rejected cleanly under --target=ptx on both ./zcc and zcc_sov_stage3.exe
        bad_c = os.path.join(self.tmp_dir, "bad_kernel.c")
        bad_ptx_a = os.path.join(self.tmp_dir, "bad_a.ptx")
        bad_ptx_b = os.path.join(self.tmp_dir, "bad_b.ptx")
        with open(bad_c, "w", encoding="utf-8") as f:
            f.write("void broken_kernel(float *x) { x[0] = @@@SYNTAX_ERROR@@@; }\n")
        r_host = subprocess.run(
            [ZCC_BIN, "--target=ptx", "--ptx-arch=sm_120", to_rel_posix(bad_c), "-o", to_rel_posix(bad_ptx_a)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(r_host.returncode, 0, "NC-1 failed: ./zcc accepted invalid C in PTX mode")
        r_win = run_win_cmd(
            ZCC_WIN64,
            ["--target=ptx", "--ptx-arch=sm_120", to_rel_posix(bad_c), "-o", to_rel_posix(bad_ptx_b)],
        )
        self.assertNotEqual(r_win.returncode, 0, "NC-1 failed: zcc_sov_stage3.exe accepted invalid C in PTX mode")

        # NC-2: nvcuda.dll JIT rejects malformed PTX
        r_badptx = run_win_cmd(COMPUTE_OUT, ["--test-bad-ptx"], timeout_s=60)
        self.assertNotEqual(r_badptx.returncode, 0, "NC-2 failed: nvcuda accepted bogus opcode")
        self.assertIn("GPU_BAD_PTX_REJECTED", r_badptx.stdout)

        # NC-3: 1-byte mutation caught by SHA-256 and fc /b on .ptx and .exe
        mut_ptx = os.path.join(self.tmp_dir, "mut.ptx")
        with open(PTX_WIN64_OUT, "rb") as f:
            praw = bytearray(f.read())
        praw[len(praw) // 2] ^= 0x01
        with open(mut_ptx, "wb") as f:
            f.write(praw)
        self.assertNotEqual(sha256_file(mut_ptx), sha256_file(PTX_WIN64_OUT))
        self.assertNotEqual(run_fc_b(PTX_WIN64_OUT, mut_ptx).returncode, 0)

        mut_exe = os.path.join(self.tmp_dir, "mut_compute.exe")
        with open(COMPUTE_OUT, "rb") as f:
            eraw = bytearray(f.read())
        eraw[len(eraw) // 2] ^= 0xFF
        with open(mut_exe, "wb") as f:
            f.write(eraw)
        self.assertNotEqual(sha256_file(mut_exe), sha256_file(COMPUTE_OUT))
        self.assertNotEqual(run_fc_b(COMPUTE_OUT, mut_exe).returncode, 0)

        # NC-4: Bogus nvcuda.dll export symbol rejected against System32\nvcuda.dll
        bad_sym = os.path.join(self.tmp_dir, "bad_nvcuda_sym.exe")
        with open(COMPUTE_OUT, "rb") as f:
            sraw = bytearray(f.read())
        self.assertEqual(sraw.count(b"cuEventRecord\x00"), 1)
        sidx = sraw.find(b"cuEventRecord\x00")
        self.assertGreater(sidx, 0)
        sraw[sidx : sidx + 13] = b"cuFake_Record"
        with open(bad_sym, "wb") as f:
            f.write(sraw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_multi_dll(bad_sym, ("msvcrt.dll", "nvcuda.dll"), ("msvcrt.dll", "nvcuda.dll"))

        # NC-5: part5.c diff stays strictly surgical (< 40 changed lines)
        ns = subprocess.run(
            ["git", "diff", "--numstat", "--", "part5.c"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            check=True,
        )
        parts = ns.stdout.strip().split()
        if parts:
            total_changed = int(parts[0]) + int(parts[1])
            self.assertLess(total_changed, 40, f"part5.c diff too large: {total_changed} lines")

        # NC-6: Unexpected DLL injection rejected on zcc_gpu_live.exe
        bad_dll = os.path.join(self.tmp_dir, "bad_live_dll.exe")
        with open(LIVE_OUT, "rb") as f:
            draw = bytearray(f.read())
        self.assertEqual(draw.count(b"USER32.dll\x00"), 1)
        didx = draw.find(b"USER32.dll\x00")
        self.assertGreater(didx, 0)
        draw[didx : didx + 10] = b"FAKE32.dll"
        with open(bad_dll, "wb") as f:
            f.write(draw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_multi_dll(
                bad_dll,
                ("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll", "nvcuda.dll"),
                ("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll", "nvcuda.dll"),
            )


if __name__ == "__main__":
    unittest.main()
