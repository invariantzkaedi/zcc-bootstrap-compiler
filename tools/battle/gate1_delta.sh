#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src
AFTER=0
for f in "$LUA"/*.c; do
    n=$($ZCC -I"$LUA" "$f" -o /tmp/out.s 2>&1 | grep -c "error:" || true)
    AFTER=$((AFTER + n))
done
BEFORE=$(cat /tmp/pp023a_before.txt)
echo "BEFORE: $BEFORE"
echo "AFTER:  $AFTER"
REDUCTION=$((BEFORE - AFTER))
PCT=$((100 * REDUCTION / BEFORE))
echo "Reduction: $REDUCTION (~$PCT%)"
if [ "$PCT" -ge 90 ]; then
    echo "GATE 1: PASS (>=90% reduction)"
elif [ "$PCT" -ge 50 ]; then
    echo "GATE 1: PARTIAL ($PCT% -- hidden root cause present, re-cluster)"
else
    echo "GATE 1: FAIL ($PCT% -- diagnosis incomplete, halt and re-diagnose)"
fi
