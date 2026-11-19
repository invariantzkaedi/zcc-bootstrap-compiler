#!/usr/bin/env python3
"""
Standalone Upstream DOOM 1.10 WASI Dual-Runtime Test Suite
===========================================================
Independently verifies `build_wasm/doom.wasm` against on-disk `doom1.wad`:
  1. V8 `WebAssembly.validate()` + zero `env.*` imports + framebuffer ABI exports
     + unmodified upstream `doom/` source verification
  2. Title screen & attract loop execution on BOTH `node:wasi` CLI (`_start`)
     and pure-JS `tools/browser_wasi_shim.js` (`fb_init`/`fb_tick`/`fb_ptr`)
  3. E1M1 3D level rendering + differential keyboard input response (`fb_tick`
     + `fb_key`) verifying pixel SHA-256 and player `(x, y, angle)` divergence
  4. EF-0 negative controls (missing `doom1.wad`, corrupt WASM, truncated IWAD)
"""

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
DOOM_WASM = os.path.join(BUILD_WASM, "doom.wasm")
DOOM_WAD = os.path.join(REPO_ROOT, "doom1.wad")
BUILD_DOOM_PY = os.path.join(REPO_ROOT, "tools", "build_doom_wasm.py")
SHIM_JS = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")


def find_node_bin() -> str:
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    found = shutil.which("node")
    if found:
        return found
    raise RuntimeError("node binary not found at /home/ZKAEDI/.local/bin/node or on PATH")


def inspect_wasm_v8(node_bin: str, wasm_path: str) -> dict:
    js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
let compiled = false;
let imports = [];
let exportsList = [];
let err = null;
if (valid) {
  try {
    const mod = new WebAssembly.Module(b);
    compiled = true;
    imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
    exportsList = WebAssembly.Module.exports(mod).map(e => `${e.name}:${e.kind}`);
  } catch (e) {
    err = String(e);
  }
}
console.log(JSON.stringify({ valid, compiled, bytes: b.length, imports, exportsList, err }));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    return json.loads(res.stdout.strip().splitlines()[-1])


def run_node_wasi(node_bin: str, wasm_path: str, args, work_dir: str, timeout: int = 180) -> subprocess.CompletedProcess:
    js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const w = new WASI({
  version: 'preview1',
  args: cfg.args,
  env: {},
  preopens: { '.': '.' },
  returnOnExit: true,
});
const imp = w.getImportObject();
const wasi = imp.wasi_snapshot_preview1;
for (const k of Object.keys(wasi)) {
  const orig = wasi[k];
  wasi[k] = (...a) => orig.apply(wasi, a);
}
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
  const mod = await WebAssembly.compile(bytes);
  const inst = await WebAssembly.instantiate(mod, imp);
  const rc = w.start(inst);
  process.exit(rc | 0);
})().catch((e) => {
  console.error(e);
  process.exit(99);
});
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": os.path.abspath(wasm_path), "args": args}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run(
            [node_bin, "-e", js, cfg_path],
            cwd=work_dir,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def run_shim_doom(node_bin: str, wasm_path: str, wad_path: str, ep: int, map_num: int, tic_masks, direct_keys=None) -> dict:
    js = r"""
const fs = require('fs');
const crypto = require('crypto');
const { BrowserWasiShim } = require(process.argv[1]);
const cfg = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
(async () => {
  const wasmBytes = fs.readFileSync(cfg.wasm);
  const files = {};
  if (cfg.wad) {
    files['doom1.wad'] = fs.readFileSync(cfg.wad);
  }
  const shim = new BrowserWasiShim({
    args: ['doom.wasm'],
    files,
    preopenDir: '.',
  });
  const { instance } = await shim.instantiate(wasmBytes);
  const ex = instance.exports;
  ex.fb_init(cfg.ep | 0, cfg.map | 0);
  if (Array.isArray(cfg.directKeys)) {
    for (const ev of cfg.directKeys) {
      ex.fb_key(ev.key | 0, ev.down ? 1 : 0);
    }
  }
  for (const mask of cfg.ticMasks) {
    ex.fb_tick(28, mask | 0, 0, 0);
  }
  const w = ex.fb_width() | 0;
  const h = ex.fb_height() | 0;
  const ptr = ex.fb_ptr() >>> 0;
  const u8 = new Uint8Array(ex.memory.buffer);
  const fb = u8.slice(ptr, ptr + w * h * 4);
  const uniq = new Set();
  let allOpaque = true;
  for (let i = 0; i < w * h; i++) {
    const o = i * 4;
    if (fb[o + 3] !== 255) allOpaque = false;
    uniq.add((fb[o] << 16) | (fb[o + 1] << 8) | fb[o + 2]);
  }
  const sha256 = crypto.createHash('sha256').update(fb).digest('hex');
  console.log('__DOOM_RES__' + JSON.stringify({
    width: w,
    height: h,
    gamestate: ex.fb_gamestate() | 0,
    gametic: ex.fb_gametic() | 0,
    playerX: ex.fb_player_x() | 0,
    playerY: ex.fb_player_y() | 0,
    playerAngle: ex.fb_player_angle() >>> 0,
    uniqueColors: uniq.size,
    allOpaque,
    sha256,
    stdout: shim.getStdout(),
    stderr: shim.getStderr(),
  }));
})().catch((e) => {
  console.error(e);
  process.exit(98);
});
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(
            {
                "wasm": os.path.abspath(wasm_path),
                "wad": os.path.abspath(wad_path) if wad_path else None,
                "ep": ep,
                "map": map_num,
                "ticMasks": list(tic_masks),
                "directKeys": list(direct_keys or []),
            },
            tf,
        )
        cfg_path = tf.name
    try:
        res = subprocess.run([node_bin, "-e", js, SHIM_JS, cfg_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        for line in reversed(res.stdout.splitlines()):
            if line.startswith("__DOOM_RES__"):
                return json.loads(line[len("__DOOM_RES__"):])
        raise RuntimeError(f"Missing __DOOM_RES__ marker: {res.stdout}\n{res.stderr}")
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def validate_ppm(ppm_path: str) -> str:
    with open(ppm_path, "rb") as f:
        raw = f.read()
    header = b"P6\n320 200\n255\n"
    if not raw.startswith(header):
        raise AssertionError(f"Invalid PPM header in {ppm_path}: {raw[:32]!r}")
    if len(raw) != len(header) + 320 * 200 * 3:
        raise AssertionError(f"Unexpected PPM length {len(raw)} != {len(header) + 320 * 200 * 3}")
    return hashlib.sha256(raw).hexdigest()


class TestWasmDoom(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node_bin = find_node_bin()
        if not os.path.isfile(DOOM_WASM):
            subprocess.run(["python3", BUILD_DOOM_PY], cwd=REPO_ROOT, check=True)
        if not os.path.isfile(DOOM_WAD):
            raise FileNotFoundError(f"Missing IWAD at {DOOM_WAD}")

    def test_01_v8_validation_zero_env_and_exports(self):
        info = inspect_wasm_v8(self.node_bin, DOOM_WASM)
        self.assertTrue(info["valid"], info)
        self.assertTrue(info["compiled"], info)
        self.assertGreater(info["bytes"], 200000)

        non_wasi = [imp for imp in info["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
        self.assertEqual(non_wasi, [], f"Forbidden non-WASI imports: {non_wasi}")

        exp_names = {e.split(":")[0] for e in info["exportsList"]}
        required = {
            "memory",
            "_start",
            "fb_width",
            "fb_height",
            "fb_ptr",
            "fb_key",
            "fb_init",
            "fb_tick",
            "fb_gamestate",
            "fb_gametic",
            "fb_player_x",
            "fb_player_y",
            "fb_player_angle",
        }
        self.assertEqual(sorted(required - exp_names), [], f"Missing exports: {sorted(required - exp_names)}")

        gd = subprocess.run(["git", "diff", "--name-only", "doom/"], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        self.assertEqual(gd.stdout.strip(), "", f"Upstream doom/ files were modified: {gd.stdout}")

    def test_02_doom_title_and_demo_dual_host(self):
        with tempfile.TemporaryDirectory() as tmpd:
            os.symlink(DOOM_WAD, os.path.join(tmpd, "doom1.wad"))
            res = run_node_wasi(
                self.node_bin,
                DOOM_WASM,
                ["doom.wasm", "--frames", "25", "--dump-ppm", "title.ppm"],
                work_dir=tmpd,
            )
            self.assertEqual(res.returncode, 0, res.stdout + "\n" + res.stderr)
            self.assertIn("DOOM_WASM_OK", res.stdout)
            self.assertIn("gamestate=3", res.stdout)
            m_uniq = None
            for tok in res.stdout.split():
                if tok.startswith("unique_colors="):
                    m_uniq = int(tok.split("=")[1])
            self.assertIsNotNone(m_uniq)
            self.assertGreaterEqual(m_uniq, 32)
            title_ppm_sha = validate_ppm(os.path.join(tmpd, "title.ppm"))
            self.assertEqual(len(title_ppm_sha), 64)

        shim_res = run_shim_doom(self.node_bin, DOOM_WASM, DOOM_WAD, 0, 0, [0] * 25)
        self.assertEqual(shim_res["width"], 320)
        self.assertEqual(shim_res["height"], 200)
        self.assertEqual(shim_res["gamestate"], 3)
        self.assertGreaterEqual(shim_res["uniqueColors"], 32)
        self.assertTrue(shim_res["allOpaque"])
        print(
            f"[DOOM TITLE PASS] {shim_res['width']}x{shim_res['height']} gs={shim_res['gamestate']} "
            f"tic={shim_res['gametic']} uniq={shim_res['uniqueColors']} sha={shim_res['sha256'][:16]}",
            flush=True,
        )

    def test_03_doom_e1m1_3d_render_and_key_input_response(self):
        with tempfile.TemporaryDirectory() as tmpd:
            os.symlink(DOOM_WAD, os.path.join(tmpd, "doom1.wad"))
            res_t = run_node_wasi(
                self.node_bin,
                DOOM_WASM,
                ["doom.wasm", "--frames", "25", "--dump-ppm", "title.ppm"],
                work_dir=tmpd,
            )
            self.assertEqual(res_t.returncode, 0, res_t.stderr)
            title_sha = validate_ppm(os.path.join(tmpd, "title.ppm"))

            res_e = run_node_wasi(
                self.node_bin,
                DOOM_WASM,
                ["doom.wasm", "--warp", "1", "1", "--frames", "25", "--dump-ppm", "e1m1.ppm"],
                work_dir=tmpd,
            )
            self.assertEqual(res_e.returncode, 0, res_e.stdout + "\n" + res_e.stderr)
            self.assertIn("DOOM_WASM_OK", res_e.stdout)
            self.assertIn("gamestate=0", res_e.stdout)
            e1m1_sha = validate_ppm(os.path.join(tmpd, "e1m1.ppm"))
            self.assertNotEqual(e1m1_sha, title_sha)

        idle = run_shim_doom(self.node_bin, DOOM_WASM, DOOM_WAD, 1, 1, [0] * 25)
        self.assertEqual(idle["gamestate"], 0)
        self.assertGreaterEqual(idle["uniqueColors"], 32)
        self.assertTrue(idle["allOpaque"])

        # KEY_UPARROW (bit 0 = 1) | KEY_RIGHTARROW (bit 3 = 8) | KEY_RCTRL (bit 4 = 16) = 25
        # plus direct fb_key toggle to verify fb_key() path explicitly
        moved = run_shim_doom(
            self.node_bin,
            DOOM_WASM,
            DOOM_WAD,
            1,
            1,
            [0] * 5 + [25] * 15 + [0] * 5,
            direct_keys=[{"key": 0xAD, "down": 1}, {"key": 0xAD, "down": 0}],
        )
        self.assertEqual(moved["gamestate"], 0)
        self.assertGreaterEqual(moved["uniqueColors"], 32)
        self.assertTrue(moved["allOpaque"])
        self.assertNotEqual(moved["sha256"], idle["sha256"], "Framebuffer pixels did not respond to input")
        self.assertNotEqual(
            (moved["playerX"], moved["playerY"], moved["playerAngle"]),
            (idle["playerX"], idle["playerY"], idle["playerAngle"]),
            "Player transform did not respond to input",
        )
        print(
            f"[DOOM E1M1 PASS] idle=({idle['playerX']},{idle['playerY']},{idle['playerAngle']},"
            f"uniq={idle['uniqueColors']},sha={idle['sha256'][:12]}) "
            f"moved=({moved['playerX']},{moved['playerY']},{moved['playerAngle']},"
            f"uniq={moved['uniqueColors']},sha={moved['sha256'][:12]})",
            flush=True,
        )

    def test_04_ef0_negative_controls(self):
        # NC-1: Running without doom1.wad must fail non-zero with W_InitFiles error
        with tempfile.TemporaryDirectory() as empty_dir:
            res_missing = run_node_wasi(
                self.node_bin,
                DOOM_WASM,
                ["doom.wasm", "--frames", "5"],
                work_dir=empty_dir,
            )
            self.assertNotEqual(res_missing.returncode, 0)
            self.assertIn("W_InitFiles: no files found", res_missing.stderr)
            self.assertNotIn("DOOM_WASM_OK", res_missing.stdout)

        # NC-2: Corrupt/truncated doom.wasm must fail V8 validation
        with tempfile.TemporaryDirectory() as bad_dir:
            corrupt_wasm = os.path.join(bad_dir, "bad_doom.wasm")
            with open(DOOM_WASM, "rb") as f:
                data = f.read(512)
            with open(corrupt_wasm, "wb") as f:
                f.write(data)
            info_bad = inspect_wasm_v8(self.node_bin, corrupt_wasm)
            self.assertFalse(info_bad["valid"])
            self.assertFalse(info_bad["compiled"])

        # NC-3: Truncated 16-byte doom1.wad must fail non-zero
        with tempfile.TemporaryDirectory() as bad_wad_dir:
            with open(os.path.join(bad_wad_dir, "doom1.wad"), "wb") as f:
                f.write(b"IWAD\x05\x00\x00\x00\x99\x99\x00\x00\x00\x00\x00\x00")
            res_bad_wad = run_node_wasi(
                self.node_bin,
                DOOM_WASM,
                ["doom.wasm", "--frames", "5"],
                work_dir=bad_wad_dir,
            )
            self.assertNotEqual(res_bad_wad.returncode, 0)
            self.assertNotIn("DOOM_WASM_OK", res_bad_wad.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
