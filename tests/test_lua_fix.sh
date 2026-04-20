#!/bin/bash
cd /mnt/h/__DOWNLOADS/selforglinux
rm -f zcc zcc.c
make zcc
cd lua-5.4.6/src
../../zcc lua_zcc.c -o lua.s
gcc -no-pie lua.s -o lua_zcc_fixed -lm
./lua_zcc_fixed -e 'print("SUCCESS:", 1+1)'
echo "EXIT CODE: $?"
