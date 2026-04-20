#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
LUA=/mnt/h/__DOWNLOADS/selforglinux/lua-5.4.6/src
$ZCC -I$LUA --pp-only $LUA/lzio.c 2>/dev/null | awk 'NR>=1144 && NR<=1165 {print NR": "$0}'
