#!/bin/bash
./zcc2 zcc.c -o zcc_out_test &
PID=$!
sleep 2
sudo gdb -p $PID -batch -ex "bt" -ex "info registers" -ex "disassemble \$pc-20,\$pc+20"
sudo kill -9 $PID
