#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src
TOTAL=0
for f in "$LUA"/*.c; do
    n=$($ZCC -I"$LUA" "$f" -o /tmp/out.s 2>&1 | grep -c "error:" || true)
    TOTAL=$((TOTAL + n))
done
echo "BEFORE: $TOTAL"
echo "$TOTAL" > /tmp/pp023a_before.txt
