#!/bin/sh
# va_arg-free probe - UNIX line endings enforced
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src

echo "=== va_arg-FREE files ==="
for f in $LUA/llex.c $LUA/ltable.c $LUA/lgc.c $LUA/lstate.c $LUA/lfunc.c $LUA/lmem.c $LUA/ldump.c $LUA/lundump.c $LUA/lzio.c $LUA/lopnames.h; do
    test -f "$f" || continue
    vacount=$(grep -c "va_arg" "$f")
    errs=$($ZCC -I$LUA "$f" -o /tmp/out.s 2>&1 | grep -c "error:" || true)
    echo "VA=[$vacount] errs=[$errs] $(basename $f)"
done

echo ""
echo "=== va_arg-USING files (error count only) ==="
for f in $LUA/lapi.c $LUA/ldo.c $LUA/lstrlib.c $LUA/lauxlib.c $LUA/lobject.c; do
    test -f "$f" || continue
    vacount=$(grep -c "va_arg" "$f")
    errs=$($ZCC -I$LUA "$f" -o /tmp/out.s 2>&1 | grep -c "error:" || true)
    echo "VA=[$vacount] errs=[$errs] $(basename $f)"
done
