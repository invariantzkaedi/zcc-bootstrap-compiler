#!/usr/bin/env bash
# scripts/build_lua.sh — Item 5 reproduction script for Lua 5.4.6 → ZCC
#
# Outputs: lua_zcc.c       (preprocessed amalgamation, gitignored byproduct)
#          lua.s           (ZCC-generated assembly, gitignored byproduct)
#          lua_elf         (linked executable if LINK=1)
#
# Reproduction baseline (2026-07-10, GCC 13.3 / WSL2):
#   md5(lua.s) = dbca31acf090e9c4d91a98d425defc1e
#
# ZCC COMPAT NOTES:
#   - DLUA_USE_JUMPTABLE=0 is required because ZCC does not support labels as values (computed gotos).
#   - D'__builtin_expect(exp,c)=(exp)' is required because ZCC does not natively support the branch prediction builtin.
#

set -euo pipefail

if [ -f "$(dirname "$0")/Makefile" ]; then
    REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
else
    REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fi
cd "$REPO_ROOT"

OUTPUT_C="${1:-lua_zcc.c}"
OUTPUT_S="${2:-lua.s}"

LUA_VERSION="5.4.6"
LUA_DIR="/tmp/lua-${LUA_VERSION}"
LUA_TAR="/tmp/lua-${LUA_VERSION}.tar.gz"

if [ ! -d "$LUA_DIR" ]; then
    if [ ! -f "$LUA_TAR" ]; then
        echo "[ZCC-LUA] Downloading Lua ${LUA_VERSION}..."
        curl -L "https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz" -o "$LUA_TAR"
    fi
    echo "[ZCC-LUA] Verifying SHA-256 checksum..."
    echo "7d5ea1b9cb6aa0b59ca3dde1c6adcb57ef83a1ba8e5432c0ecd06bf439b3ad88  $LUA_TAR" | sha256sum -c -
    echo "[ZCC-LUA] Extracting Lua ${LUA_VERSION}..."
    tar -xzf "$LUA_TAR" -C /tmp
fi

echo "[ZCC-LUA] Generating onelua.c..."
cat > "${LUA_DIR}/src/onelua.c" << 'EOF'
#define MAKE_LIB

/* core */
#include "lapi.c"
#include "lcode.c"
#include "lctype.c"
#include "ldebug.c"
#include "ldo.c"
#include "ldump.c"
#include "lfunc.c"
#include "lgc.c"
#include "llex.c"
#include "lmem.c"
#include "lobject.c"
#include "lopcodes.c"
#include "lparser.c"
#include "lstate.c"
#include "lstring.c"
#include "ltable.c"
#include "ltm.c"
#include "lundump.c"
#include "lvm.c"
#include "lzio.c"

/* libraries */
#include "lauxlib.c"
#include "lbaselib.c"
#include "lcorolib.c"
#include "ldblib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"
#include "linit.c"

/* interpreter */
#undef MAKE_LIB
#include "lua.c"
EOF

echo "[ZCC-LUA] Preprocessing Lua sources..."
cd "${LUA_DIR}/src"
gcc -E -P -nostdinc \
    -I "${REPO_ROOT}/zcc_sys_includes" \
    -I "${REPO_ROOT}/zcc-libc" \
    -DMAKE_LUA \
    -DLUA_USE_JUMPTABLE=0 \
    -D'__builtin_expect(exp,c)=(exp)' \
    onelua.c -o "${REPO_ROOT}/${OUTPUT_C}"

cd "${REPO_ROOT}"
echo "[ZCC-LUA] Compiling ${OUTPUT_C} with zcc2..."
./zcc2 "${OUTPUT_C}" -o "${OUTPUT_S}" 2>&1 | tee /tmp/lua_build.log
ZCC_EXIT="${PIPESTATUS[0]}"

ERRORS=$(grep -c "error:" /tmp/lua_build.log || true)
echo ""
echo "ZCC_EXIT=$ZCC_EXIT  ERRORS=$ERRORS"

if [ "$ZCC_EXIT" -eq 0 ] && [ "$ERRORS" -eq 0 ]; then
    echo "[PASS] ${OUTPUT_S} generated: $(wc -l < "${OUTPUT_S}") lines, $(wc -c < "${OUTPUT_S}") bytes"
    MD5=$(md5sum "${OUTPUT_S}" | cut -d' ' -f1)
    echo "[PASS] md5(${OUTPUT_S}) = $MD5"
else
    echo "[FAIL] $ERRORS errors remain — see /tmp/lua_build.log"
    echo ""
    echo "=== ERROR SUMMARY ==="
    grep "error:" /tmp/lua_build.log | sed 's/.*error: //' | sort | uniq -c | sort -rn | head -10
    exit 1
fi

if [ "${LINK:-0}" = "1" ]; then
    echo "[ZCC-LUA] Linking..."
    gcc "${OUTPUT_S}" -o lua_elf -lm -ldl
    echo "[PASS] Executable lua_elf successfully built and linked."
    ./lua_elf -v
fi

echo "[ZCC-LUA] Done."
