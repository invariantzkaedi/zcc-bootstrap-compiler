#!/usr/bin/env bash
# tools/battle/narrow_luaopenlibs.sh
# ===================================================================
# Step 1 – Binary-narrow luaL_openlibs to the exact failing luaopen_*
# ===================================================================
# Usage:
#   LUA=/path/to/lua-5.4.6  ZCC=/path/to/zcc  bash narrow_luaopenlibs.sh
#
# Defaults assume repo root is the CWD.
set -euo pipefail

LUA=${LUA:-lua-5.4.6}
ZCC=${ZCC:-./zcc}
HARNESS=tools/battle/lua_narrow.c
OUT=lua_narrow

echo "=== [narrow] Rebuilding Lua $LUA with zcc ==="
# Assumes lua-5.4.6 Makefile has been patched to accept CC override.
make -C "$LUA" CC="$PWD/$ZCC" clean all 2>&1 | tail -30

echo ""
echo "=== [narrow] Linking diagnostic harness ==="
"$ZCC" -I"$LUA/src" -o "$OUT" "$HARNESS" \
    "$LUA"/src/*.o -lm -ldl

echo ""
echo "=== [narrow] Running (stderr → narrow.log) ==="
# Redirect stderr (fd 2) to log; stdout is suppressed but kept for Lua prints.
"$OUT" 2>&1 | tee narrow.log || true

echo ""
echo "=== [narrow] Last 10 markers before crash ==="
tail -10 narrow.log

echo ""
echo "Paste narrow.log tail into the session for Step 2 classification."
