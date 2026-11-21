#!/usr/bin/env python3
"""
Standalone Upstream DOOM 1.10 Native Win64 GUI + Headless Builder (`build_win64/doom.exe`)
==========================================================================================
Compiles all 57 strictly unmodified upstream `doom/*.c` core modules (from `doom/Makefile`,
excluding Linux/X11 `i_*.c`) with `src/win64_app_crt.c`, `src/win64_c99_rt.c`, and
`src/win64_doom_platform.c` with zero MSVC/MinGW/binutils via BOTH:
  1. Host `./zcc` -> `build_win64/doom_host_zcc.exe`
  2. Native Windows sovereign `build_win64/zcc_sov_stage3.exe` -> `build_win64/doom.exe`
Verifies SHA-256 + Windows `fc /b` byte-for-byte identity, PE32+ headers, and
`C:\\Windows\\System32\\{msvcrt,kernel32,user32,gdi32}.dll` export resolution.
"""

import argparse
import os
import re
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from build_doom_wasm import (  # noqa: E402
    DOOM_CORE_MODULES,
    DOOM_DIR,
    DOOM_SYS_HEADER_QUOTED,
    STRIP_SYS_HEADERS,
)
from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    MSVCRT_PATH,
    REPO_ROOT,
    ZCC_BIN,
    ZCC_WIN64,
    compile_and_link_win64_units,
    parse_pe_exports,
    run_fc_b,
    sha256_file,
    to_rel_posix,
)

DEFAULT_BUNDLE = os.path.join(BUILD_DIR, "doom_win64_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_DIR, "doom.exe")
DEFAULT_HOST_OUT = os.path.join(BUILD_DIR, "doom_host_zcc.exe")

SYSTEM32_DLL_PATHS = {
    "msvcrt.dll": MSVCRT_PATH,
    "KERNEL32.dll": "/mnt/c/Windows/System32/kernel32.dll",
    "USER32.dll": "/mnt/c/Windows/System32/user32.dll",
    "GDI32.dll": "/mnt/c/Windows/System32/gdi32.dll",
}

WIN64_DOOM_PRELUDE = """/* Auto-generated Upstream DOOM 1.10 Native Win64 Dual-Mode Bundle */
#define NORMALUNIX 1
#define LINUX 1
#define __BYTEBOOL__ 1
typedef int boolean;
typedef unsigned char byte;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <alloca.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

extern int _win64_posix_open(const char *path, int flags, ...);
extern int _win64_posix_close(int fd);
extern int _win64_posix_read(int fd, void *buf, unsigned int count);
extern int _win64_posix_write(int fd, const void *buf, unsigned int count);
extern long _win64_posix_lseek(int fd, long offset, int whence);
extern int _win64_posix_fstat_size(int fd, long long *out_size);
extern int _win64_posix_mkdir(const char *path, int mode);
extern int _win64_posix_access(const char *path, int mode);
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
extern int feof(FILE *stream);
extern int fscanf(FILE *stream, const char *format, ...);

static int _doom_posix_open(const char *path, int flags, ...) {
    return _win64_posix_open(path, flags, 0666);
}
static int _doom_posix_close(int fd) {
    return _win64_posix_close(fd);
}
static int _doom_posix_read(int fd, void *buf, size_t count) {
    return _win64_posix_read(fd, buf, (unsigned int)count);
}
static int _doom_posix_write(int fd, const void *buf, size_t count) {
    return _win64_posix_write(fd, buf, (unsigned int)count);
}
static long _doom_posix_lseek(int fd, long offset, int whence) {
    return _win64_posix_lseek(fd, offset, whence);
}
static int _doom_posix_fstat(int fd, struct stat *st) {
    long long sz = 0;
    int rc = _win64_posix_fstat_size(fd, &sz);
    if (rc == 0 && st) {
        memset(st, 0, sizeof(*st));
        st->st_size = (long)sz;
    }
    return rc;
}
static int _doom_posix_mkdir(const char *path, int mode) {
    return _win64_posix_mkdir(path, mode);
}
static int _doom_posix_access(const char *path, int mode) {
    return _win64_posix_access(path, mode);
}

static char *_doom_win64_getenv(const char *name) {
    char *v = getenv(name);
    if (!v && name && (strcmp(name, "HOME") == 0 || strcmp(name, "DOOMWADDIR") == 0)) return ".";
    return v;
}
#define getenv _doom_win64_getenv

"""


def audit_pe32plus_win32_dlls(exe_path: str, required_dlls: tuple[str, ...] = ("msvcrt.dll", "KERNEL32.dll", "USER32.dll", "GDI32.dll")) -> dict:
    """Validate PE32+ headers, .pdata/.reloc tables, and real System32 DLL export resolution."""
    with open(exe_path, "rb") as f:
        data = f.read()

    if len(data) < 0x200 or data[:2] != b"MZ":
        raise AssertionError(f"Missing MZ signature in {exe_path}")
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_off + 0x110 > len(data) or data[pe_off : pe_off + 4] != b"PE\x00\x00":
        raise AssertionError(f"Invalid PE signature in {exe_path}")

    machine, num_sec, _, _, _, opt_sz, _ = struct.unpack_from("<HHIIIHH", data, pe_off + 4)
    if machine != 0x8664:
        raise AssertionError(f"Expected AMD64 machine 0x8664, got 0x{machine:x}")
    opt_off = pe_off + 24
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x20B:
        raise AssertionError(f"Expected PE32+ magic 0x20B, got 0x{magic:x}")

    stack_reserve = struct.unpack_from("<Q", data, opt_off + 72)[0]
    if stack_reserve < 0x1000000:
        raise AssertionError(f"Expected SizeOfStackReserve >= 0x1000000, got 0x{stack_reserve:x}")

    dd_off = opt_off + 112
    imp_rva, _ = struct.unpack_from("<II", data, dd_off + 1 * 8)
    exc_rva, exc_sz = struct.unpack_from("<II", data, dd_off + 3 * 8)
    rel_rva, rel_sz = struct.unpack_from("<II", data, dd_off + 5 * 8)
    if exc_rva == 0 or exc_sz == 0:
        raise AssertionError(".pdata exception directory is empty")
    if rel_rva == 0 or rel_sz == 0:
        raise AssertionError(".reloc directory is empty")

    secs = []
    sec_off = opt_off + opt_sz
    for i in range(num_sec):
        s_off = sec_off + i * 40
        vsz, va, rsz, rp = struct.unpack_from("<IIII", data, s_off + 8)
        secs.append((va, max(vsz, rsz), rp))

    def rva_to_off(rva: int) -> int:
        for va, span, rp in secs:
            if va <= rva < va + span:
                return rp + (rva - va)
        raise AssertionError(f"Unmapped RVA 0x{rva:x} in {exe_path}")

    def read_cstr_rva(rva: int) -> str:
        off = rva_to_off(rva)
        end = data.index(b"\x00", off)
        return data[off:end].decode("ascii")

    dll_exports = {}
    for dll_name in required_dlls:
        dll_path = SYSTEM32_DLL_PATHS[dll_name]
        if not os.path.isfile(dll_path):
            raise FileNotFoundError(f"Missing Windows System32 DLL for export check: {dll_path}")
        dll_exports[dll_name] = parse_pe_exports(dll_path)

    dlls = {}
    if imp_rva != 0:
        cur = rva_to_off(imp_rva)
        while True:
            orig_first, _, _, name_rva, first_thunk = struct.unpack_from("<IIIII", data, cur)
            if orig_first == 0 and name_rva == 0 and first_thunk == 0:
                break
            dll_name = read_cstr_rva(name_rva)
            if dll_name not in required_dlls:
                raise AssertionError(f"Unexpected Win32 DLL import {dll_name!r}; allowed={list(required_dlls)}")
            exports = dll_exports[dll_name]
            thunk_rva = orig_first if orig_first != 0 else first_thunk
            t_off = rva_to_off(thunk_rva)
            funcs = []
            while True:
                val = struct.unpack_from("<Q", data, t_off)[0]
                if val == 0:
                    break
                if val & (1 << 63):
                    raise AssertionError(f"Unexpected ordinal import 0x{val:x} from {dll_name}")
                fn = read_cstr_rva(int(val + 2))
                if fn not in exports:
                    raise AssertionError(f"Imported symbol {dll_name}!{fn} missing from {SYSTEM32_DLL_PATHS[dll_name]}")
                funcs.append(fn)
                t_off += 8
            dlls[dll_name] = funcs
            cur += 20

    for req in required_dlls:
        if req not in dlls or not dlls[req]:
            raise AssertionError(f"Expected non-empty import descriptor for {req}, found={list(dlls.keys())}")

    return {
        "bytes": len(data),
        "pdata": exc_sz,
        "reloc": rel_sz,
        "stack_reserve": stack_reserve,
        "dlls": dlls,
    }


def sanitize_doom_module_win64(mod_name: str, src: str) -> str:
    """Adapt an upstream DOOM 1.10 .c module for 64-bit Win64 single-TU compilation without modifying doom/."""
    stem = mod_name[:-2]
    src = src.replace("\r\n", "\n").replace("\r", "\n")

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
        if re.match(r"^\s*#\s*define\s+O_BINARY\b", line):
            out_lines.append("/* using global O_BINARY */\n")
            continue
        out_lines.append(line)
    src = "".join(out_lines)

    src = re.sub(r"\brcsid\b", f"rcsid_{stem}", src)
    src = re.sub(
        r"struct\s+stat\s*\{[^}]*\}\s*;",
        "/* using struct stat from stdio/win64 prelude */",
        src,
        flags=re.S,
    )
    src = re.sub(
        r"^\s*(?:int|void)\s+(?:open|close|read|write|fstat|lseek|access|mkdir|strcasecmp|strncasecmp|toupper|setbuf|feof|fscanf)\s*\([^;]*\)\s*;\s*$",
        "/* forward decl supplied by win64 prelude */",
        src,
        flags=re.M,
    )

    # 64-bit Win64 pointer / alignment fixes applied strictly in memory
    if mod_name == "r_data.c":
        src = src.replace(
            "void\t\t**columndirectory;\t// OBSOLETE",
            "int\t\t\tcolumndirectory;\t// OBSOLETE (32-bit WAD layout)",
        )
        src = src.replace(
            "textures = Z_Malloc (numtextures*4, PU_STATIC, 0);",
            "textures = Z_Malloc (numtextures * sizeof(*textures), PU_STATIC, 0);",
        )
        src = src.replace(
            "texturecolumnlump = Z_Malloc (numtextures*4, PU_STATIC, 0);",
            "texturecolumnlump = Z_Malloc (numtextures * sizeof(*texturecolumnlump), PU_STATIC, 0);",
        )
        src = src.replace(
            "texturecolumnofs = Z_Malloc (numtextures*4, PU_STATIC, 0);",
            "texturecolumnofs = Z_Malloc (numtextures * sizeof(*texturecolumnofs), PU_STATIC, 0);",
        )
        src = src.replace(
            "texturecomposite = Z_Malloc (numtextures*4, PU_STATIC, 0);",
            "texturecomposite = Z_Malloc (numtextures * sizeof(*texturecomposite), PU_STATIC, 0);",
        )
        src = src.replace(
            "colormaps = (byte *)( ((int)colormaps + 255)&~0xff);",
            "colormaps = (byte *)( ((uintptr_t)colormaps + 255) & ~(uintptr_t)0xff);",
        )
    elif mod_name == "p_setup.c":
        src = src.replace(
            "linebuffer = Z_Malloc (total*4, PU_LEVEL, 0);",
            "linebuffer = Z_Malloc (total * sizeof(*linebuffer), PU_LEVEL, 0);",
        )
    elif mod_name == "r_draw.c":
        src = src.replace(
            "translationtables = (byte *)(( (int)translationtables + 255 )& ~255);",
            "translationtables = (byte *)( ((uintptr_t)translationtables + 255) & ~(uintptr_t)255);",
        )
    elif mod_name == "p_saveg.c":
        src = src.replace(
            "#define PADSAVEP()\tsave_p += (4 - ((int) save_p & 3)) & 3",
            "#define PADSAVEP()\tsave_p += (4 - ((uintptr_t) save_p & 3)) & 3",
        )
        src = re.sub(
            r"\(int\)\s*(?=(?:players|mobj|ceiling|door|floor|plat|flash|strobe|glow)\b)",
            "(int)(intptr_t)",
            src,
        )
    elif mod_name == "d_net.c":
        src = src.replace("(int)&(((doomdata_t *)0)->", "(int)(intptr_t)&(((doomdata_t *)0)->")
    elif mod_name == "z_zone.c":
        src = src.replace("size = (size + 3) & ~3;", "size = (size + 7) & ~7;")
    elif mod_name == "m_misc.c":
        src = re.sub(
            r"int\s*\*\s*location\s*;\s*int\s+defaultvalue\s*;",
            "void*\tlocation;\n    intptr_t\tdefaultvalue;",
            src,
            count=1,
        )
        src = re.sub(r"\(\s*int\s*\*\s*\)", "(void *)", src)
        src = re.sub(r"\(\s*int\s*\)\s*(?=\"|HUSTR_)", "(intptr_t) ", src)
        src = src.replace("v = *defaults[i].location;", "v = *(int *)defaults[i].location;")
        src = src.replace(
            "*defaults[i].location = defaults[i].defaultvalue;",
            "if (defaults[i].defaultvalue > -0xfff && defaults[i].defaultvalue < 0xfff) "
            "*(int *)defaults[i].location = (int)defaults[i].defaultvalue; "
            "else *(char **)defaults[i].location = (char *)(intptr_t)defaults[i].defaultvalue;",
        )
        src = src.replace("*defaults[i].location = parm;", "*(int *)defaults[i].location = parm;")
        src = re.sub(
            r"\*defaults\[i\]\.location\s*=\s*\(\s*int\s*\)\s*newstring\s*;",
            "*(char **)defaults[i].location = newstring;",
            src,
        )

    if mod_name in ("w_wad.c", "m_misc.c", "m_menu.c", "d_main.c"):
        src = (
            "#define open _doom_posix_open\n"
            "#define close _doom_posix_close\n"
            "#define read _doom_posix_read\n"
            "#define write _doom_posix_write\n"
            "#define lseek _doom_posix_lseek\n"
            "#define fstat _doom_posix_fstat\n"
            "#define mkdir _doom_posix_mkdir\n"
            "#define access _doom_posix_access\n"
            + src
            + "\n#undef open\n#undef close\n#undef read\n#undef write\n#undef lseek\n#undef fstat\n#undef mkdir\n#undef access\n"
        )
    if mod_name == "d_main.c":
        src = re.sub(
            r"\bvoid\s+D_DoomLoop\s*\(\s*void\s*\)\s*\{",
            "void D_DoomLoop_Unused_Upstream(void) {",
            src,
            count=1,
        )
    elif mod_name == "p_spec.c":
        src = "#define anim_t p_spec_anim_t\n#define anims p_spec_anims\n" + src + "\n#undef anim_t\n#undef anims\n"
    elif mod_name == "wi_stuff.c":
        src = "#define anim_t wi_anim_t\n#define anims wi_anims\n" + src + "\n#undef anim_t\n#undef anims\n"
    elif mod_name == "hu_stuff.c":
        src = "#define plr hu_plr\n" + src + "\n#undef plr\n"
    elif mod_name == "m_menu.c":
        src = "#define y mm_menu_y\n" + src + "\n#undef y\n"
    elif mod_name == "f_wipe.c":
        src = "#define y fw_wipe_y\n" + src + "\n#undef y\n"
    elif mod_name == "r_things.c":
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
    parts = [WIN64_DOOM_PRELUDE]
    for mod in DOOM_CORE_MODULES:
        full = os.path.join(DOOM_DIR, mod)
        if not os.path.isfile(full):
            raise FileNotFoundError(f"Missing DOOM source file: {full}")
        with open(full, "r", encoding="latin1") as f:
            raw = f.read()
        parts.append(f"/* ===== BEGIN doom/{mod} ===== */\n")
        parts.append(f'#line 1 "doom/{mod}"\n')
        parts.append(sanitize_doom_module_win64(mod, raw))
        parts.append(f"\n/* ===== END doom/{mod} ===== */\n\n")

    parts.append("#undef getenv\n")
    plat = os.path.join(REPO_ROOT, "src", "win64_doom_platform.c")
    if not os.path.isfile(plat):
        raise FileNotFoundError(f"Missing DOOM Win64 platform file: {plat}")
    parts.append('#include "src/win64_doom_platform.c"\n')

    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(parts)
    return bundle_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone DOOM 1.10 Win64 GUI + Headless PE32+ executable cleanly via ZCC.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--host-output", default=DEFAULT_HOST_OUT)
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()

    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found; run `make zcc` first.")
    bundle_path = generate_bundle(args.bundle)
    bundle_rel = to_rel_posix(bundle_path)
    print(
        f"[BUNDLE] {bundle_rel}: {os.path.getsize(bundle_path)} bytes ({len(DOOM_CORE_MODULES)} upstream DOOM modules)",
        flush=True,
    )

    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        ("doom_bundle.s", bundle_rel, ["-Idoom", "-Iinclude", "-I."]),
    ]

    host_dir = os.path.join(BUILD_DIR, "doom_host_asm")
    host_out = args.output if args.skip_win64_selfcompile else args.host_output
    print(f"[BUILD-HOST] ./zcc -mabi=ms -> {to_rel_posix(host_out)}", flush=True)
    compile_and_link_win64_units(units, [], host_dir, host_out, use_win64_compiler=False, timeout_s=args.timeout)
    host_sha = sha256_file(host_out)
    print(f"[HOST-PE] {host_out}: {os.path.getsize(host_out)} bytes, SHA256={host_sha}", flush=True)

    if not args.skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        win_dir = os.path.join(BUILD_DIR, "doom_win64_asm")
        print(f"[BUILD-WIN64] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(args.output)}", flush=True)
        compile_and_link_win64_units(units, [], win_dir, args.output, use_win64_compiler=True, timeout_s=args.timeout)
        win_sha = sha256_file(args.output)
        print(f"[WIN64-PE] {args.output}: {os.path.getsize(args.output)} bytes, SHA256={win_sha}", flush=True)
        fc = run_fc_b(host_out, args.output)
        print(f"[FC /B] {fc.stdout.strip()}", flush=True)
        if host_sha != win_sha or fc.returncode != 0 or "no differences encountered" not in fc.stdout.lower():
            raise AssertionError(f"Parity mismatch between ./zcc and zcc_sov_stage3.exe: {host_sha} vs {win_sha}")
        print("[PARITY] Byte-identical DOOM Win64 .exe produced by BOTH ./zcc and build_win64/zcc_sov_stage3.exe!", flush=True)

    audit = audit_pe32plus_win32_dlls(args.output)
    dll_summary = ", ".join(f"{k}({len(v)})" for k, v in audit["dlls"].items())
    print(
        f"[AUDIT] PE32+ OK: bytes={audit['bytes']} .pdata={audit['pdata']}B .reloc={audit['reloc']}B "
        f"StackReserve=0x{audit['stack_reserve']:x} Imports=[{dll_summary}]",
        flush=True,
    )
    print("[PASS] Standalone build_win64/doom.exe validated cleanly (zero MSVC/MinGW/binutils)!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
