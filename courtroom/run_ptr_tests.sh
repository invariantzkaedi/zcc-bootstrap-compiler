#!/bin/bash
set -e
cd /mnt/g/zccMAIN/zcc

PASS=0
FAIL=0
for f in tests/pointers/t_*.c; do
    name=$(basename "$f" .c)
    
    # GCC oracle
    gcc -O0 -w -fwrapv "$f" -o "/tmp/gcc_$name" 2>/dev/null
    set +e
    /tmp/gcc_$name
    gcc_rc=$?
    set -e
    
    # ZCC
    ./zcc "$f" 2>/dev/null
    set +e
    ./a.out
    zcc_rc=$?
    set -e
    
    if [ "$gcc_rc" = "$zcc_rc" ]; then
        echo "[PASS] $name (gcc=$gcc_rc zcc=$zcc_rc)"
        PASS=$((PASS + 1))
    else
        echo "[FAIL] $name (gcc=$gcc_rc zcc=$zcc_rc)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "═══════════════════════════════════════"
echo "  POINTER TESTS: PASS=$PASS FAIL=$FAIL"
echo "═══════════════════════════════════════"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
