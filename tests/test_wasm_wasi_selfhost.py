#!/usr/bin/env python3
"""
ZCC Browser & WASI Sovereign Self-Host Test Suite (`make selfhost-wasm`)
========================================================================
Tests:
  1. WASI Preview1 runtime primitives (`fopen`/`fwrite`/`fseek`/`ftell`/`fgets`/
     `fread`/`remove`, `argc`/`argv`, `getenv`, `vsnprintf`/`va_arg`/`va_copy`,
     `sscanf` `%[^(` scanset, struct by-value params & `sret` return, `<=64B` and
     `>64B` struct assignment, pointer `+=`, `goto` via `br_table`) executed in
     BOTH Node 22 `node:wasi` and pure-JS `tools/browser_wasi_shim.js`.
  2. Full 3-stage WASI self-host convergence in Node 22 `node:wasi`:
     `tools/build_wasm_bundle.py` -> `build_wasm/zcc.wasm` -> `zcc2.wasm` -> `zcc3.wasm`,
     asserting `WebAssembly.validate()`, zero `env.*` imports, `cmp` exit 0, and
     `SHA-256(zcc2.wasm) == SHA-256(zcc3.wasm)`. Reports `WASMTIME_STATUS`.
  3. Pure-JS `tools/browser_wasi_shim.js` (zero `node:wasi`): `build_wasm/zcc.wasm`
     compiles C -> `.wasm` (validated & executed in V8) and C -> `.s`
     (byte-identical to `./zcc -S`).
  4. EF-0 negative controls: invalid C exits nonzero via WASI `proc_exit` and
     produces no valid `.wasm`; 1-byte mutation fails SHA-256/`cmp` and magic
     corruption fails `WebAssembly.validate()`.
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
ZCC_WASM = os.path.join(BUILD_WASM, "zcc.wasm")
ZCC2_WASM = os.path.join(BUILD_WASM, "zcc2.wasm")
ZCC3_WASM = os.path.join(BUILD_WASM, "zcc3.wasm")
BUNDLE_C = os.path.join(BUILD_WASM, "zcc_wasm_bundle.c")
SHIM_JS = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")
BUNDLER_PY = os.path.join(REPO_ROOT, "tools", "build_wasm_bundle.py")


def find_node_bin() -> str:
    found = shutil.which("node")
    if found:
        return found
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise RuntimeError("Node.js binary not found on PATH or at /home/ZKAEDI/.local/bin/node")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def inspect_wasm_v8(node_bin: str, wasm_path: str) -> dict:
    js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
let imports = [];
let err = null;
if (valid) {
  try {
    const mod = new WebAssembly.Module(b);
    imports = WebAssembly.Module.imports(mod).map(i => ({module: i.module, name: i.name, kind: i.kind}));
  } catch (e) { err = String(e); }
}
console.log(JSON.stringify({valid, bytes: b.length, imports, err}));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    return json.loads(res.stdout.strip().splitlines()[-1])


def run_node_wasi(node_bin: str, wasm_path: str, args, env_dict=None, stdin_str=""):
    """Runs a WASI Preview1 module using Node 22 `node:wasi` with JS wrapper functions
    over `wasi_snapshot_preview1` (avoids Node 22 V8 Fast-API crash under memory.grow)."""
    js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const w = new WASI({
  version: 'preview1',
  args: cfg.args,
  env: cfg.env || {},
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
  if (!WebAssembly.validate(bytes)) throw new Error('WebAssembly.validate failed');
  const mod = await WebAssembly.compile(bytes);
  const inst = await WebAssembly.instantiate(mod, imp);
  const rc = w.start(inst);
  process.exit(rc | 0);
})().catch((e) => { console.error(e); process.exit(99); });
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": wasm_path, "args": args, "env": env_dict or {}}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run(
            [node_bin, "-e", js, cfg_path],
            cwd=REPO_ROOT,
            input=stdin_str,
            capture_output=True,
            text=True,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


class TestWasmWasiSelfhost(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node_bin = find_node_bin()
        if not os.path.isfile(ZCC_BIN):
            raise RuntimeError("./zcc not built; run `make zcc` first.")
        wt = shutil.which("wasmtime")
        print(f"WASMTIME_STATUS: {wt if wt else 'NOT_INSTALLED_ON_HOST'}", flush=True)

    def test_01_wasi_runtime_primitives_dual_host(self):
        probe_dir = os.path.join(BUILD_WASM, "test_probe_01")
        shutil.rmtree(probe_dir, ignore_errors=True)
        os.makedirs(probe_dir, exist_ok=True)
        try:
            probe_src = os.path.join(probe_dir, "probe_combined.c")
            probe_wasm = os.path.join(probe_dir, "probe.wasm")
            probe_code = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
    int kind;
    long long val;
} SmallBox;

typedef struct {
    int a[20];
    int tag;
} BigBox;

static SmallBox make_small(int k, long long v) {
    SmallBox b;
    b.kind = k;
    b.val = v;
    return b;
}

static SmallBox mix_small(SmallBox a, SmallBox b) {
    a.kind += 100; /* must not mutate caller's struct copy */
    SmallBox r;
    r.kind = a.kind - 100 + b.kind;
    r.val = a.val * b.val;
    return r;
}

static void fmt_check(char *out, size_t n, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    vsnprintf(out, n, fmt, ap);
    va_end(ap);
    int v1 = va_arg(ap2, int);
    const char *s1 = va_arg(ap2, const char *);
    va_end(ap2);
    size_t len = strlen(out);
    snprintf(out + len, n - len, "|copy=%d:%s", v1, s1);
}

int main(int argc, char **argv) {
    const char *ev = getenv("ZCC_WASI_ENV");
    printf("ARGS=%d:%s:%s|ENV=%s\n", argc, argv[1], argv[2], ev ? ev : "NONE");

    /* Scanset sscanf matching part5.c peephole pattern */
    char disp[64] = {0};
    int sc = sscanf("    leaq -24(%rbp), %rax", "    leaq %63[^(](%%rbp), %%rax", disp);
    printf("SSCANF=%d:%s\n", sc, disp);

    /* Variadics + vsnprintf + va_copy */
    char fbuf[128];
    fmt_check(fbuf, sizeof(fbuf), "%d+%s", 42, "ok");
    printf("FMT=%s\n", fbuf);

    /* By-value struct parameter + nested sret return + <=64B / >64B copies */
    SmallBox s1 = make_small(3, 7);
    SmallBox s2 = mix_small(s1, make_small(4, 9));
    printf("STRUCT_S=%d:%lld|%d:%lld\n", s1.kind, s1.val, s2.kind, s2.val);

    BigBox b1, b2;
    for (int i = 0; i < 20; i++) b1.a[i] = i * 3 + 1;
    b1.tag = 777;
    b2 = b1;
    b1.a[0] = -1;
    printf("STRUCT_B=%d:%d:%d\n", b2.a[0], b2.a[19], b2.tag);

    /* Pointer += scaling */
    int arr[4] = {10, 20, 30, 40};
    int *p = arr;
    p += 2;
    printf("PTR=%d:%d\n", *p, p[1]);

    /* goto + br_table + backward jump */
    int acc = 0, idx = 0;
loop_lbl:
    acc += idx;
    idx++;
    if (idx < 5) goto loop_lbl;
    printf("GOTO=%d\n", acc);

    /* WASI File I/O: fopen, fwrite, fflush, fclose, fopen rb, fseek, ftell, fgets, fread, remove */
    const char *fpath = "build_wasm/test_probe_01/io_probe.txt";
    FILE *fw = fopen(fpath, "wb");
    if (!fw) return 10;
    const char *line1 = "hello_wasi_fs\n";
    fwrite(line1, 1, strlen(line1), fw);
    fwrite("second_line\n", 1, 12, fw);
    fclose(fw);

    FILE *fr = fopen(fpath, "rb");
    if (!fr) return 11;
    fseek(fr, 0, 2);
    long fsz = ftell(fr);
    fseek(fr, 0, 0);
    char lbuf[64] = {0};
    if (!fgets(lbuf, sizeof(lbuf), fr)) return 12;
    char rest[32] = {0};
    size_t nr = fread(rest, 1, 12, fr);
    fclose(fr);
    if (remove(fpath) != 0) return 13;
    FILE *chk = fopen(fpath, "rb");
    int gone = (chk == NULL) ? 1 : 0;
    if (chk) fclose(chk);
    lbuf[strcspn(lbuf, "\n")] = 0;
    rest[nr] = 0;
    rest[strcspn(rest, "\n")] = 0;
    printf("FS=%ld:%s:%s:DEL=%d\n", fsz, lbuf, rest, gone);
    return 0;
}
"""
            with open(os.path.join(REPO_ROOT, "src", "wasm_wasi_rt.c"), "r", encoding="utf-8") as rt_f:
                rt_txt = rt_f.read()
            with open(probe_src, "w", encoding="utf-8", newline="\n") as out_f:
                out_f.write(rt_txt + "\n" + probe_code + "\n")

            subprocess.run(
                [ZCC_BIN, "--target=wasm32", os.path.relpath(probe_src, REPO_ROOT).replace("\\", "/"),
                 "-o", os.path.relpath(probe_wasm, REPO_ROOT).replace("\\", "/")],
                cwd=REPO_ROOT, check=True, capture_output=True, text=True,
            )

            info = inspect_wasm_v8(self.node_bin, probe_wasm)
            self.assertTrue(info["valid"], f"Probe WASM failed V8 validate: {info}")
            non_wasi = [i for i in info["imports"] if i["module"] != "wasi_snapshot_preview1"]
            self.assertEqual(non_wasi, [], f"Forbidden non-WASI imports in probe: {non_wasi}")

            expected_stdout = (
                "ARGS=3:alpha:beta|ENV=wasi_ok\n"
                "SSCANF=1:-24\n"
                "FMT=42+ok|copy=42:ok\n"
                "STRUCT_S=3:7|7:63\n"
                "STRUCT_B=1:58:777\n"
                "PTR=30:40\n"
                "GOTO=10\n"
                "FS=26:hello_wasi_fs:second_line:DEL=1\n"
            )

            # Host A: Node 22 node:wasi
            rel_wasm = os.path.relpath(probe_wasm, REPO_ROOT).replace("\\", "/")
            res_nw = run_node_wasi(
                self.node_bin, rel_wasm, ["probe.wasm", "alpha", "beta"],
                env_dict={"ZCC_WASI_ENV": "wasi_ok"},
            )
            self.assertEqual(res_nw.returncode, 0, f"node:wasi probe rc={res_nw.returncode}\n{res_nw.stderr}")
            self.assertEqual(res_nw.stdout, expected_stdout)

            # Host B: Pure-JS BrowserWasiShim (in-memory Map VFS)
            shim_js = r"""
const fs = require('fs');
const { BrowserWasiShim } = require('./tools/browser_wasi_shim.js');
(async () => {
  const shim = new BrowserWasiShim({
    args: ['probe.wasm', 'alpha', 'beta'],
    env: { ZCC_WASI_ENV: 'wasi_ok' },
    files: {},
    preopenDir: '.',
  });
  const res = await shim.run(fs.readFileSync(process.argv[1]));
  process.stdout.write(res.stdout);
  process.stderr.write(res.stderr);
  process.exit(res.exitCode | 0);
})().catch(e => { console.error(e); process.exit(99); });
"""
            res_sh = subprocess.run(
                [self.node_bin, "-e", shim_js, rel_wasm],
                cwd=REPO_ROOT, capture_output=True, text=True,
            )
            self.assertEqual(res_sh.returncode, 0, f"BrowserWasiShim probe rc={res_sh.returncode}\n{res_sh.stderr}")
            self.assertEqual(res_sh.stdout, expected_stdout)
            print("[TEST_01 PASS] Dual-host (node:wasi & BrowserWasiShim) WASI primitives & ABI verified.")
        finally:
            shutil.rmtree(probe_dir, ignore_errors=True)

    def test_02_wasi_selfhost_convergence(self):
        # Stage 1: native ./zcc -> build_wasm/zcc.wasm + V8 validate + zero env.* imports
        res_build = subprocess.run(
            [sys.executable, BUNDLER_PY],
            cwd=REPO_ROOT, capture_output=True, text=True, check=True,
        )
        info1 = inspect_wasm_v8(self.node_bin, ZCC_WASM)
        self.assertTrue(info1["valid"])
        self.assertEqual([i for i in info1["imports"] if i["module"] != "wasi_snapshot_preview1"], [])
        sha1 = sha256_file(ZCC_WASM)

        rel_bundle = os.path.relpath(BUNDLE_C, REPO_ROOT).replace("\\", "/")
        rel_zcc1 = os.path.relpath(ZCC_WASM, REPO_ROOT).replace("\\", "/")
        rel_zcc2 = os.path.relpath(ZCC2_WASM, REPO_ROOT).replace("\\", "/")
        rel_zcc3 = os.path.relpath(ZCC3_WASM, REPO_ROOT).replace("\\", "/")

        # Stage 2: build_wasm/zcc.wasm -> build_wasm/zcc2.wasm under Node 22 node:wasi
        res2 = run_node_wasi(
            self.node_bin, rel_zcc1,
            [rel_zcc1, "--target=wasm32", "-Iinclude", "-Isrc", "-I.", rel_bundle, "-o", rel_zcc2],
        )
        self.assertEqual(res2.returncode, 0, f"Stage 2 zcc.wasm failed rc={res2.returncode}\n{res2.stderr}")
        info2 = inspect_wasm_v8(self.node_bin, ZCC2_WASM)
        self.assertTrue(info2["valid"])
        self.assertEqual([i for i in info2["imports"] if i["module"] != "wasi_snapshot_preview1"], [])
        sha2 = sha256_file(ZCC2_WASM)

        # Stage 3: build_wasm/zcc2.wasm -> build_wasm/zcc3.wasm under Node 22 node:wasi
        res3 = run_node_wasi(
            self.node_bin, rel_zcc2,
            [rel_zcc2, "--target=wasm32", "-Iinclude", "-Isrc", "-I.", rel_bundle, "-o", rel_zcc3],
        )
        self.assertEqual(res3.returncode, 0, f"Stage 3 zcc2.wasm failed rc={res3.returncode}\n{res3.stderr}")
        info3 = inspect_wasm_v8(self.node_bin, ZCC3_WASM)
        self.assertTrue(info3["valid"])
        self.assertEqual([i for i in info3["imports"] if i["module"] != "wasi_snapshot_preview1"], [])
        sha3 = sha256_file(ZCC3_WASM)

        cmp_res = subprocess.run(["cmp", ZCC2_WASM, ZCC3_WASM], capture_output=True, text=True)
        self.assertEqual(cmp_res.returncode, 0, f"cmp zcc2.wasm zcc3.wasm failed: {cmp_res.stdout} {cmp_res.stderr}")
        self.assertEqual(sha2, sha3)
        print(
            f"[TEST_02 PASS] WASI Self-Host Converged:\n"
            f"  zcc.wasm : {os.path.getsize(ZCC_WASM)}B SHA256={sha1}\n"
            f"  zcc2.wasm: {os.path.getsize(ZCC2_WASM)}B SHA256={sha2}\n"
            f"  zcc3.wasm: {os.path.getsize(ZCC3_WASM)}B SHA256={sha3}\n"
            f"  cmp zcc2.wasm zcc3.wasm: EXIT 0"
        )

    def test_03_browser_wasi_shim_compile_wasm_and_asm(self):
        if not os.path.isfile(ZCC_WASM):
            subprocess.run([sys.executable, BUNDLER_PY], cwd=REPO_ROOT, check=True)

        c_src = (
            "int fib(int n) { return (n <= 1) ? n : fib(n - 1) + fib(n - 2); }\n"
            "int main(void) { return fib(10); }\n"
        )
        shim_work = os.path.join(BUILD_WASM, "test_shim_03")
        shutil.rmtree(shim_work, ignore_errors=True)
        os.makedirs(shim_work, exist_ok=True)
        try:
            native_c = os.path.join(shim_work, "fib.c")
            native_s = os.path.join(shim_work, "fib_native.s")
            with open(native_c, "w", encoding="utf-8", newline="\n") as f:
                f.write(c_src)

            rel_c = "fib.c"
            # Generate native reference from a CWD with identical input arg `fib.c`
            shutil.copyfile(native_c, os.path.join(shim_work, "fib_ref_copy.c"))
            subprocess.run(
                [ZCC_BIN, "-S", "fib.c", "-o", "fib_native.s"],
                cwd=shim_work, check=True, capture_output=True, text=True,
            )
            with open(native_s, "rb") as f:
                expected_asm = f.read()

            # Pure-JS BrowserWasiShim runs build_wasm/zcc.wasm with zero node:wasi
            driver_js = r"""
const fs = require('fs');
const { BrowserWasiShim } = require('./tools/browser_wasi_shim.js');

(async () => {
  const zccBytes = fs.readFileSync(process.argv[1]);
  const cText = process.argv[2];

  // 1. C -> .wasm in-memory VFS
  const s1 = new BrowserWasiShim({
    args: ['zcc.wasm', '--target=wasm32', 'fib.c', '-o', 'fib.wasm'],
    files: { 'fib.c': cText },
    preopenDir: '.',
  });
  const r1 = await s1.run(zccBytes);
  if (r1.exitCode !== 0) throw new Error('WASM compile failed: ' + r1.stderr);
  const fibWasm = s1.getFile('fib.wasm');
  if (!fibWasm || !WebAssembly.validate(fibWasm)) throw new Error('Compiled fib.wasm invalid');
  const fibMod = new WebAssembly.Module(fibWasm);
  const fibInst = new WebAssembly.Instance(fibMod, {});
  const fibResult = fibInst.exports.main();

  // 2. C -> -S .s in-memory VFS
  const s2 = new BrowserWasiShim({
    args: ['zcc.wasm', '-S', 'fib.c', '-o', 'fib.s'],
    files: { 'fib.c': cText },
    preopenDir: '.',
  });
  const r2 = await s2.run(zccBytes);
  if (r2.exitCode !== 0) throw new Error('-S compile failed: ' + r2.stderr);
  const fibS = s2.getFile('fib.s');
  fs.writeFileSync(process.argv[3], Buffer.from(fibS));
  console.log(JSON.stringify({wasmBytes: fibWasm.length, fibResult, asmBytes: fibS.length}));
})().catch(e => { console.error(e); process.exit(1); });
"""
            shim_s = os.path.join(shim_work, "fib_shim.s")
            rel_zcc = os.path.relpath(ZCC_WASM, REPO_ROOT).replace("\\", "/")
            res = subprocess.run(
                [self.node_bin, "-e", driver_js, rel_zcc, c_src, shim_s],
                cwd=REPO_ROOT, capture_output=True, text=True, check=True,
            )
            info = json.loads(res.stdout.strip().splitlines()[-1])
            self.assertEqual(info["fibResult"], 55)
            with open(shim_s, "rb") as f:
                actual_asm = f.read()
            self.assertEqual(
                actual_asm, expected_asm,
                f"BrowserWasiShim -S output differed from native ./zcc -S (bytes {len(actual_asm)} vs {len(expected_asm)})",
            )
            print(
                f"[TEST_03 PASS] Pure-JS BrowserWasiShim: fib(10)={info['fibResult']} "
                f"(.wasm {info['wasmBytes']}B) & -S {info['asmBytes']}B byte-identical to native ./zcc."
            )
        finally:
            shutil.rmtree(shim_work, ignore_errors=True)

    def test_04_ef0_negative_controls(self):
        if not os.path.isfile(ZCC_WASM):
            subprocess.run([sys.executable, BUNDLER_PY], cwd=REPO_ROOT, check=True)
        neg_dir = os.path.join(BUILD_WASM, "test_neg_04")
        shutil.rmtree(neg_dir, ignore_errors=True)
        os.makedirs(neg_dir, exist_ok=True)
        try:
            # NC-A: Invalid C compiled by zcc.wasm must exit nonzero & produce no output .wasm
            bad_c = os.path.join(neg_dir, "broken.c")
            bad_wasm = os.path.join(neg_dir, "broken.wasm")
            with open(bad_c, "w", encoding="utf-8") as f:
                f.write("int main(void) { return @@@ invalid syntax ??? ; }\n")

            rel_zcc = os.path.relpath(ZCC_WASM, REPO_ROOT).replace("\\", "/")
            rel_bad_c = os.path.relpath(bad_c, REPO_ROOT).replace("\\", "/")
            rel_bad_wasm = os.path.relpath(bad_wasm, REPO_ROOT).replace("\\", "/")
            res_bad = run_node_wasi(
                self.node_bin, rel_zcc,
                [rel_zcc, "--target=wasm32", rel_bad_c, "-o", rel_bad_wasm],
            )
            self.assertNotEqual(res_bad.returncode, 0, "EF-0 NC-A FAIL: invalid C unexpectedly exited 0!")
            self.assertFalse(os.path.exists(bad_wasm), "EF-0 NC-A FAIL: invalid C emitted an output .wasm!")

            # NC-B: 1-byte payload mutation fails cmp & SHA-256; magic corruption fails WebAssembly.validate()
            src_wasm = ZCC3_WASM if os.path.isfile(ZCC3_WASM) else ZCC_WASM
            mut_wasm = os.path.join(neg_dir, "mutated.wasm")
            with open(src_wasm, "rb") as f:
                orig = bytearray(f.read())
            orig_sha = sha256_bytes(bytes(orig))

            payload_mut = bytearray(orig)
            payload_mut[-17] ^= 0x01
            with open(mut_wasm, "wb") as f:
                f.write(payload_mut)
            cmp_mut = subprocess.run(["cmp", src_wasm, mut_wasm], capture_output=True, text=True)
            self.assertNotEqual(cmp_mut.returncode, 0, "EF-0 NC-B1 FAIL: cmp failed to detect 1-byte mutation!")
            self.assertNotEqual(sha256_file(mut_wasm), orig_sha, "EF-0 NC-B1 FAIL: SHA-256 matched after mutation!")

            magic_mut = bytearray(orig)
            magic_mut[0] ^= 0xFF
            with open(mut_wasm, "wb") as f:
                f.write(magic_mut)
            info_mut = inspect_wasm_v8(self.node_bin, mut_wasm)
            self.assertFalse(info_mut["valid"], "EF-0 NC-B2 FAIL: WebAssembly.validate() accepted corrupted magic!")

            print(
                f"[TEST_04 PASS] EF-0 Negative Controls verified: invalid C rc={res_bad.returncode} (no .wasm), "
                f"1-byte mutation caught by cmp/SHA-256, corrupt magic caught by WebAssembly.validate()=False."
            )
        finally:
            shutil.rmtree(neg_dir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
