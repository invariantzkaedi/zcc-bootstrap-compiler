#!/bin/bash
echo "=== Running Image Generators ==="
for e in exp11 exp12 exp13 exp14 exp15 exp9 exp8; do
    if [ -f "./$e" ]; then
        echo "--- $e ---"
        ./$e > /dev/null
    else
        echo "--- $e NOT FOUND ---"
    fi
done

echo "=== Running Text-Only Experiments ==="
for e in exp16 exp17_avx512 exp18 exp19 exp20; do
    if [ -f "./$e" ]; then
        echo "--- $e ---"
        ./$e
    else
        echo "--- $e NOT FOUND ---"
    fi
done
