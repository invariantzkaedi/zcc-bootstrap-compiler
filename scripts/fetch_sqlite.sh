#!/usr/bin/env bash
# scripts/fetch_sqlite.sh — Item 5 reproduction script for SQLite 3.45.0 → ZCC
#
# Requires: >4 GB RAM (amalgamation preprocessing and parsing has high RSS footprint)
#
# Outputs: sqlite3_zcc.c       (preprocessed/patched amalgamation, gitignored byproduct)
#          sqlite3_zcc.s       (ZCC-generated assembly, gitignored byproduct)
#          sqlite3_test        (linked test binary if LINK=1)
#
# Reproduction baseline (2026-07-10, GCC 13.3 / WSL2):
#   md5(sqlite3_zcc.s) = 187f253bedc550a214f07bb1f204368f
#

set -euo pipefail

if [ -f "$(dirname "$0")/Makefile" ]; then
    REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
else
    REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fi
cd "$REPO_ROOT"

OUTPUT_C="${1:-sqlite3_zcc.c}"
OUTPUT_S="${2:-sqlite3_zcc.s}"

SQLITE_VERSION="3450000"
SQLITE_DIR="/tmp/sqlite-amalgamation-${SQLITE_VERSION}"
SQLITE_ZIP="/tmp/sqlite-amalgamation-${SQLITE_VERSION}.zip"

if [ ! -d "$SQLITE_DIR" ]; then
    if [ ! -f "$SQLITE_ZIP" ]; then
        echo "[ZCC-SQLITE] Downloading SQLite 3.45.0..."
        curl -L "https://sqlite.org/2024/sqlite-amalgamation-${SQLITE_VERSION}.zip" -o "$SQLITE_ZIP"
    fi
    echo "[ZCC-SQLITE] Verifying SHA-256 checksum..."
    echo "bde30d13ebdf84926ddd5e8b6df145be03a577a48fd075a087a5dd815bcdf740  $SQLITE_ZIP" | sha256sum -c -
    echo "[ZCC-SQLITE] Extracting SQLite 3.45.0..."
    unzip -q -o "$SQLITE_ZIP" -d /tmp
fi

# Determine compiler binary
if [ -f ./zcc2 ]; then
    ZCC_BIN="./zcc2"
elif [ -f ./zcc ]; then
    ZCC_BIN="./zcc"
else
    echo "[ZCC-SQLITE] Error: no ZCC compiler binary found."
    exit 1
fi

echo "[ZCC-SQLITE] Preprocessing SQLite sources..."
gcc -E -P -nostdinc \
    -I "${REPO_ROOT}/zcc_sys_includes" \
    -I "${REPO_ROOT}/zcc-libc" \
    -include "${REPO_ROOT}/zcc_sqlite_compat.h" \
    -DSQLITE_THREADSAFE=0 \
    -DSQLITE_OMIT_LOAD_EXTENSION \
    "${SQLITE_DIR}/sqlite3.c" -o "${REPO_ROOT}/sqlite3_pp.c"

echo "[ZCC-SQLITE] Post-processing SQLite for ZCC compatibility..."
python3 "${REPO_ROOT}/patches/prep_sqlite_for_zcc.py" "${REPO_ROOT}/sqlite3_pp.c" "${REPO_ROOT}/${OUTPUT_C}"
rm -f "${REPO_ROOT}/sqlite3_pp.c"

# Apply layout patches for ZCC constant folding limitations
echo "[ZCC-SQLITE] Patching offset layout expressions..."
sed -i 's/sizeof(pPager->dbFileVers)/16/g' "${REPO_ROOT}/${OUTPUT_C}"
sed -i 's/(sizeof(Parse)-((long)\&((Parse\*)0)->sLastToken))/136/g' "${REPO_ROOT}/${OUTPUT_C}"
sed -i 's/((long)\&((Parse\*)0)->sLastToken)/288/g' "${REPO_ROOT}/${OUTPUT_C}"

echo "[ZCC-SQLITE] Compiling ${OUTPUT_C} with ZCC..."
"$ZCC_BIN" "${OUTPUT_C}" -o "${OUTPUT_S}" 2>&1 | tee /tmp/sqlite_build.log
ZCC_EXIT="${PIPESTATUS[0]}"

ERRORS=$(grep -c "error:" /tmp/sqlite_build.log || true)
echo ""
echo "ZCC_EXIT=$ZCC_EXIT  ERRORS=$ERRORS"

if [ "$ZCC_EXIT" -eq 0 ] && [ "$ERRORS" -eq 0 ]; then
    echo "[PASS] ${OUTPUT_S} generated: $(wc -l < "${OUTPUT_S}") lines, $(wc -c < "${OUTPUT_S}") bytes"
    MD5=$(md5sum "${OUTPUT_S}" | cut -d' ' -f1)
    echo "[PASS] md5(${OUTPUT_S}) = $MD5"
else
    echo "[FAIL] $ERRORS errors remain — see /tmp/sqlite_build.log"
    echo ""
    echo "=== ERROR SUMMARY ==="
    grep "error:" /tmp/sqlite_build.log | sed 's/.*error: //' | sort | uniq -c | sort -rn | head -10
    exit 1
fi

if [ "${LINK:-0}" = "1" ]; then
    echo "[ZCC-SQLITE] Linking..."
    gcc -no-pie -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables \
        -o sqlite3_test "${OUTPUT_S}" sqlite3_functest.c -ldl -lpthread -lm
    echo "[PASS] Test binary sqlite3_test successfully built and linked."
    ./sqlite3_test
fi

echo "[ZCC-SQLITE] Done."
