#!/usr/bin/env python3
"""
Standalone Upstream MicroPython v1.24.1 Native Win64 PE32+ Builder (`build_win64/micropython.exe`)
====================================================================================================
Compiles strictly unmodified upstream MicroPython v1.24.1 core C sources
(`tests/embedded_micropython/{py,extmod,lib/re1.5}`) with `src/win64_app_crt.c`,
`src/win64_c99_rt.c`, `src/win64_setjmp.s` (powering `MICROPY_NLR_SETJMP`), and
`src/win64_mpy_driver.c` with zero MSVC/MinGW/binutils via BOTH:
  1. Host `./zcc` -> `build_win64/micropython_host_zcc.exe`
  2. Native Windows sovereign `build_win64/zcc_sov_stage3.exe` -> `build_win64/micropython.exe`
Verifies SHA-256 + Windows `fc /b` byte-for-byte identity, PE32+ headers, and
`C:\\Windows\\System32\\msvcrt.dll` export resolution.
"""

import argparse
import os
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    REPO_ROOT,
    ZCC_BIN,
    ZCC_WIN64,
    audit_pe32plus_msvcrt,
    compile_and_link_win64_units,
    run_fc_b,
    sha256_file,
    to_rel_posix,
)
from build_micropython_wasm import (  # noqa: E402
    MPY_DIR,
    SPECIAL_PRE_QSTRS,
    SPECIAL_QSTR_ENCODED_NAMES,
    STRIP_SYS_HEADERS,
    collect_guarded_headers,
    collect_mpy_c_sources,
    ensure_micropython_sources,
    strip_c_line_comments_and_rename_class,
)

MPY_GENHDR_DIR = os.path.join(BUILD_DIR, "mpy_genhdr_win64")
DEFAULT_BUNDLE = os.path.join(BUILD_DIR, "micropython_win64_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_DIR, "micropython.exe")
DEFAULT_HOST_OUT = os.path.join(BUILD_DIR, "micropython_host_zcc.exe")

WIN64_MPCONFIGPORT_H = r"""#ifndef ZCC_WIN64_MPCONFIGPORT_H
#define ZCC_WIN64_MPCONFIGPORT_H

/* 64-bit Win64 LP64 under ZCC (-mabi=ms): preserve __x86_64__ / 64-bit pointers */
#define MPZ_DIG_SIZE (16)

#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)

#define MICROPY_ENABLE_COMPILER             (1)
#define MICROPY_ENABLE_GC                   (1)
#define MICROPY_ENABLE_PYSTACK              (1)
#define MICROPY_PY_GC                       (1)
#define MICROPY_HELPER_REPL                 (0)
#define MICROPY_HELPER_LEXER_UNIX           (0)
#define MICROPY_ENABLE_SOURCE_LINE          (1)
#define MICROPY_ENABLE_DOC_STRING           (0)
#define MICROPY_ERROR_REPORTING             (MICROPY_ERROR_REPORTING_DETAILED)
#define MICROPY_WARNINGS                    (1)
#define MICROPY_OPT_COMPUTED_GOTO           (0)
#define MICROPY_ROM_TEXT_COMPRESSION        (0)
#define MICROPY_NLR_SETJMP                  (1)
#define MICROPY_GCREGS_SETJMP               (0)
#define MICROPY_LONGINT_IMPL                (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL                  (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_PY_BUILTINS_COMPLEX         (1)
#define MICROPY_PY_MATH                     (1)
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS   (0)
#define MICROPY_PY_CMATH                    (0)
#define MICROPY_PY_IO                       (1)
#define MICROPY_PY_IO_IOBASE                (1)
#define MICROPY_PY_JSON                     (1)
#define MICROPY_PY_RE                       (1)
#define MICROPY_PY_RANDOM                   (1)
#define MICROPY_PY_RANDOM_EXTRA_FUNCS       (1)
#define MICROPY_PY_SYS_EXIT                 (1)
#define MICROPY_PY_SYS_PATH                 (1)
#define MICROPY_PY_SYS_ARGV                 (1)
#define MICROPY_PY_SYS_STDFILES             (0)
#define MICROPY_PY_SYS_MAXSIZE              (1)
#define MICROPY_PY_SYS_EXC_INFO             (1)
#define MICROPY_PY_TIME                     (0)
#define MICROPY_PY_SELECT                   (0)
#define MICROPY_PY_OS                       (0)
#define MICROPY_PY_PLATFORM                 (0)
#define MICROPY_PY_BINASCII                 (0)
#define MICROPY_PY_HASHLIB                  (0)
#define MICROPY_PY_HEAPQ                    (1)
#define MICROPY_PY_COLLECTIONS              (1)
#define MICROPY_PY_COLLECTIONS_DEQUE        (1)
#define MICROPY_PY_COLLECTIONS_ORDEREDDICT  (1)
#define MICROPY_PY_STRUCT                   (1)
#define MICROPY_PY_ERRNO                    (1)
#define MICROPY_PY_ASYNCIO                  (0)
#define MICROPY_PY_DEFLATE                  (0)
#define MICROPY_PY_FRAMEBUF                 (0)
#define MICROPY_PY_VFS                      (0)
#define MICROPY_PY_CRYPTOLIB                (0)
#define MICROPY_PY_SSL                      (0)
#define MICROPY_PY_WEBSOCKET                (0)
#define MICROPY_PY_UCTYPES                  (0)
#define MICROPY_READER_POSIX                (0)
#define MICROPY_READER_VFS                  (0)
#define MICROPY_PERSISTENT_CODE_LOAD        (0)
#define MICROPY_PERSISTENT_CODE_SAVE        (0)
#define MICROPY_KBD_EXCEPTION               (0)
#define MICROPY_CPYTHON_COMPAT              (1)
#define MICROPY_PY_DELATTR_SETATTR          (1)
#define MICROPY_PY_FSTRINGS                 (1)
#define MICROPY_PY_BUILTINS_HELP            (0)
#define MICROPY_PY_MICROPYTHON_MEM_INFO     (1)
#define MICROPY_STACK_CHECK                 (0)
#define MICROPY_USE_INTERNAL_PRINTF         (0)
#define MICROPY_USE_INTERNAL_ERRNO          (1)

#define MICROPY_HW_BOARD_NAME "ZCC-WIN64-PE32+"
#define MICROPY_HW_MCU_NAME   "x86_64-ms"
#define MICROPY_PY_SYS_PLATFORM "win64"

typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

#define MP_STATE_PORT MP_STATE_VM
#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void *)((mp_uint_t)(p)))
#define MICROPY_MIN_USE_C_STDLIB (1)
#define MP_PLAT_PRINT_STRN(str, len) mp_hal_stdout_tx_strn(str, len)

#define __builtin_expect(x, v) (x)
#define __builtin_unreachable() do {} while (0)
#define __builtin_clz(x) zcc_mpy_clz(x)
#define __builtin_ctz(x) zcc_mpy_ctz(x)

static inline int zcc_mpy_clz(unsigned int x) {
    if (x == 0U) return 32;
    int n = 0;
    while ((x & 0x80000000U) == 0U) { x <<= 1; n++; }
    return n;
}

static inline int zcc_mpy_ctz(unsigned int x) {
    if (x == 0U) return 32;
    int n = 0;
    while ((x & 1U) == 0U) { x >>= 1; n++; }
    return n;
}

#define MICROPY_PORT_ROOT_POINTERS
#endif
"""

WIN64_MPHALPORT_H = """#ifndef ZCC_WIN64_MPHALPORT_H
#define ZCC_WIN64_MPHALPORT_H
#include <stddef.h>
static inline void mp_hal_set_interrupt_char(int c) { (void)c; }
void mp_hal_stdout_tx_strn(const char *str, size_t len);
#endif
"""

WIN64_MPY_PRELUDE = """/* Auto-generated Upstream MicroPython v1.24.1 Native Win64 Bundle */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <assert.h>
#include <inttypes.h>
#include <alloca.h>
#include <time.h>

#undef PRId64
#define PRId64 "lld"
#undef PRIi64
#define PRIi64 "lli"
#undef PRIu64
#define PRIu64 "llu"
#undef PRIo64
#define PRIo64 "llo"
#undef PRIx64
#define PRIx64 "llx"
#undef PRIX64
#define PRIX64 "llX"

extern int _w64_isnan(double x);
extern int _w64_isinf(double x);
extern int _w64_isfinite(double x);
extern int _w64_signbit(double x);
#ifdef isnan
#undef isnan
#endif
#define isnan(x) _w64_isnan((double)(x))
#ifdef isinf
#undef isinf
#endif
#define isinf(x) _w64_isinf((double)(x))
#ifdef isfinite
#undef isfinite
#endif
#define isfinite(x) _w64_isfinite((double)(x))
#ifdef signbit
#undef signbit
#endif
#define signbit(x) _w64_signbit((double)(x))

extern double copysign(double x, double y);
extern double trunc(double x);
extern double round(double x);
extern float roundf(float x);
extern double nearbyint(double x);
extern double rint(double x);
extern double fmax(double x, double y);
extern double fmin(double x, double y);
extern double log2(double x);
extern double exp2(double x);
extern double expm1(double x);
extern double log1p(double x);
extern double asinh(double x);
extern double acosh(double x);
extern double atanh(double x);
extern double hypot(double x, double y);
extern double cbrt(double x);
extern double nan(const char *tagp);
extern long long strtoll(const char *nptr, char **endptr, int base);
extern unsigned long long strtoull(const char *nptr, char **endptr, int base);
extern float strtof(const char *nptr, char **endptr);
extern int *__errno_location(void);
#ifndef errno
#define errno (*__errno_location())
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX 9223372036854775807LL
#endif

#include "mpconfigport.h"
"""


def write_bundle_and_genhdr(
    bundle_path: str = DEFAULT_BUNDLE,
    mpy_dir: str = MPY_DIR,
    gen_root: str = MPY_GENHDR_DIR,
) -> tuple:
    ensure_micropython_sources(mpy_dir)
    genhdr_dir = os.path.join(gen_root, "genhdr")
    os.makedirs(genhdr_dir, exist_ok=True)
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)

    with open(os.path.join(gen_root, "mpconfigport.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(WIN64_MPCONFIGPORT_H)
    with open(os.path.join(gen_root, "mphalport.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(WIN64_MPHALPORT_H)
    with open(os.path.join(genhdr_dir, "mpversion.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(
            '#define MICROPY_GIT_TAG "v1.24.1"\n'
            '#define MICROPY_GIT_HASH "v1.24.1-zcc-win64"\n'
            '#define MICROPY_BUILD_DATE "2026-11-20"\n'
        )

    all_c = collect_mpy_c_sources(mpy_dir)
    guarded_hdrs = collect_guarded_headers(mpy_dir)

    mod_regs = []
    rp_regs = []
    for fn in all_c:
        with open(fn, "r", encoding="utf-8", errors="replace") as f:
            txt = f.read()
        for m in re.finditer(r"MP_REGISTER_(?:EXTENSIBLE_)?MODULE(?:_DELEGATION)?\s*\([^;]+\)\s*;", txt):
            line = m.group(0)
            if "MP_QSTR_cmath" in line or "MP_QSTR__thread" in line:
                continue
            mod_regs.append(line)
        for m in re.finditer(r"MP_REGISTER_ROOT_POINTER\s*\([^;]+\)\s*;", txt):
            rp_regs.append(m.group(0))

    mod_i = os.path.join(genhdr_dir, "module_regs.i")
    with open(mod_i, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(mod_regs) + "\n")
    r_m = subprocess.run(
        [sys.executable, os.path.join(mpy_dir, "py", "makemoduledefs.py"), mod_i],
        capture_output=True,
        text=True,
        check=True,
    )
    with open(os.path.join(genhdr_dir, "moduledefs.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(r_m.stdout)

    rp_i = os.path.join(genhdr_dir, "root_pointers.i")
    with open(rp_i, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rp_regs) + "\n")
    r_rp = subprocess.run(
        [sys.executable, os.path.join(mpy_dir, "py", "make_root_pointers.py"), rp_i],
        capture_output=True,
        text=True,
        check=True,
    )
    with open(os.path.join(genhdr_dir, "root_pointers.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(r_rp.stdout)

    qgen_path = os.path.join(genhdr_dir, "qstrdefs.generated.h")
    with open(qgen_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("")

    seen_hdrs = set()

    def process_src(raw: str, cur_dir: str) -> str:
        raw = raw.replace("\r\n", "\n").replace("\r", "\n")
        raw = strip_c_line_comments_and_rename_class(raw)
        out = []
        for line in raw.splitlines(keepends=True):
            m_ang = re.match(r"^\s*#\s*include\s*<([^>]+)>", line)
            if m_ang and m_ang.group(1) in STRIP_SYS_HEADERS:
                out.append(f"/* stripped <{m_ang.group(1)}> */\n")
                continue
            m_q = re.match(r'^\s*#\s*include\s*"([^"]+)"', line)
            if m_q:
                inc = m_q.group(1)
                cand = inc
                if not os.path.isfile(os.path.join(mpy_dir, cand)):
                    rel_c = os.path.relpath(os.path.join(cur_dir, inc), mpy_dir).replace("\\", "/")
                    if os.path.isfile(os.path.join(mpy_dir, rel_c)):
                        cand = rel_c
                if cand in guarded_hdrs:
                    if cand in seen_hdrs:
                        out.append(f'/* dedup "{cand}" */\n')
                        continue
                    seen_hdrs.add(cand)
                    with open(os.path.join(mpy_dir, cand), "r", encoding="utf-8", errors="replace") as hf:
                        out.append(f"/* >>> BEGIN HDR {cand} */\n")
                        out.append(process_src(hf.read(), os.path.dirname(os.path.join(mpy_dir, cand))))
                        out.append(f"\n/* <<< END HDR {cand} */\n")
                    continue
                if cand.startswith("lib/re1.5/") and os.path.isfile(os.path.join(mpy_dir, cand)):
                    with open(os.path.join(mpy_dir, cand), "r", encoding="utf-8", errors="replace") as cf:
                        out.append(f"/* >>> BEGIN INC {cand} */\n")
                        out.append(process_src(cf.read(), os.path.dirname(os.path.join(mpy_dir, cand))))
                        out.append(f"\n/* <<< END INC {cand} */\n")
                    continue
            out.append(line)
        return "".join(out)

    parts = [WIN64_MPY_PRELUDE]
    for ch in ("py/mpconfig.h", "py/misc.h"):
        seen_hdrs.add(ch)
        with open(os.path.join(mpy_dir, ch), "r", encoding="utf-8", errors="replace") as hf:
            parts.append(f"/* >>> BEGIN CORE HDR {ch} */\n")
            parts.append(process_src(hf.read(), os.path.join(mpy_dir, "py")))
            parts.append(f"\n/* <<< END CORE HDR {ch} */\n")
    parts.append("#undef MP_STATIC_ASSERT_NONCONSTEXPR\n#define MP_STATIC_ASSERT_NONCONSTEXPR(cond) ((void)1)\n")
    for ch in ("py/qstr.h", "py/obj.h", "py/runtime.h"):
        if ch not in seen_hdrs:
            seen_hdrs.add(ch)
            with open(os.path.join(mpy_dir, ch), "r", encoding="utf-8", errors="replace") as hf:
                parts.append(f"/* >>> BEGIN CORE HDR {ch} */\n")
                parts.append(process_src(hf.read(), os.path.join(mpy_dir, "py")))
                parts.append(f"\n/* <<< END CORE HDR {ch} */\n")

    for fn in all_c:
        rel = os.path.relpath(fn, mpy_dir).replace("\\", "/")
        parts.append(f"\n/* ===== BEGIN tests/embedded_micropython/{rel} ===== */\n")
        parts.append(f'#line 1 "tests/embedded_micropython/{rel}"\n')
        with open(fn, "r", encoding="utf-8", errors="replace") as f:
            parts.append(process_src(f.read(), os.path.dirname(fn)))
        parts.append(f"\n/* ===== END tests/embedded_micropython/{rel} ===== */\n")
        if os.path.basename(fn) == "malloc.c":
            parts.append("#undef malloc\n#undef free\n#undef realloc\n#undef calloc\n")

    drv = os.path.join(REPO_ROOT, "src", "win64_mpy_driver.c")
    if not os.path.isfile(drv):
        raise FileNotFoundError(f"Missing MicroPython Win64 driver: {drv}")
    parts.append('#include "src/win64_mpy_driver.c"\n')

    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(parts)

    bundle_rel = to_rel_posix(bundle_path)
    gen_rel = to_rel_posix(gen_root)
    mpy_rel = to_rel_posix(mpy_dir)
    pp_res = subprocess.run(
        [ZCC_BIN, "--pp-only", "-DNO_QSTR", f"-I{gen_rel}", f"-I{mpy_rel}", "-Iinclude", "-Isrc", "-I.", bundle_rel],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=90,
        check=True,
    )
    qstrs = set(re.findall(r"\bMP_QSTR_([A-Za-z0-9_]+)\b", pp_res.stdout))

    q_in = os.path.join(genhdr_dir, "qstrdefs.preprocessed.h")
    with open(q_in, "w", encoding="utf-8", newline="\n") as f:
        f.write("QCFG(BYTES_IN_LEN, 1)\nQCFG(BYTES_IN_HASH, 2)\n")
        with open(os.path.join(mpy_dir, "py", "qstrdefs.h"), "r", encoding="utf-8") as qf:
            for line in qf:
                m = re.match(r"^\s*Q\(([^)]*)\)", line)
                if m:
                    f.write(f"Q({m.group(1)})\n")
        for extra in SPECIAL_PRE_QSTRS:
            f.write(f"Q({extra})\n")
        for q in sorted(qstrs):
            if q in SPECIAL_QSTR_ENCODED_NAMES:
                continue
            f.write(f"Q({q})\n")

    r_q = subprocess.run(
        [sys.executable, os.path.join(mpy_dir, "py", "makeqstrdata.py"), q_in],
        capture_output=True,
        text=True,
        check=True,
    )
    with open(qgen_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(r_q.stdout)

    return bundle_path, len(all_c), len(qstrs), len(mod_regs)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone MicroPython v1.24.1 Win64 PE32+ executable cleanly via ZCC.")
    parser.add_argument("--bundle", default=DEFAULT_BUNDLE)
    parser.add_argument("--output", default=DEFAULT_OUT)
    parser.add_argument("--host-output", default=DEFAULT_HOST_OUT)
    parser.add_argument("--skip-win64-selfcompile", action="store_true")
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()

    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc not found; run `make zcc` first.")
    bundle_path, num_c, num_qstrs, num_mods = write_bundle_and_genhdr(args.bundle)
    bundle_rel = to_rel_posix(bundle_path)
    gen_rel = to_rel_posix(MPY_GENHDR_DIR)
    mpy_rel = to_rel_posix(MPY_DIR)
    print(
        f"[BUNDLE] {bundle_rel}: {os.path.getsize(bundle_path)} bytes "
        f"({num_c} upstream MicroPython C modules, {num_qstrs} QSTRs, {num_mods} builtin modules)",
        flush=True,
    )

    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        ("mpy_bundle.s", bundle_rel, [f"-I{gen_rel}", f"-I{mpy_rel}", "-Iinclude", "-Isrc", "-I."]),
    ]
    asm_inputs = [os.path.join(REPO_ROOT, "src", "win64_setjmp.s")]

    host_dir = os.path.join(BUILD_DIR, "mpy_host_asm")
    host_out = args.output if args.skip_win64_selfcompile else args.host_output
    print(f"[BUILD-HOST] ./zcc -mabi=ms -> {to_rel_posix(host_out)}", flush=True)
    compile_and_link_win64_units(units, asm_inputs, host_dir, host_out, use_win64_compiler=False, timeout_s=args.timeout)
    host_sha = sha256_file(host_out)
    print(f"[HOST-PE] {host_out}: {os.path.getsize(host_out)} bytes, SHA256={host_sha}", flush=True)

    if not args.skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        win_dir = os.path.join(BUILD_DIR, "mpy_win64_asm")
        print(f"[BUILD-WIN64] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(args.output)}", flush=True)
        compile_and_link_win64_units(units, asm_inputs, win_dir, args.output, use_win64_compiler=True, timeout_s=args.timeout)
        win_sha = sha256_file(args.output)
        print(f"[WIN64-PE] {args.output}: {os.path.getsize(args.output)} bytes, SHA256={win_sha}", flush=True)
        fc = run_fc_b(host_out, args.output)
        print(f"[FC /B] {fc.stdout.strip()}", flush=True)
        if host_sha != win_sha or fc.returncode != 0 or "no differences encountered" not in fc.stdout.lower():
            raise AssertionError(f"Parity mismatch between ./zcc and zcc_sov_stage3.exe: {host_sha} vs {win_sha}")
        print("[PARITY] Byte-identical MicroPython Win64 .exe produced by BOTH ./zcc and build_win64/zcc_sov_stage3.exe!", flush=True)

    audit = audit_pe32plus_msvcrt(args.output)
    print(
        f"[AUDIT] PE32+ OK: bytes={audit['bytes']} .pdata={audit['pdata']}B .reloc={audit['reloc']}B "
        f"StackReserve=0x{audit['stack_reserve']:x} Imports=msvcrt.dll({len(audit['msvcrt_imports'])})",
        flush=True,
    )
    print("[PASS] Standalone build_win64/micropython.exe validated cleanly (zero MSVC/MinGW/binutils)!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
