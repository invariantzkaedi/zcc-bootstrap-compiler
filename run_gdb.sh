#!/bin/bash
gdb --batch -ex 'run' -ex 'bt' -ex 'frame 0' -ex 'p cc->pos' -ex 'p cc->source[cc->pos-10]' -ex 'p cc->source[cc->pos]' --args ./zcc_debug zcc.c -o zcc3 &
PID=\$!
sleep 5
kill -INT \28884
sleep 1
