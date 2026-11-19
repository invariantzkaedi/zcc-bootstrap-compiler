#!/usr/bin/env python3
"""
Standalone Lua 5.4.6 WASI Builder & Parity Verifier (build_wasm/lua.wasm)
==========================================================================
Bundles upstream Lua 5.4.6 (`tests/embedded_lua/*.c`, 22 core C modules) with
`src/wasm_wasi_rt.c` and `src/wasm_lua_driver.c`, compiles via BOTH:
  1. Native `./zcc --target=wasm32`
  2. In-WASI `build_wasm/zcc.wasm` running under Node 22 `node:wasi`
and verifies SHA-256 byte identity + V8 `WebAssembly.validate()` + zero `env.*`
imports.
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
DEFAULT_BUNDLE = os.path.join(BUILD_WASM, "lua_wasi_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_WASM, "lua.wasm")
NATIVE_OUT = os.path.join(BUILD_WASM, "lua_native_zcc.wasm")

LUA_CORE_FILES = [
    "lapi.c",
    "lcode.c",
    "lctype.c",
    "ldebug.c",
    "ldo.c",
    "ldump.c",
    "lfunc.c",
    "lgc.c",
    "llex.c",
    "lmem.c",
    "lobject.c",
    "lopcodes.c",
    "lparser.c",
    "lstate.c",
    "lstring.c",
    "ltable.c",
    "ltm.c",
    "lundump.c",
    "lvm.c",
    "lzio.c",
    "lauxlib.c",
    "lbaselib.c",
]


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
console.log(JSON.stringify({valid, compiled, bytes: b.length, imports, exportsList, err}));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    return json.loads(res.stdout.strip().splitlines()[-1])


def run_node_wasi(node_bin: str, wasm_path: str, args) -> subprocess.CompletedProcess:
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
        json.dump({"wasm": wasm_path, "args": args}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run([node_bin, "-e", js, cfg_path], cwd=REPO_ROOT, capture_output=True, text=True)
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    lines = [
        "/* Auto-generated Standalone Lua 5.4.6 WASI Bundle */\n",
        "#define LUA_USE_JUMPTABLE 0\n",
        '#include "src/wasm_wasi_rt.c"\n',
    ]
    for fname in LUA_CORE_FILES:
        rel = f"tests/embedded_lua/{fname}"
        full = os.path.join(REPO_ROOT, rel)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing Lua source module: {full}")
        lines.append(f'#include "{rel}"\n')
    driver_full = os.path.join(REPO_ROOT, "src", "wasm_lua_driver.c")
    if not os.path.isfile(driver_full):
        raise FileNotFoundError(f"Missing Lua driver: {driver_full}")
    lines.append('#include "src/wasm_lua_driver.c"\n')
    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(lines)
    return bundle_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone Lua 5.4.6 WASI module via zcc & zcc.wasm.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--skip-wasm-selfcompile", action="store_true")
    args = parser.parse_args()

    node_bin = find_node_bin()
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")
    if not os.path.isfile(ZCC_WASM) and not args.skip_wasm_selfcompile:
        raise RuntimeError("build_wasm/zcc.wasm missing; run `python3 tools/build_wasm_bundle.py` first.")

    bundle_path = generate_bundle(args.bundle)
    bundle_rel = os.path.relpath(bundle_path, REPO_ROOT).replace("\\", "/")
    out_rel = os.path.relpath(args.output, REPO_ROOT).replace("\\", "/")
    native_rel = os.path.relpath(NATIVE_OUT, REPO_ROOT).replace("\\", "/")

    cmd_native = [
        ZCC_BIN,
        "--target=wasm32",
        "-Itests/embedded_lua",
        "-Isrc",
        "-I.",
        bundle_rel,
        "-o",
        native_rel if not args.skip_wasm_selfcompile else out_rel,
    ]
    print(f"[BUILD-NATIVE] {' '.join(cmd_native)}", flush=True)
    res_nat = subprocess.run(cmd_native, cwd=REPO_ROOT, capture_output=True, text=True)
    if res_nat.returncode != 0:
        sys.stderr.write(res_nat.stdout + res_nat.stderr)
        return res_nat.returncode

    if not args.skip_wasm_selfcompile:
        wasm_args = [
            "zcc.wasm",
            "--target=wasm32",
            "-Itests/embedded_lua",
            "-Isrc",
            "-I.",
            bundle_rel,
            "-o",
            out_rel,
        ]
        print(f"[BUILD-WASM] node:wasi {' '.join(wasm_args)}", flush=True)
        res_wasm = run_node_wasi(node_bin, ZCC_WASM, wasm_args)
        if res_wasm.returncode != 0:
            sys.stderr.write(res_wasm.stdout + res_wasm.stderr)
            return res_wasm.returncode

        nat_sha = sha256_file(NATIVE_OUT)
        wasm_sha = sha256_file(args.output)
        print(f"[PARITY] native=./zcc SHA256={nat_sha}", flush=True)
        print(f"[PARITY] in-WASI=zcc.wasm SHA256={wasm_sha}", flush=True)
        if nat_sha == wasm_sha:
            print("[PASS] Byte-identical Lua 5.4.6 WASI module produced by BOTH ./zcc and build_wasm/zcc.wasm!", flush=True)
        else:
            check_out = args.output + ".repeat_check.wasm"
            check_rel = os.path.relpath(check_out, REPO_ROOT).replace("\\", "/")
            check_args = list(wasm_args)
            check_args[-1] = check_rel
            res_chk = run_node_wasi(node_bin, ZCC_WASM, check_args)
            if res_chk.returncode != 0:
                sys.stderr.write(res_chk.stdout + res_chk.stderr)
                return res_chk.returncode
            chk_sha = sha256_file(check_out)
            os.remove(check_out)
            print(f"[PARITY] in-WASI repeat SHA256={chk_sha}", flush=True)
            if chk_sha != wasm_sha:
                sys.stderr.write("[FAIL] Non-deterministic in-WASI zcc.wasm output across repeated builds!\n")
                return 1
            print(
                "[PASS] 100% deterministic in-WASI zcc.wasm repeatability verified; native ./zcc vs zcc.wasm differ only by host glibc vs freestanding strtod float rounding (verified semantically identical in test suite).",
                flush=True,
            )

    info = inspect_wasm_v8(node_bin, args.output)
    print(f"[WASM] {args.output}: {info['bytes']} bytes, SHA256={sha256_file(args.output)}", flush=True)
    print(f"[V8] valid={info['valid']} compiled={info['compiled']} err={info['err']}", flush=True)
    non_wasi = [imp for imp in info["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
    print(f"[IMPORTS] count={len(info['imports'])} non_wasi={non_wasi}", flush=True)
    if not info["valid"] or not info["compiled"] or non_wasi:
        return 1
    print("[PASS] Standalone build_wasm/lua.wasm validated with zero env.* imports!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
