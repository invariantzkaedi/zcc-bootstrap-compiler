#!/bin/bash
PID=$(pgrep -n zcc2)
if [ -n "$PID" ]; then
    gdb -batch -ex "bt" -p $PID
else
    echo "No zcc2 running"
fi
