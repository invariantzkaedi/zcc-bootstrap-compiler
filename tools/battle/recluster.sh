#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src

echo '=== Cluster: top error patterns after LLONG_MAX fix ==='
for f in "$LUA"/*.c; do
    $ZCC -I"$LUA" "$f" -o /tmp/out.s 2>&1
done 2>/dev/null | grep "error:" \
    | sed 's/[0-9]\+/N/g' \
    | sed "s/'[^']*'/'X'/g" \
    | sort | uniq -c | sort -rn | head -20

echo ''
echo '=== First error per file (after fix) ==='
for f in "$LUA"/lzio.c "$LUA"/lgc.c "$LUA"/ltable.c "$LUA"/lstate.c "$LUA"/lfunc.c; do
    test -f "$f" || continue
    first=$($ZCC -I"$LUA" "$f" -o /tmp/out.s 2>&1 | grep "error:" | head -3)
    echo "$(basename $f): $first"
done

echo ''
echo '=== LUA_INTEGER residual (should be 0 after fix) ==='
$ZCC -I"$LUA" --pp-only "$LUA/lzio.c" 2>/dev/null | grep -c "LUA_INTEGER"
