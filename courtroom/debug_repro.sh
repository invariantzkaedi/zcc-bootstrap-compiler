#!/bin/bash
cd /mnt/g/zccMAIN/zcc

# GCC oracle
gcc -O0 -w -fwrapv courtroom/repro_ptr.c -o /tmp/repro_gcc
/tmp/repro_gcc
GCC_RC=$?

# ZCC
./zcc courtroom/repro_ptr.c 2>/dev/null
./a.out
ZCC_RC=$?

echo "GCC_RC=$GCC_RC"
echo "ZCC_RC=$ZCC_RC"

if [ "$GCC_RC" != "$ZCC_RC" ]; then
    echo "MISMATCH"
else
    echo "MATCH"
fi
