#!/usr/bin/env python3
"""
ZCC WebAssembly Self-Host Bundler & Validator (build_wasm/zcc.wasm)
===================================================================
Combines:
  - src/wasm_emit.h
  - src/wasm_wasi_rt.c
  - src/wasm_emit.c
  - zcc.c
into build_wasm/zcc_wasm_bundle.c (preserving include guards and stripping
duplicate #include "wasm_emit.h" / "src/wasm_emit.h" directives), compiles
via `./zcc --target=wasm32 -Iinclude -Isrc -I.`, and validates with Node 22 V8:
  - WebAssembly.validate(bytes) == true
  - new WebAssembly.Module(bytes) succeeds
  - Zero `env.*` imports (strictly `wasi_snapshot_preview1.*`)
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_BUNDLE = os.path.join(REPO_ROOT, "build_wasm", "zcc_wasm_bundle.c")
DEFAULT_WASM = os.path.join(REPO_ROOT, "build_wasm", "zcc.wasm")

INCLUDE_WASM_EMIT_RE = re.compile(
    r'^\s*#\s*include\s+"(?:src/)?wasm_emit\.h"\s*(?:/\*.*\*/|//.*)?$'
)


def find_node_bin() -> str:
    found = shutil.which("node")
    if found:
        return found
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise RuntimeError("node binary not found on PATH or at /home/ZKAEDI/.local/bin/node")


def strip_wasm_emit_include(text: str) -> str:
    out_lines = []
    for line in text.splitlines(keepends=True):
        if INCLUDE_WASM_EMIT_RE.match(line):
            out_lines.append("/* stripped duplicate wasm_emit.h include for bundle */\n")
        else:
            out_lines.append(line)
    return "".join(out_lines)


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    header_p = os.path.join(REPO_ROOT, "src", "wasm_emit.h")
    rt_p = os.path.join(REPO_ROOT, "src", "wasm_wasi_rt.c")
    emit_p = os.path.join(REPO_ROOT, "src", "wasm_emit.c")
    zcc_p = os.path.join(REPO_ROOT, "zcc.c")
    stubs_p = os.path.join(REPO_ROOT, "src", "wasm_zcc_stubs.c")

    for p in (header_p, rt_p, emit_p, zcc_p, stubs_p):
        if not os.path.isfile(p):
            raise FileNotFoundError(f"Missing required bundle source: {p}")

    with open(header_p, "r", encoding="utf-8", errors="replace") as f:
        header_txt = f.read()
    with open(rt_p, "r", encoding="utf-8", errors="replace") as f:
        rt_txt = strip_wasm_emit_include(f.read())
    with open(emit_p, "r", encoding="utf-8", errors="replace") as f:
        emit_txt = strip_wasm_emit_include(f.read())
    with open(zcc_p, "r", encoding="utf-8", errors="replace") as f:
        zcc_txt = strip_wasm_emit_include(f.read())
    with open(stubs_p, "r", encoding="utf-8", errors="replace") as f:
        stubs_txt = strip_wasm_emit_include(f.read())

    parts = [
        "/* Auto-generated ZCC WASM/WASI Self-Host Bundle */\n",
        "/* === [1/5] src/wasm_emit.h === */\n",
        header_txt,
        "\n/* === [2/5] src/wasm_wasi_rt.c === */\n",
        rt_txt,
        "\n/* === [3/5] src/wasm_emit.c === */\n",
        emit_txt,
        "\n/* === [4/5] zcc.c === */\n",
        zcc_txt,
        "\n/* === [5/5] src/wasm_zcc_stubs.c === */\n",
        stubs_txt,
        "\n",
    ]
    bundle_txt = "".join(parts)
    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(bundle_txt)
    return bundle_path


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def validate_wasm_v8(wasm_path: str, node_bin: str) -> dict:
    js_script = r"""
const fs = require('fs');
const bytes = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(bytes);
let compiled = false;
let imports = [];
let exportsList = [];
let errMsg = null;
try {
  const mod = new WebAssembly.Module(bytes);
  compiled = true;
  imports = WebAssembly.Module.imports(mod).map(i => ({module: i.module, name: i.name, kind: i.kind}));
  exportsList = WebAssembly.Module.exports(mod).map(e => ({name: e.name, kind: e.kind}));
} catch (e) {
  errMsg = String(e && e.stack ? e.stack : e);
}
console.log(JSON.stringify({valid, compiled, bytes: bytes.length, imports, exports: exportsList, errMsg}));
"""
    res = subprocess.run(
        [node_bin, "-e", js_script, wasm_path],
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"V8 validate script failed (rc={res.returncode}):\n{res.stdout}\n{res.stderr}")
    return json.loads(res.stdout.strip().splitlines()[-1])


def main() -> int:
    ap = argparse.ArgumentParser(description="Build and validate build_wasm/zcc.wasm")
    ap.add_argument("--bundle-out", default=DEFAULT_BUNDLE)
    ap.add_argument("--wasm-out", default=DEFAULT_WASM)
    ap.add_argument("--bundle-only", action="store_true")
    args = ap.parse_args()

    bundle_path = generate_bundle(args.bundle_out)
    print(f"[BUNDLE] Wrote {os.path.relpath(bundle_path, REPO_ROOT)} ({os.path.getsize(bundle_path)} bytes)")
    if args.bundle_only:
        return 0

    zcc_bin = os.path.join(REPO_ROOT, "zcc")
    if not os.path.isfile(zcc_bin):
        raise FileNotFoundError(f"Host compiler not found at {zcc_bin}; run `make zcc` first.")

    rel_bundle = os.path.relpath(bundle_path, REPO_ROOT).replace("\\", "/")
    rel_wasm = os.path.relpath(args.wasm_out, REPO_ROOT).replace("\\", "/")
    cmd = [zcc_bin, "--target=wasm32", "-Iinclude", "-Isrc", "-I.", rel_bundle, "-o", rel_wasm]
    print("[BUILD]", " ".join(cmd))
    res = subprocess.run(cmd, cwd=REPO_ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.stdout.strip():
        print(res.stdout.strip())
    if res.stderr.strip():
        print(res.stderr.strip(), file=sys.stderr)
    if res.returncode != 0:
        print(f"[ERROR] ./zcc --target=wasm32 exited with code {res.returncode}", file=sys.stderr)
        return res.returncode

    wasm_size = os.path.getsize(args.wasm_out)
    wasm_sha = sha256_file(args.wasm_out)
    print(f"[WASM] {rel_wasm}: {wasm_size} bytes, SHA256={wasm_sha}")

    node_bin = find_node_bin()
    info = validate_wasm_v8(args.wasm_out, node_bin)
    print(f"[V8] valid={info['valid']} compiled={info['compiled']} errMsg={info['errMsg']}")
    imports = info["imports"]
    print(f"[IMPORTS] count={len(imports)}")
    for imp in imports:
        print(f"  - {imp['module']}.{imp['name']} ({imp['kind']})")
    env_imports = [i for i in imports if i["module"] != "wasi_snapshot_preview1"]
    if not info["valid"] or not info["compiled"]:
        print("[FAIL] WebAssembly V8 validation failed!", file=sys.stderr)
        return 2
    if env_imports:
        print(f"[FAIL] Found forbidden non-WASI imports: {env_imports}", file=sys.stderr)
        return 3
    print(f"[PASS] V8 WebAssembly.validate()=true, zero env.* imports ({len(imports)} wasi_snapshot_preview1 imports)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
