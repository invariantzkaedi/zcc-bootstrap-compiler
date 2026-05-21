#!/bin/bash
./zcc2 zcc.c -o zcc3 > /dev/null 2>&1 &
PID=$!
sleep 1
gdb -batch -ex "bt" -p $PID
sleep 1
gdb -batch -ex "bt" -p $PID
sleep 1
gdb -batch -ex "bt" -p $PID
kill -9 $PID
