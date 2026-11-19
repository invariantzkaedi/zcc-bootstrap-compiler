#!/usr/bin/env python3
"""
Standalone SQLite 3.45.0 WASI Builder & Validator (build_wasm/sqlite3.wasm)
===========================================================================
Compiles unmodified upstream `src/sqlite3.c` (9.0 MB) + `src/wasm_wasi_rt.c` +
`src/wasm_sqlite_wasi_vfs.c` using `./zcc --target=wasm32` into
`build_wasm/sqlite3.wasm`, verifies V8 validation + zero `env.*` imports, and
optionally probes whether `build_wasm/zcc.wasm` inside Node 22 WASI can also
compile the 9.0 MB amalgamation within WASM linear memory.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
ZCC_WASM = os.path.join(BUILD_WASM, "zcc.wasm")
DEFAULT_BUNDLE = os.path.join(BUILD_WASM, "sqlite3_wasi_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_WASM, "sqlite3.wasm")


def find_node_bin() -> str:
    found = shutil.which("node")
    if found:
        return found
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise RuntimeError("node binary not found on PATH or at /home/ZKAEDI/.local/bin/node")


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
let compiled = false;
let imports = [];
let err = null;
if (valid) {
  try {
    const mod = new WebAssembly.Module(b);
    compiled = true;
    imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
  } catch (e) {
    err = String(e);
  }
}
console.log(JSON.stringify({valid, compiled, bytes: b.length, imports, err}));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    return json.loads(res.stdout.strip().splitlines()[-1])


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    for req in ("src/wasm_wasi_rt.c", "src/sqlite3.c", "src/sqlite3.h", "src/wasm_sqlite_wasi_vfs.c"):
        full = os.path.join(REPO_ROOT, req)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing required SQLite WASI source: {full}")
    txt = (
        "/* Auto-generated Standalone SQLite 3.45.0 WASI Bundle */\n"
        "#define SQLITE_THREADSAFE 0\n"
        "#define SQLITE_OMIT_LOAD_EXTENSION 1\n"
        "#define SQLITE_OMIT_WAL 1\n"
        "#define SQLITE_TEMP_STORE 3\n"
        "#define SQLITE_OS_OTHER 1\n"
        "#define SQLITE_ENABLE_MEMORY_MANAGEMENT 0\n"
        '#include "src/wasm_wasi_rt.c"\n'
        '#include "src/sqlite3.c"\n'
        '#include "src/wasm_sqlite_wasi_vfs.c"\n'
    )
    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(txt)
    return bundle_path


def probe_zcc_wasm_sqlite(node_bin: str, bundle_rel: str, probe_out_rel: str, timeout_s: int = 180) -> dict:
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
})().catch((e) => { console.error(e); process.exit(99); });
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(
            {
                "wasm": ZCC_WASM,
                "args": ["zcc.wasm", "--target=wasm32", "-Isrc", "-I.", bundle_rel, "-o", probe_out_rel],
            },
            tf,
        )
        cfg_path = tf.name
    try:
        try:
            res = subprocess.run(
                [node_bin, "-e", js, cfg_path],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=timeout_s,
            )
            return {"status": "ok" if res.returncode == 0 else "nonzero", "rc": res.returncode, "stderr_tail": res.stderr[-400:]}
        except subprocess.TimeoutExpired:
            return {"status": "timeout", "rc": -1, "stderr_tail": f"timed out after {timeout_s}s"}
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone SQLite 3.45.0 WASI module via zcc.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--probe-wasm-selfcompile", action="store_true")
    parser.add_argument("--probe-timeout", type=int, default=180)
    args = parser.parse_args()

    node_bin = find_node_bin()
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")

    bundle_path = generate_bundle(args.bundle)
    bundle_rel = os.path.relpath(bundle_path, REPO_ROOT).replace("\\", "/")
    out_rel = os.path.relpath(args.output, REPO_ROOT).replace("\\", "/")

    cmd = [ZCC_BIN, "--target=wasm32", "-Isrc", "-I.", bundle_rel, "-o", out_rel]
    print(f"[BUILD] {' '.join(cmd)}", flush=True)
    res = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stdout + res.stderr)
        return res.returncode

    info = inspect_wasm_v8(node_bin, args.output)
    out_sha = sha256_file(args.output)
    print(f"[WASM] {args.output}: {info['bytes']} bytes, SHA256={out_sha}", flush=True)
    print(f"[V8] valid={info['valid']} compiled={info['compiled']} err={info['err']}", flush=True)
    non_wasi = [imp for imp in info["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
    print(f"[IMPORTS] count={len(info['imports'])} non_wasi={non_wasi}", flush=True)
    if not info["valid"] or not info["compiled"] or non_wasi:
        return 1

    if args.probe_wasm_selfcompile:
        probe_out = os.path.join(BUILD_WASM, "sqlite3_inwasi_probe.wasm")
        probe_rel = os.path.relpath(probe_out, REPO_ROOT).replace("\\", "/")
        print(f"[PROBE-WASM] Invoking build_wasm/zcc.wasm on {bundle_rel} (timeout={args.probe_timeout}s)...", flush=True)
        pr = probe_zcc_wasm_sqlite(node_bin, bundle_rel, probe_rel, timeout_s=args.probe_timeout)
        if pr["status"] == "ok" and os.path.isfile(probe_out):
            inwasi_sha = sha256_file(probe_out)
            probe_info = inspect_wasm_v8(node_bin, probe_out)
            probe_non_wasi = [imp for imp in probe_info["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
            print(
                f"[PROBE-WASM] rc=0 bytes={probe_info['bytes']} valid={probe_info['valid']} "
                f"compiled={probe_info['compiled']} non_wasi={probe_non_wasi} SHA256={inwasi_sha} "
                f"byte_identical={inwasi_sha == out_sha}",
                flush=True,
            )
            os.remove(probe_out)
            if not probe_info["valid"] or not probe_info["compiled"] or probe_non_wasi:
                return 1
        else:
            if os.path.isfile(probe_out):
                os.remove(probe_out)
            print(f"[PROBE-WASM] status={pr['status']} rc={pr['rc']} tail={pr['stderr_tail']!r}", flush=True)

    print("[PASS] Standalone build_wasm/sqlite3.wasm validated with zero env.* imports!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
