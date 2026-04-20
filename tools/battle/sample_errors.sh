#!/bin/bash
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src

for f in "$LUA/lua.c" "$LUA/lstrlib.c" "$LUA/lmathlib.c" "$LUA/liolib.c" "$LUA/loslib.c"; do
    echo "=== $f ==="
    "$ZCC" -I"$LUA" "$f" -o /tmp/out.s 2>&1 | grep 'error:' | head -8
done
