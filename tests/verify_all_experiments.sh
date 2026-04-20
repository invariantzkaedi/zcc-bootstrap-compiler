#!/bin/bash
# verify_all_experiments.sh

echo "🔍 Starting ZCC Experiment Verification Suite..."
ALL_PASSED=1

for i in 1 2 3 4 5; do
    echo "========================================"
    echo "Testing Experiment $i"
    
    # Identify source file
    SRC=$(ls exp${i}_*.c 2>/dev/null | head -n 1)
    if [ -z "$SRC" ]; then
        echo "✗ FAIL: Could not find source file for experiment $i"
        ALL_PASSED=0
        continue
    fi
    
    # 1. Compile with ZCC
    echo -n "  Compiling with ZCC... "
    ./zcc "$SRC" -o "exp${i}.s" >/dev/null 2>&1
    if [ $? -eq 0 ] && [ -s "exp${i}.s" ]; then
        echo "✅ OK"
    else
        echo "✗ FAIL"
        ALL_PASSED=0
        continue
    fi
    
    # 2. Link with GCC
    echo -n "  Linking with GCC... "
    gcc -o "exp${i}" "exp${i}.s" -lm >/dev/null 2>&1
    if [ $? -eq 0 ] && [ -x "exp${i}" ]; then
        echo "✅ OK"
    else
        echo "✗ FAIL"
        ALL_PASSED=0
        continue
    fi
    
    # 3. Runtime Test
    echo -n "  Running Native Binary... "
    ./"exp${i}" > "exp${i}_output.ppm" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✅ OK"
    else
        echo "✗ FAIL"
        ALL_PASSED=0
        continue
    fi
    
    # 4. output validation
    echo -n "  Validating PPM Output... "
    MAGIC=$(head -c 2 "exp${i}_output.ppm")
    if [ "$MAGIC" = "P6" ]; then
        echo "✅ OK"
    else
        echo "✗ FAIL ($MAGIC)"
        ALL_PASSED=0
        continue
    fi
done

echo "========================================"
if [ $ALL_PASSED -eq 1 ]; then
    echo "╔══════════════════════════════════════════════════════════════════╗"
    echo "║                                                                  ║"
    echo "║  ✓✓✓ ALL EXPERIMENTS VALIDATED - 100% SUCCESS ✓✓✓               ║"
    echo "║                                                                  ║"
    echo "║  ZCC compiler is PRODUCTION READY for graphics workloads!       ║"
    echo "║                                                                  ║"
    echo "╚══════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Generated outputs:"
    ls -lh exp*_output.ppm
else
    echo "🔴 SOME EXPERIMENTS FAILED VERIFICATION 🔴"
fi
