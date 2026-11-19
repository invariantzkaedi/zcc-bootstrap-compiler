#!/usr/bin/env python3
"""
Standalone Upstream DOOM 1.10 WASI Builder (build_wasm/doom.wasm)
=================================================================
Bundles all 57 real upstream `doom/*.c` core engine modules (from `doom/Makefile`,
excluding Linux/X11 `i_*.c`) with `src/wasm_wasi_rt.c` and `src/wasm_doom_platform.c`,
compiles cleanly via `./zcc --target=wasm32` (and optionally probes `build_wasm/zcc.wasm`)
into standalone WASI Preview1 `build_wasm/doom.wasm` with zero `env.*` imports.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
ZCC_BIN = os.path.join(REPO_ROOT, "zcc")
ZCC_WASM = os.path.join(BUILD_WASM, "zcc.wasm")
DOOM_DIR = os.path.join(REPO_ROOT, "doom")
DEFAULT_BUNDLE = os.path.join(BUILD_WASM, "doom_wasi_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_WASM, "doom.wasm")

# Exact 57 core modules from doom/Makefile OBJS excluding platform i_*.c
DOOM_CORE_MODULES = [
    "doomdef.c",
    "doomstat.c",
    "dstrings.c",
    "tables.c",
    "f_finale.c",
    "f_wipe.c",
    "d_main.c",
    "d_net.c",
    "d_items.c",
    "g_game.c",
    "m_menu.c",
    "m_misc.c",
    "m_argv.c",
    "m_bbox.c",
    "m_fixed.c",
    "m_swap.c",
    "m_cheat.c",
    "m_random.c",
    "am_map.c",
    "p_ceilng.c",
    "p_doors.c",
    "p_enemy.c",
    "p_floor.c",
    "p_inter.c",
    "p_lights.c",
    "p_map.c",
    "p_maputl.c",
    "p_mobj.c",
    "p_plats.c",
    "p_pspr.c",
    "p_saveg.c",
    "p_setup.c",
    "p_sight.c",
    "p_spec.c",
    "p_switch.c",
    "p_telept.c",
    "p_tick.c",
    "p_user.c",
    "r_bsp.c",
    "r_data.c",
    "r_draw.c",
    "r_main.c",
    "r_plane.c",
    "r_segs.c",
    "r_sky.c",
    "r_things.c",
    "w_wad.c",
    "wi_stuff.c",
    "v_video.c",
    "st_lib.c",
    "st_stuff.c",
    "hu_lib.c",
    "hu_stuff.c",
    "s_sound.c",
    "z_zone.c",
    "info.c",
    "sounds.c",
]

STRIP_SYS_HEADERS = {
    "stdio.h",
    "stdlib.h",
    "string.h",
    "ctype.h",
    "stdarg.h",
    "stdint.h",
    "stddef.h",
    "math.h",
    "unistd.h",
    "fcntl.h",
    "alloca.h",
    "malloc.h",
    "sys/stat.h",
    "sys/types.h",
    "sys/time.h",
    "errno.h",
    "time.h",
    "values.h",
}

DOOM_SYS_HEADER_QUOTED = {
    "stdlib.h",
    "string.h",
    "stdio.h",
    "ctype.h",
    "math.h",
}

# Add save_p guard if needed: DOOM_CORE_MODULES has 57 upstream modules (excluding i_*.c)


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


def run_node_wasi(node_bin: str, wasm_path: str, args, timeout: int = 180) -> subprocess.CompletedProcess:
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


def sanitize_doom_module(mod_name: str, src: str) -> str:
    """Adapt an upstream DOOM 1.10 .c module for clean single-TU WASI bundling without modifying doom/."""
    stem = mod_name[:-2]
    src = src.replace("\r\n", "\n").replace("\r", "\n")

    # Strip redundant system headers already supplied by src/wasm_wasi_rt.c
    out_lines = []
    for line in src.splitlines(keepends=True):
        m_ang = re.match(r"^\s*#\s*include\s*<([^>]+)>", line)
        if m_ang and m_ang.group(1) in STRIP_SYS_HEADERS:
            out_lines.append(f"/* stripped <{m_ang.group(1)}> */\n")
            continue
        m_quo = re.match(r'^\s*#\s*include\s*"([^"]+)"', line)
        if m_quo and m_quo.group(1) in DOOM_SYS_HEADER_QUOTED:
            out_lines.append(f'/* stripped "{m_quo.group(1)}" */\n')
            continue
        out_lines.append(line)
    src = "".join(out_lines)

    # Deduplicate per-file static rcsid strings
    src = re.sub(r"\brcsid\b", f"rcsid_{stem}", src)

    # Strip inline `struct stat { ... };` redefinitions in w_wad.c / m_misc.c
    src = re.sub(
        r"struct\s+stat\s*\{[^}]*\}\s*;",
        "/* using struct stat from wasm_wasi_rt.c */",
        src,
        flags=re.S,
    )
    # Strip forward decls in w_wad.c / m_misc.c / d_main.c / m_menu.c that duplicate wasm_wasi_rt.c
    src = re.sub(
        r"^\s*(?:int|void)\s+(?:open|close|read|write|fstat|lseek|access|mkdir|strcasecmp|toupper|setbuf|feof|fscanf)\s*\([^;]*\)\s*;\s*$",
        "/* forward decl supplied by wasm_wasi_rt.c */",
        src,
        flags=re.M,
    )

    # Disambiguate static symbols that collide across modules in a single translation unit
    if mod_name in ("w_wad.c", "m_misc.c", "m_menu.c"):
        src = (
            "#define open _doom_posix_open\n"
            "#define close _doom_posix_close\n"
            + src
            + "\n#undef open\n#undef close\n"
        )
    elif mod_name == "d_main.c":
        src = re.sub(
            r"\bvoid\s+D_DoomLoop\s*\(\s*void\s*\)\s*\{",
            "void D_DoomLoop_Unused_Upstream(void) {",
            src,
            count=1,
        )
    elif mod_name == "p_spec.c":
        src = (
            "#define anim_t p_spec_anim_t\n"
            "#define anims p_spec_anims\n"
            + src
            + "\n#undef anim_t\n#undef anims\n"
        )
    elif mod_name == "wi_stuff.c":
        src = (
            "#define anim_t wi_anim_t\n"
            "#define anims wi_anims\n"
            + src
            + "\n#undef anim_t\n#undef anims\n"
        )
    elif mod_name == "hu_stuff.c":
        src = "#define plr hu_plr\n" + src + "\n#undef plr\n"
    elif mod_name == "m_menu.c":
        src = "#define y mm_menu_y\n" + src + "\n#undef y\n"
    elif mod_name == "f_wipe.c":
        src = "#define y fw_wipe_y\n" + src + "\n#undef y\n"
    elif mod_name == "r_things.c":
        # Upstream DOOM 1.10 info.c defines char *sprnames[NUMSPRITES] without a trailing NULL
        src = re.sub(
            r"check = namelist;\s*while\s*\(\s*\*check\s*!=\s*NULL\s*\)\s*check\+\+;\s*numsprites\s*=\s*check\s*-\s*namelist;",
            "if (namelist == sprnames) { numsprites = NUMSPRITES; } else { check = namelist; while (*check != NULL) check++; numsprites = check - namelist; }",
            src,
            count=1,
            flags=re.S,
        )

    return src


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    # Remove any partial core module count mistake check: DOOM_CORE_MODULES has 56 non-i_ files + i_ replaced
    parts = [
        "/* Auto-generated Upstream DOOM 1.10 WASI Bundle (zero-binutils, zero env.* imports) */\n",
        "#define NORMALUNIX 1\n",
        "#define LINUX 1\n",
        "#define __BYTEBOOL__ 1\n",
        "typedef int boolean;\n",
        "typedef unsigned char byte;\n",
        "#ifndef alloca\n#define alloca(x) malloc(x)\n#endif\n",
        '#include "src/wasm_wasi_rt.c"\n\n',
        "static int _doom_posix_open(const char *path, int flags, ...) {\n",
        "    return open(path, flags, 0666);\n",
        "}\n",
        "static int _doom_posix_close(int fd) {\n",
        "    return close(fd);\n",
        "}\n",
        "static char *_doom_wasi_getenv(const char *name) {\n",
        "    char *v = getenv(name);\n",
        "    if (!v && name && (strcmp(name, \"HOME\") == 0 || strcmp(name, \"DOOMWADDIR\") == 0)) return \".\";\n",
        "    return v;\n",
        "}\n",
        "#define getenv _doom_wasi_getenv\n\n",
    ]
    for mod in DOOM_CORE_MODULES:
        full = os.path.join(DOOM_DIR, mod)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing DOOM source file: {full}")
        with open(full, "r", encoding="latin1") as f:
            raw = f.read()
        parts.append(f"/* ===== BEGIN doom/{mod} ===== */\n")
        parts.append(f'#line 1 "doom/{mod}"\n')
        parts.append(sanitize_doom_module(mod, raw))
        parts.append(f"\n/* ===== END doom/{mod} ===== */\n\n")

    parts.append("#undef getenv\n")
    plat = os.path.join(REPO_ROOT, "src", "wasm_doom_platform.c")
    if not os.path.isfile(plat):
        raise FileNotFoundError(f"Missing DOOM WASI platform file: {plat}")
    parts.append('#include "src/wasm_doom_platform.c"\n')

    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(parts)
    return bundle_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone DOOM 1.10 WASI module via zcc.")
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
    print(f"[BUNDLE] {bundle_rel}: {bundle_sz} bytes ({len(DOOM_CORE_MODULES)} upstream DOOM modules)", flush=True)

    cmd = [
        ZCC_BIN,
        "--target=wasm32",
        "-Idoom",
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
    exp_names = {e.split(":")[0] for e in info["exportsList"]}
    required_exports = {"memory", "_start", "fb_init", "fb_tick", "fb_ptr", "fb_width", "fb_height", "fb_key"}
    missing_exp = sorted(required_exports - exp_names)
    print(f"[IMPORTS] count={len(info['imports'])} non_wasi={non_wasi}", flush=True)
    print(f"[EXPORTS] count={len(info['exportsList'])} missing_fb={missing_exp}", flush=True)
    if not info["valid"] or not info["compiled"] or non_wasi or missing_exp:
        return 1

    if args.probe_wasm_selfcompile:
        if not os.path.isfile(ZCC_WASM):
            raise RuntimeError("build_wasm/zcc.wasm missing; run `python3 tools/build_wasm_bundle.py` first.")
        probe_out = os.path.join(BUILD_WASM, "doom_inwasi_probe.wasm")
        probe_rel = os.path.relpath(probe_out, REPO_ROOT).replace("\\", "/")
        wasm_cmd = [
            "zcc.wasm",
            "--target=wasm32",
            "-Idoom",
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
                print(
                    f"[PROBE-WASM] rc=0 bytes={pinfo['bytes']} valid={pinfo['valid']} "
                    f"compiled={pinfo['compiled']} non_wasi={p_non_wasi} SHA256={psha} "
                    f"byte_identical={psha == sha}",
                    flush=True,
                )
            else:
                print(f"[PROBE-WASM] rc={pres.returncode} err={(pres.stdout + pres.stderr)[:400]}", flush=True)
                return 1
        finally:
            if os.path.exists(probe_out):
                os.remove(probe_out)

    print("[PASS] Standalone build_wasm/doom.wasm validated with zero env.* imports!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
