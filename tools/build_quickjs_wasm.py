#!/usr/bin/env python3
"""
Standalone Upstream QuickJS 2024-01-13 WASI Builder (`build_wasm/qjs.wasm`)
===========================================================================
Bundles the 5 strictly unmodified upstream QuickJS 2024-01-13 C core modules
(`tests/embedded_quickjs/{cutils,libunicode,libregexp,libbf,quickjs}.c`) with
`src/wasm_wasi_rt.c` and `src/wasm_qjs_driver.c`, compiles cleanly via
`./zcc --target=wasm32` (and optionally probes `build_wasm/zcc.wasm`) into
standalone WASI Preview1 `build_wasm/qjs.wasm` with zero `env.*` imports.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
ZCC_WASM = os.path.join(BUILD_WASM, "zcc.wasm")
QJS_DIR = os.path.join(REPO_ROOT, "tests", "embedded_quickjs")
DEFAULT_BUNDLE = os.path.join(BUILD_WASM, "qjs_wasi_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_WASM, "qjs.wasm")

UPSTREAM_TARBALL_URL = "https://bellard.org/quickjs/quickjs-2024-01-13.tar.xz"
UPSTREAM_TARBALL_SHA256 = "3c4bf8f895bfa54beb486c8d1218112771ecfc5ac3be1036851ef41568212e03"
UPSTREAM_FILE_SHA256 = {
    "LICENSE": "f41baf09eef895d468d18c23055d74f711e4b8b2641cef279b5d71285c07bfe8",
    "cutils.c": "82a2cf31e18fb5d080e0ef146886af46a336bd523fb5ad01e4187c687008ebf2",
    "cutils.h": "f645b0f9a9587204df187a435d6c314747af54b8cc884caa5258281f748682e7",
    "libbf.c": "d23726ba993a69c9966de072c563569750f06bf2e6d0647447dd82139b44c159",
    "libbf.h": "71a0d4e540013f5f6006300a1a806b72155afc046739153f5865a8cb92219e1b",
    "libregexp-opcode.h": "2385f4fdfa5cabbd5e8d3bc88d80aa2058527e2cd2fc2b57f5e95ee27edb8cae",
    "libregexp.c": "076be6e24046d5561b7009b7777e959033811b15270ad9ad3c962f89db56c464",
    "libregexp.h": "dd6780f3c4d6ff1075224919508957dcbcedd36ac383f5b190171023e8a789c4",
    "libunicode-table.h": "3fb764cd5ff98a15bf8bc31d2f8d7f4f3050e61ca565bda2a878561569b9a6d4",
    "libunicode.c": "e7f711afca3a9717644d4c4f2ba9110f3ea02b7e75b778016b1b124dcbebf6cd",
    "libunicode.h": "a5cfc0b989f1ae8180da33370d54852c78ad3db54111907daada0be0dda49724",
    "list.h": "484a604d8996a9c23c23d453ec5017af05b2c74de4bb34da8abcd5f5b5880f3e",
    "quickjs-atom.h": "442137751a67e0d6c6a6cde984a42fdc47877c6fb02598bc5a8cdecae9cb9d0f",
    "quickjs-opcode.h": "94ccec05d82f048ebcec201444b1ad14618a7c4d6e47439894ca40c40103432e",
    "quickjs.c": "21f072e8b24ee115562ff961bafd4b0464d7a30828c43b1884f4828c24370c54",
    "quickjs.h": "bb5fa20aeb433f53dc439d4d28656658c294013e6652aa22c479dd56f7d1f0df",
}

QJS_CORE_MODULES = [
    "cutils.c",
    "libunicode.c",
    "libregexp.c",
    "libbf.c",
    "quickjs.c",
]

STRIP_SYS_HEADERS = {
    "stdlib.h",
    "stdio.h",
    "stdarg.h",
    "string.h",
    "assert.h",
    "inttypes.h",
    "sys/time.h",
    "time.h",
    "fenv.h",
    "math.h",
    "malloc.h",
    "malloc/malloc.h",
    "malloc_np.h",
    "immintrin.h",
    "pthread.h",
    "stdatomic.h",
    "errno.h",
    "stddef.h",
    "stdint.h",
    "limits.h",
    "float.h",
    "ctype.h",
}


def find_node_bin() -> str:
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    found = shutil.which("node")
    if found:
        return found
    raise RuntimeError("node binary not found at /home/ZKAEDI/.local/bin/node or on PATH")


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


def run_node_wasi(node_bin: str, wasm_path: str, args, timeout: int = 240) -> subprocess.CompletedProcess:
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
        return subprocess.run(
            [node_bin, "-e", js, cfg_path],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def sanitize_qjs_module(mod_name: str, raw: str, local_hdrs: set) -> str:
    """Adapt an upstream QuickJS module for single-TU WASI bundling without altering files on disk."""
    raw = raw.replace("\r\n", "\n").replace("\r", "\n")
    out_lines = []
    for line in raw.splitlines(keepends=True):
        m_ang = re.match(r"^\s*#\s*include\s*<([^>]+)>", line)
        if m_ang and m_ang.group(1) in STRIP_SYS_HEADERS:
            out_lines.append(f"/* stripped <{m_ang.group(1)}> */\n")
            continue
        m_quo = re.match(r'^\s*#\s*include\s*"([^"]+)"', line)
        if m_quo and m_quo.group(1) in local_hdrs:
            # Explicit repo-relative path prevents include/quickjs.h from shadowing tests/embedded_quickjs/quickjs.h
            out_lines.append(f'#include "tests/embedded_quickjs/{m_quo.group(1)}"\n')
            continue
        if "_is_forbidden" in line:
            out_lines.append("/* stripped upstream developer lint macro */\n")
            continue
        out_lines.append(line)

    s = "".join(out_lines)
    if mod_name == "libregexp.c":
        s = re.sub(r"\bcompute_stack_size\b", "re_compute_stack_size", s)
        s = re.sub(r"\bis_digit\b", "re_is_digit", s)
    elif mod_name == "libbf.c":
        s = re.sub(r"\bfloor_div\b", "bf_floor_div", s)
        s = re.sub(r"\bto_digit\b", "bf_to_digit", s)
    return s


def ensure_quickjs_sources(qjs_dir: str = QJS_DIR) -> None:
    """Verify pinned QuickJS 2024-01-13 sources on disk, fetching and verifying tarball if absent."""
    os.makedirs(qjs_dir, exist_ok=True)
    all_present = all(os.path.isfile(os.path.join(qjs_dir, fn)) for fn in UPSTREAM_FILE_SHA256)
    if not all_present:
        print(f"[FETCH] Downloading upstream QuickJS 2024-01-13 from {UPSTREAM_TARBALL_URL}...", flush=True)
        with tempfile.NamedTemporaryFile(suffix=".tar.xz", delete=False) as tf:
            tmp_xz = tf.name
        try:
            with urllib.request.urlopen(UPSTREAM_TARBALL_URL, timeout=60) as resp:
                with open(tmp_xz, "wb") as out_f:
                    shutil.copyfileobj(resp, out_f)
            got_tar_sha = sha256_file(tmp_xz)
            if got_tar_sha != UPSTREAM_TARBALL_SHA256:
                raise RuntimeError(
                    f"QuickJS tarball SHA-256 mismatch: expected {UPSTREAM_TARBALL_SHA256}, got {got_tar_sha}"
                )
            with tarfile.open(tmp_xz, mode="r:xz") as tf_xz:
                for fn in UPSTREAM_FILE_SHA256:
                    member = tf_xz.getmember(f"quickjs-2024-01-13/{fn}")
                    src_stream = tf_xz.extractfile(member)
                    if src_stream is None:
                        raise RuntimeError(f"Tarball missing member quickjs-2024-01-13/{fn}")
                    with open(os.path.join(qjs_dir, fn), "wb") as dst_f:
                        shutil.copyfileobj(src_stream, dst_f)
        finally:
            if os.path.exists(tmp_xz):
                os.remove(tmp_xz)

    for fn, expected_sha in UPSTREAM_FILE_SHA256.items():
        full = os.path.join(qjs_dir, fn)
        if not os.path.isfile(full):
            raise RuntimeError(f"Missing QuickJS source file after ensure_quickjs_sources: {full}")
        got_sha = sha256_file(full)
        if got_sha != expected_sha:
            raise RuntimeError(f"SHA-256 mismatch for {full}: expected {expected_sha}, got {got_sha}")


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    ensure_quickjs_sources(QJS_DIR)
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    local_hdrs = {f for f in os.listdir(QJS_DIR) if f.endswith(".h")}

    parts = [
        "/* Auto-generated Upstream QuickJS 2024-01-13 WASI Bundle (zero-binutils, zero env.* imports) */\n",
        "#define EMSCRIPTEN 1\n",
        '#define CONFIG_VERSION "2024-01-13"\n',
        "#define CONFIG_BIGNUM 1\n",
        "#define __builtin_expect(x, v) (x)\n",
        '#include "src/wasm_wasi_rt.c"\n',
        "#ifndef INTPTR_MAX\n#define INTPTR_MAX 2147483647\n#endif\n",
        "#ifndef INT64_MAX\n#define INT64_MAX 9223372036854775807LL\n#endif\n\n",
    ]

    for mod in QJS_CORE_MODULES:
        full = os.path.join(QJS_DIR, mod)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing upstream QuickJS file: {full}")
        with open(full, "r", encoding="utf-8") as f:
            raw = f.read()
        parts.append(f"/* ===== BEGIN tests/embedded_quickjs/{mod} ===== */\n")
        parts.append(f'#line 1 "tests/embedded_quickjs/{mod}"\n')
        parts.append(sanitize_qjs_module(mod, raw, local_hdrs))
        parts.append(f"\n/* ===== END tests/embedded_quickjs/{mod} ===== */\n\n")

    drv = os.path.join(REPO_ROOT, "src", "wasm_qjs_driver.c")
    if not os.path.isfile(drv):
        raise FileNotFoundError(f"Missing QuickJS WASI driver: {drv}")
    parts.append('#include "src/wasm_qjs_driver.c"\n')

    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(parts)
    return bundle_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone QuickJS 2024-01-13 WASI module via zcc.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--probe-wasm-selfcompile", action="store_true")
    args = parser.parse_args()

    node_bin = find_node_bin()
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")

    bundle_path = generate_bundle(args.bundle)
    bundle_rel = os.path.relpath(bundle_path, REPO_ROOT).replace("\\", "/")
    out_rel = os.path.relpath(args.output, REPO_ROOT).replace("\\", "/")
    bundle_sz = os.path.getsize(bundle_path)
    print(f"[BUNDLE] {bundle_rel}: {bundle_sz} bytes ({len(QJS_CORE_MODULES)} upstream QuickJS modules)", flush=True)

    cmd = [
        ZCC_BIN,
        "--target=wasm32",
        "-Itests/embedded_quickjs",
        "-Isrc",
        "-I.",
        bundle_rel,
        "-o",
        out_rel,
    ]
    print(f"[BUILD] {' '.join(cmd)}", flush=True)
    res = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True)
    if res.returncode != 0:
        sys.stderr.write(res.stdout + res.stderr)
        return res.returncode

    info = inspect_wasm_v8(node_bin, args.output)
    sha = sha256_file(args.output)
    print(f"[WASM] {args.output}: {info['bytes']} bytes, SHA256={sha}", flush=True)
    print(f"[V8] valid={info['valid']} compiled={info['compiled']} err={info['err']}", flush=True)
    non_wasi = [imp for imp in info["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
    print(f"[IMPORTS] count={len(info['imports'])} non_wasi={non_wasi}", flush=True)
    if not info["valid"] or not info["compiled"] or non_wasi:
        return 1

    if args.probe_wasm_selfcompile:
        if not os.path.isfile(ZCC_WASM):
            raise RuntimeError("build_wasm/zcc.wasm missing; run `python3 tests/test_wasm_wasi_selfhost.py` first.")
        probe_out = os.path.join(BUILD_WASM, "qjs_inwasi_probe.wasm")
        probe_rel = os.path.relpath(probe_out, REPO_ROOT).replace("\\", "/")
        wasm_cmd = [
            "zcc.wasm",
            "--target=wasm32",
            "-Itests/embedded_quickjs",
            "-Isrc",
            "-I.",
            bundle_rel,
            "-o",
            probe_rel,
        ]
        print(f"[PROBE-WASM] Invoking build_wasm/zcc.wasm on {bundle_rel}...", flush=True)
        try:
            pres = run_node_wasi(node_bin, ZCC_WASM, wasm_cmd, timeout=240)
            if pres.returncode == 0 and os.path.isfile(probe_out):
                pinfo = inspect_wasm_v8(node_bin, probe_out)
                psha = sha256_file(probe_out)
                p_non_wasi = [imp for imp in pinfo["imports"] if not imp.startswith("wasi_snapshot_preview1.")]
                exec_res = run_node_wasi(node_bin, probe_out, ["qjs.wasm", "-p", "(2n**64n) + 1n"], timeout=60)
                exec_ok = (exec_res.returncode == 0 and exec_res.stdout.strip() == "18446744073709551617")
                print(
                    f"[PROBE-WASM] rc=0 bytes={pinfo['bytes']} valid={pinfo['valid']} "
                    f"compiled={pinfo['compiled']} non_wasi={p_non_wasi} exec_ok={exec_ok} "
                    f"SHA256={psha} byte_identical={psha == sha}",
                    flush=True,
                )
                if not pinfo["valid"] or not pinfo["compiled"] or p_non_wasi or not exec_ok:
                    return 1
            else:
                print(f"[PROBE-WASM] rc={pres.returncode} err={(pres.stdout + pres.stderr)[:400]}", flush=True)
                return 1
        finally:
            if os.path.exists(probe_out):
                os.remove(probe_out)

    print("[PASS] Standalone build_wasm/qjs.wasm validated with zero env.* imports!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
