#!/bin/bash
# PP-HEADERS-023 cluster analysis
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc

echo "=== Compiling all Lua .c files ==="
for f in "$LUA"/*.c; do
    "$ZCC" -I"$LUA" "$f" -o /tmp/lua_out.s 2>&1
done > /tmp/lua_all_errors.txt 2>&1

echo "=== Total error count ==="
grep -c "error:" /tmp/lua_all_errors.txt || echo 0

echo ""
echo "=== Top error patterns by frequency ==="
grep "error:" /tmp/lua_all_errors.txt \
    | grep -oE "error: [^\n]+" \
    | sed 's/[0-9]\+/N/g' \
    | sed "s/'[^']*'/'X'/g" \
    | sort | uniq -c | sort -rn | head -20

echo ""
echo "=== Files with most errors ==="
grep "error:" /tmp/lua_all_errors.txt \
    | grep -oE "[a-z]+\.c:[0-9]+" \
    | grep -oE "^[a-z]+\.c" \
    | sort | uniq -c | sort -rn | head -10

echo ""
echo "=== First 25 distinct error lines ==="
grep "error:" /tmp/lua_all_errors.txt | head -25

echo ""
echo "=== Sample context lines around errors ==="
grep -B1 "error:" /tmp/lua_all_errors.txt | head -50
