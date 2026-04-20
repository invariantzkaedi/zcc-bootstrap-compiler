#!/bin/bash
# PP-STDARG-002 pre-design probe: verify va_arg-free files compile cleanly
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc

echo "=== va_arg-free file probe ==="
for f in "$LUA"/*.c; do
    count=$(grep -cE "va_arg|va_start|va_end" "$f" 2>/dev/null || echo 0)
    if [ "$count" -eq 0 ]; then
        errs=$("$ZCC" -I"$LUA" "$f" -o /tmp/nover.s 2>&1 | grep -cE "error:" || echo 0)
        echo "CLEAN[$errs] $f"
    fi
done

echo ""
echo "=== va_arg-USING files (first error per file) ==="
for f in "$LUA"/*.c; do
    count=$(grep -cE "va_arg|va_start|va_end" "$f" 2>/dev/null || echo 0)
    if [ "$count" -gt 0 ]; then
        first=$("$ZCC" -I"$LUA" "$f" -o /tmp/ver.s 2>&1 | grep "error:" | head -2)
        echo "VA[$count] $(basename $f): $first"
    fi
done
