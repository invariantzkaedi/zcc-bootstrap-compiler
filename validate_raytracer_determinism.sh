#!/bin/bash
set -e

echo "=== Compiling Compiler (Make Selfhost) ==="
make selfhost

echo "=== Building ZCC3 Binary ==="
gcc -O0 -w -fno-asynchronous-unwind-tables -o zcc3 zcc3.s compiler_passes.c compiler_passes_ir.c -lm

echo "=== Compiling Raytracer with ZCC2 ==="
ZCC_EMIT_TELEMETRY=1 ./zcc2 raytracer.c -o raytracer2.s 2> telem2.log
gcc -O0 -w -fno-asynchronous-unwind-tables -o r2 raytracer2.s -lm

echo "=== Compiling Raytracer with ZCC3 ==="
ZCC_EMIT_TELEMETRY=1 ./zcc3 raytracer.c -o raytracer3.s 2> telem3.log
gcc -O0 -w -fno-asynchronous-unwind-tables -o r3 raytracer3.s -lm

echo "=== Verifying Raytracer Assembly Parity ==="
if cmp -s raytracer2.s raytracer3.s; then
    echo "SUCCESS: raytracer2.s and raytracer3.s match identically (Diamond Check Pass)"
else
    echo "FAILED: Assembly parity failed for raytracer.c"
    exit 1
fi

echo "=== Tracing Raytracer Execution (PPM and Binary Parity) ==="
./r2 > out2.ppm
./r3 > out3.ppm

if cmp -s out2.ppm out3.ppm; then
    echo "SUCCESS: out2.ppm and out3.ppm match identically"
else
    echo "FAILED: PPM output differs"
    exit 1
fi

echo "=== Running Hardware Register Allocation Slack Audit (asm_scraper) ==="
python3 asm_scraper.py telem2.log raytracer2.s

echo "=== Validation Complete ==="
