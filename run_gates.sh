cd /mnt/g/zccMAIN/zcc
echo "--- Phase 2 Gate ---"
grep 'getchar\|putchar' part0_pp.c

echo "--- Phase 3 Gate ---"
make zcc

echo "--- Phase 4 Gate ---"
for f in zcc-level1/tools/cat.c zcc-level1/tools/echo.c \
          zcc-level1/tools/wc.c \
          zcc-level2/tools/cut.c zcc-level2/tools/grep.c \
          zcc-level2/tools/hexdump.c zcc-level2/tools/sort.c \
          zcc-level2/tools/uniq.c \
          zcc-level3/tools/calc.c zcc-level3/tools/minish.c \
          zcc-level4/tools/bf.c zcc-level4/tools/miniforth.c \
          zcc-level4/tools/minilisp.c zcc-level4/tools/tinycc.c; do
    ZCC_EMIT_IR=1 ./zcc $f -o /tmp/_g.s > /tmp/_g.ir 2>/tmp/_g.err
    rc=$?
    lines=$(wc -l < /tmp/_g.ir)
    if [ $rc -eq 0 ] && [ $lines -gt 0 ]; then
        echo "PASS $f lines=$lines"
    else
        echo "FAIL $f rc=$rc lines=$lines $(head -1 /tmp/_g.err)"
    fi
done

echo "--- Phase 5 Gate ---"
bash gate_ir1.sh

echo "--- Phase 6 Gate ---"
if make selfhost && cmp zcc2.s zcc3.s; then
    echo "BOOTSTRAP_GREEN"
    echo "[SYSTEM] Initiating ZKAEDI KINETIC BRIDGE..."
    ZCC_HASH=$(sha256sum zcc3.s | awk '{print $1}')
    command -v python3 >/dev/null 2>&1 && PYTHON=python3 || PYTHON=python
    $PYTHON tools/zkaedi_zcc_bridge.py "$ZCC_HASH"
else
    echo "BOOTSTRAP_RED"
    command -v python3 >/dev/null 2>&1 && PYTHON=python3 || PYTHON=python
    $PYTHON tools/zkaedi_zcc_bridge.py "0000000000000000"
    exit 1
fi
