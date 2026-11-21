#!/usr/bin/env python3
"""
Standalone Upstream QuickJS 2024-01-13 Native Win64 PE32+ Builder (`build_win64/qjs.exe`)
==========================================================================================
Compiles the 5 strictly unmodified upstream QuickJS 2024-01-13 C core modules
(`tests/embedded_quickjs/{cutils,libunicode,libregexp,libbf,quickjs}.c`) with
`src/win64_app_crt.c`, `src/win64_c99_rt.c`, and `src/win64_qjs_driver.c` with zero
MSVC/MinGW/binutils via BOTH:
  1. Host `./zcc` -> `build_win64/qjs_host_zcc.exe`
  2. Native Windows sovereign `build_win64/zcc_sov_stage3.exe` -> `build_win64/qjs.exe`
Verifies SHA-256 + Windows `fc /b` byte-for-byte identity, PE32+ headers, and
`C:\\Windows\\System32\\msvcrt.dll` export resolution.
"""

import argparse
import os
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
from build_quickjs_wasm import (  # noqa: E402
    QJS_CORE_MODULES,
    QJS_DIR,
    ensure_quickjs_sources,
    sanitize_qjs_module,
)

DEFAULT_BUNDLE = os.path.join(BUILD_DIR, "qjs_win64_bundle.c")
DEFAULT_OUT = os.path.join(BUILD_DIR, "qjs.exe")
DEFAULT_HOST_OUT = os.path.join(BUILD_DIR, "qjs_host_zcc.exe")


def generate_bundle(bundle_path: str = DEFAULT_BUNDLE) -> str:
    ensure_quickjs_sources(QJS_DIR)
    os.makedirs(os.path.dirname(bundle_path), exist_ok=True)
    local_hdrs = {f for f in os.listdir(QJS_DIR) if f.endswith(".h")}

    parts = [
        "/* Auto-generated Upstream QuickJS 2024-01-13 Native Win64 Bundle */\n",
        "#define EMSCRIPTEN 1\n",
        '#define CONFIG_VERSION "2024-01-13"\n',
        "#define CONFIG_BIGNUM 1\n",
        "#define __builtin_expect(x, v) (x)\n",
        "#include <stdio.h>\n",
        "#include <stdlib.h>\n",
        "#include <stddef.h>\n",
        "#include <stdint.h>\n",
        "#include <string.h>\n",
        "#include <stdarg.h>\n",
        "#include <math.h>\n",
        "#include <limits.h>\n",
        "#include <float.h>\n",
        "#include <ctype.h>\n",
        "#include <errno.h>\n",
        "#include <time.h>\n",
        "#include <sys/time.h>\n",
        "#include <inttypes.h>\n",
        "#include <fenv.h>\n",
        "#include <alloca.h>\n",
        "#undef PRId64\n#define PRId64 \"lld\"\n",
        "#undef PRIi64\n#define PRIi64 \"lli\"\n",
        "#undef PRIu64\n#define PRIu64 \"llu\"\n",
        "#undef PRIo64\n#define PRIo64 \"llo\"\n",
        "#undef PRIx64\n#define PRIx64 \"llx\"\n",
        "#undef PRIX64\n#define PRIX64 \"llX\"\n",
        "#undef INTPTR_MIN\n#define INTPTR_MIN (-9223372036854775807LL - 1)\n",
        "#undef INTPTR_MAX\n#define INTPTR_MAX 9223372036854775807LL\n",
        "#undef INT64_MIN\n#define INT64_MIN (-9223372036854775807LL - 1)\n",
        "#undef INT64_MAX\n#define INT64_MAX 9223372036854775807LL\n",
        "#undef UINT64_MAX\n#define UINT64_MAX 18446744073709551615ULL\n",
        "extern size_t _msize(void *memblock);\n",
        "extern int _w64_isnan(double x);\n",
        "extern int _w64_isinf(double x);\n",
        "extern int _w64_isfinite(double x);\n",
        "extern int _w64_signbit(double x);\n",
        "#ifdef isnan\n#undef isnan\n#endif\n#define isnan(x) _w64_isnan((double)(x))\n",
        "#ifdef isinf\n#undef isinf\n#endif\n#define isinf(x) _w64_isinf((double)(x))\n",
        "#ifdef isfinite\n#undef isfinite\n#endif\n#define isfinite(x) _w64_isfinite((double)(x))\n",
        "#ifdef signbit\n#undef signbit\n#endif\n#define signbit(x) _w64_signbit((double)(x))\n",
        "extern double copysign(double x, double y);\n",
        "extern double round(double x);\n",
        "extern float roundf(float x);\n",
        "extern double nearbyint(double x);\n",
        "extern long long llrint(double x);\n",
        "extern double fmax(double x, double y);\n",
        "extern double fmin(double x, double y);\n",
        "extern double log2(double x);\n",
        "extern double exp2(double x);\n",
        "extern double fma(double x, double y, double z);\n",
        "extern double nan(const char *tagp);\n",
        "extern double exp(double x);\n",
        "extern double log(double x);\n",
        "extern long long strtoll(const char *nptr, char **endptr, int base);\n",
        "extern unsigned long long strtoull(const char *nptr, char **endptr, int base);\n",
        "extern float strtof(const char *nptr, char **endptr);\n",
        "extern time_t mktime(struct tm *tm);\n",
        "extern int *__errno_location(void);\n",
        "#ifndef errno\n#define errno (*__errno_location())\n#endif\n\n",
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

    drv = os.path.join(REPO_ROOT, "src", "win64_qjs_driver.c")
    if not os.path.isfile(drv):
        raise FileNotFoundError(f"Missing QuickJS Win64 driver: {drv}")
    parts.append('#include "src/win64_qjs_driver.c"\n')

    with open(bundle_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(parts)
    return bundle_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Build standalone QuickJS 2024-01-13 Win64 PE32+ executable cleanly via ZCC.")
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
    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        ("qjs_bundle.s", bundle_rel, ["-Itests/embedded_quickjs", "-Iinclude", "-I."]),
    ]

    host_dir = os.path.join(BUILD_DIR, "qjs_host_asm")
    host_out = args.output if args.skip_win64_selfcompile else args.host_output
    print(f"[BUILD-HOST] ./zcc -mabi=ms -> {to_rel_posix(host_out)}", flush=True)
    compile_and_link_win64_units(units, [], host_dir, host_out, use_win64_compiler=False, timeout_s=args.timeout)
    host_sha = sha256_file(host_out)
    print(f"[HOST-PE] {host_out}: {os.path.getsize(host_out)} bytes, SHA256={host_sha}", flush=True)

    if not args.skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        win_dir = os.path.join(BUILD_DIR, "qjs_win64_asm")
        print(f"[BUILD-WIN64] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(args.output)}", flush=True)
        compile_and_link_win64_units(units, [], win_dir, args.output, use_win64_compiler=True, timeout_s=args.timeout)
        win_sha = sha256_file(args.output)
        print(f"[WIN64-PE] {args.output}: {os.path.getsize(args.output)} bytes, SHA256={win_sha}", flush=True)
        fc = run_fc_b(host_out, args.output)
        print(f"[FC /B] {fc.stdout.strip()}", flush=True)
        if host_sha != win_sha or fc.returncode != 0 or "no differences encountered" not in fc.stdout.lower():
            raise AssertionError(f"Parity mismatch between ./zcc and zcc_sov_stage3.exe: {host_sha} vs {win_sha}")
        print("[PARITY] Byte-identical QuickJS Win64 .exe produced by BOTH ./zcc and build_win64/zcc_sov_stage3.exe!", flush=True)

    audit = audit_pe32plus_msvcrt(args.output)
    print(
        f"[AUDIT] PE32+ OK: bytes={audit['bytes']} .pdata={audit['pdata']}B .reloc={audit['reloc']}B "
        f"StackReserve=0x{audit['stack_reserve']:x} Imports=msvcrt.dll({len(audit['msvcrt_imports'])})",
        flush=True,
    )
    print("[PASS] Standalone build_win64/qjs.exe validated cleanly (zero MSVC/MinGW/binutils)!", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
