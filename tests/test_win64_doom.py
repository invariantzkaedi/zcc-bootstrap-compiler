#!/usr/bin/env python3
"""
Native Zero-Toolchain Win64 GUI + Headless Verification Suite for Upstream DOOM 1.10
====================================================================================
Verifies standalone native Windows PE32+ DOOM (`build_win64/doom.exe`) compiled with
zero MSVC/MinGW/binutils by BOTH host `./zcc` and native Windows sovereign
`build_win64/zcc_sov_stage3.exe`:
  1. Multi-DLL PE32+ structural audit (.pdata, .reloc, StackReserve>=16MB) + exact
     System32 DLL export resolution across `msvcrt.dll`, `KERNEL32.dll`, `USER32.dll`,
     and `GDI32.dll` + unmodified upstream `doom/` verification.
  2. Byte-for-byte convergence between `./zcc` and `zcc_sov_stage3.exe` via SHA-256 + `fc /b`.
  3. Headless Title Screen (`--headless --frames 12`) & E1M1 3D level rendering
     (`--headless --warp 1 1 --frames 15`) with valid 320x200 P6 PPM output & diversity.
  4. Deterministic input response (`--keys-mask 1` forward vs `--keys-mask 0` idle)
     proving player position `(px, py)` advances and rendered PPM bytes diverge.
  5. Cross-architecture parity check vs `build_wasm/doom.wasm` (`--warp 1 1 --frames 15`)
     confirming matching `gamestate`, `gametic`, and 320x200 PPM framebuffer pixels.
  6. Real interactive Win32 GUI smoke check (`CreateWindowExA` + `StretchDIBits` GDI
     framebuffer blit via `--auto-close-frames 8 --warp 1 1`).
  7. ZCC-EF-0 negative controls (corrupted PE magic, bogus Win32 DLL/export injected
     into IAT, 1-byte PE mutation, missing `doom1.wad`).
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
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from build_doom_win64 import audit_pe32plus_win32_dlls  # noqa: E402
from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    run_fc_b,
    sha256_file,
    to_rel_win,
)
from test_wasm_doom import (  # noqa: E402
    find_node_bin,
    run_node_wasi,
    validate_ppm,
)

DOOM_EXE = os.path.join(BUILD_DIR, "doom.exe")
DOOM_HOST_EXE = os.path.join(BUILD_DIR, "doom_host_zcc.exe")
DOOM_WASM = os.path.join(REPO_ROOT, "build_wasm", "doom.wasm")
DOOM_WAD = os.path.join(REPO_ROOT, "doom1.wad")


def parse_kv_line(text: str, marker: str) -> dict[str, str]:
    for line in text.replace("\r\n", "\n").splitlines():
        if marker in line:
            vals = {}
            for tok in line.split():
                if "=" in tok:
                    k, v = tok.split("=", 1)
                    vals[k] = v
            return vals
    raise AssertionError(f"Missing marker {marker!r} in output:\n{text}")


class TestWin64Doom(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for req in (DOOM_EXE, DOOM_HOST_EXE, DOOM_WAD):
            if not os.path.isfile(req):
                raise RuntimeError(f"Missing {req}; run build_doom_win64.py and ensure doom1.wad exists.")
        cls.tmp_dir = tempfile.mkdtemp(prefix="_test_doom_", dir=BUILD_DIR)
        cls.empty_dir = os.path.join(cls.tmp_dir, "empty_no_wad")
        os.makedirs(cls.empty_dir, exist_ok=True)
        cls.node_bin = find_node_bin() if os.path.isfile(DOOM_WASM) else None

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp_dir, ignore_errors=True)

    def test_01_multi_dll_pe32plus_audit_and_clean_upstream(self):
        audit = audit_pe32plus_win32_dlls(DOOM_EXE)
        self.assertGreater(audit["bytes"], 400000)
        for dll in ("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll"):
            self.assertIn(dll, audit["dlls"])
            self.assertGreater(len(audit["dlls"][dll]), 0)
        self.assertIn("CreateWindowExA", audit["dlls"]["USER32.dll"])
        self.assertIn("StretchDIBits", audit["dlls"]["GDI32.dll"])

        gd = subprocess.run(["git", "diff", "--name-only", "doom/"], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        self.assertEqual(gd.stdout.strip(), "", f"Upstream doom/ files were modified: {gd.stdout}")

    def test_02_sovereign_cross_vs_native_byte_identity(self):
        self.assertEqual(sha256_file(DOOM_HOST_EXE), sha256_file(DOOM_EXE))
        fc = run_fc_b(DOOM_HOST_EXE, DOOM_EXE)
        self.assertEqual(fc.returncode, 0, f"fc /b failed for doom.exe: {fc.stdout}\n{fc.stderr}")
        self.assertIn("no differences encountered", fc.stdout.lower())

    def test_03_headless_title_and_e1m1_plus_key_movement(self):
        title_ppm = os.path.join(self.tmp_dir, "title.ppm")
        r_title = subprocess.run(
            [DOOM_EXE, "--headless", "--frames", "12", "--dump-ppm", to_rel_win(title_ppm)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(r_title.returncode, 0, f"Title failed:\n{r_title.stdout}\n{r_title.stderr}")
        kv_t = parse_kv_line(r_title.stdout, "DOOM_WIN64_OK")
        self.assertEqual(kv_t["gamestate"], "3")
        self.assertGreaterEqual(int(kv_t["unique_colors"]), 32)
        title_sha = validate_ppm(title_ppm)

        idle_ppm = os.path.join(self.tmp_dir, "e1m1_idle.ppm")
        r_idle = subprocess.run(
            [DOOM_EXE, "--headless", "--warp", "1", "1", "--frames", "15", "--dump-ppm", to_rel_win(idle_ppm)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(r_idle.returncode, 0, f"E1M1 idle failed:\n{r_idle.stdout}\n{r_idle.stderr}")
        kv_i = parse_kv_line(r_idle.stdout, "DOOM_WIN64_OK")
        self.assertEqual(kv_i["gamestate"], "0")
        self.assertEqual(kv_i["gametic"], "15")
        self.assertGreaterEqual(int(kv_i["frames"]), 15)
        self.assertGreaterEqual(int(kv_i["unique_colors"]), 20)
        idle_sha = validate_ppm(idle_ppm)
        self.assertNotEqual(idle_sha, title_sha)

        move_ppm = os.path.join(self.tmp_dir, "e1m1_move.ppm")
        r_move = subprocess.run(
            [DOOM_EXE, "--headless", "--warp", "1", "1", "--frames", "15", "--keys-mask", "1", "--dump-ppm", to_rel_win(move_ppm)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(r_move.returncode, 0, f"E1M1 move failed:\n{r_move.stdout}\n{r_move.stderr}")
        kv_m = parse_kv_line(r_move.stdout, "DOOM_WIN64_OK")
        self.assertEqual(kv_m["gamestate"], "0")
        move_sha = validate_ppm(move_ppm)
        self.assertTrue(kv_m["px"] != kv_i["px"] or kv_m["py"] != kv_i["py"], f"Player did not move: idle={kv_i} move={kv_m}")
        self.assertNotEqual(move_sha, idle_sha, "Forward key input did not change rendered E1M1 frame")

    def test_04_cross_arch_parity_vs_doom_wasm(self):
        if not self.node_bin or not os.path.isfile(DOOM_WASM):
            self.skipTest("build_wasm/doom.wasm not present")
        win_ppm = os.path.join(self.tmp_dir, "parity_win64.ppm")
        r_win = subprocess.run(
            [DOOM_EXE, "--headless", "--warp", "1", "1", "--frames", "45", "--dump-ppm", to_rel_win(win_ppm)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(r_win.returncode, 0, r_win.stderr)
        kv_win = parse_kv_line(r_win.stdout, "DOOM_WIN64_OK")
        validate_ppm(win_ppm)

        with tempfile.TemporaryDirectory() as wdir:
            os.symlink(DOOM_WAD, os.path.join(wdir, "doom1.wad"))
            wasm_ppm = os.path.join(wdir, "parity_wasm.ppm")
            r_wasm = run_node_wasi(
                self.node_bin,
                DOOM_WASM,
                ["doom.wasm", "--warp", "1", "1", "--frames", "45", "--dump-ppm", "parity_wasm.ppm"],
                work_dir=wdir,
            )
            self.assertEqual(r_wasm.returncode, 0, r_wasm.stdout + "\n" + r_wasm.stderr)
            kv_wasm = parse_kv_line(r_wasm.stdout, "DOOM_WASM_OK")
            validate_ppm(wasm_ppm)
            with open(wasm_ppm, "rb") as fa:
                wasm_px = fa.read()[15:]

        self.assertEqual(kv_win["gamestate"], kv_wasm["gamestate"])
        self.assertEqual(kv_win["gametic"], kv_wasm["gametic"])
        self.assertEqual(kv_win["unique_colors"], kv_wasm["unique_colors"])
        with open(win_ppm, "rb") as fw:
            win_px = fw.read()[15:]
        self.assertEqual(len(win_px), 320 * 200 * 3)
        self.assertEqual(len(wasm_px), 320 * 200 * 3)
        matching = sum(
            1 for off in range(0, len(win_px), 3) if win_px[off : off + 3] == wasm_px[off : off + 3]
        )
        self.assertGreaterEqual(matching, 63900, f"Low pixel parity vs doom.wasm: {matching}/64000")

    def test_05_real_win32_gui_hwnd_and_stretchdibits_smoke(self):
        gui_ppm = os.path.join(self.tmp_dir, "gui_e1m1.ppm")
        r_gui = subprocess.run(
            [DOOM_EXE, "--auto-close-frames", "8", "--warp", "1", "1", "--dump-ppm", to_rel_win(gui_ppm)],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(r_gui.returncode, 0, f"Win32 GUI mode failed:\n{r_gui.stdout}\n{r_gui.stderr}")
        kv_g = parse_kv_line(r_gui.stdout, "DOOM_WIN64_GUI_OK")
        self.assertEqual(kv_g["hwnd_ok"], "1")
        self.assertEqual(kv_g["blit_ok"], "1")
        self.assertGreaterEqual(int(kv_g["frames"]), 8)
        self.assertEqual(kv_g["gamestate"], "0")
        self.assertGreaterEqual(int(kv_g["unique_colors"]), 20)
        validate_ppm(gui_ppm)

    def test_06_ef0_negative_controls(self):
        # NC-1: Corrupted PE signature rejected
        bad_hdr = os.path.join(self.tmp_dir, "bad_doom_hdr.exe")
        with open(DOOM_EXE, "rb") as f:
            raw = bytearray(f.read())
        pe_off = int.from_bytes(raw[0x3C:0x40], "little")
        raw[pe_off : pe_off + 4] = b"BAD!"
        with open(bad_hdr, "wb") as f:
            f.write(raw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_win32_dlls(bad_hdr)

        # NC-2: Injected unknown DLL name rejected
        bad_dll = os.path.join(self.tmp_dir, "bad_doom_dll.exe")
        with open(DOOM_EXE, "rb") as f:
            raw = bytearray(f.read())
        idx = raw.find(b"USER32.dll\x00")
        self.assertGreater(idx, 0)
        raw[idx : idx + 10] = b"FAKE32.dll"
        with open(bad_dll, "wb") as f:
            f.write(raw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_win32_dlls(bad_dll)

        # NC-3: Bogus GDI32 export symbol rejected against System32\gdi32.dll
        bad_sym = os.path.join(self.tmp_dir, "bad_doom_sym.exe")
        with open(DOOM_EXE, "rb") as f:
            raw = bytearray(f.read())
        sidx = raw.find(b"StretchDIBits\x00")
        self.assertGreater(sidx, 0)
        raw[sidx : sidx + 13] = b"FakeGdiExport"
        with open(bad_sym, "wb") as f:
            f.write(raw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_win32_dlls(bad_sym)

        # NC-4: 1-byte PE mutation caught by SHA-256 and fc /b
        mut = os.path.join(self.tmp_dir, "mut_doom.exe")
        with open(DOOM_EXE, "rb") as f:
            mraw = bytearray(f.read())
        mraw[len(mraw) // 2] ^= 0xFF
        with open(mut, "wb") as f:
            f.write(mraw)
        self.assertNotEqual(sha256_file(mut), sha256_file(DOOM_EXE))
        self.assertNotEqual(run_fc_b(DOOM_EXE, mut).returncode, 0)

        # NC-5: Missing doom1.wad in empty directory fails cleanly with nonzero rc
        r_nowad = subprocess.run(
            [DOOM_EXE, "--headless", "--frames", "2"],
            cwd=self.empty_dir,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertNotEqual(r_nowad.returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
