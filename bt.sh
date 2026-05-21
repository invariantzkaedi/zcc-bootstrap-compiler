#!/bin/bash
./zcc2 zcc.c -o zcc3 > /dev/null 2>&1 &
PID=$!
sleep 2
echo "Attaching to PID $PID"
gdb -batch -ex "bt" -p $PID
kill -9 $PID
