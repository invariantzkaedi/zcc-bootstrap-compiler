#!/bin/bash
# ══════════════════════════════════════════════════════════════════
# verify_ir_backend.sh — IR backend parity verification script
# ══════════════════════════════════════════════════════════════════
set -e

RED='\033[0;31m'
GRN='\033[0;32m'
RST='\033[0m'

# 1. Determine ZCC compiler binary
if [ -f ./zcc2 ]; then
    ZCC="./zcc2"
elif [ -f ./zcc ]; then
    ZCC="./zcc"
else
    echo -e "${RED}[FAIL] IR backend parity verification: no ZCC compiler binary found${RST}"
    exit 1
fi

TMPDIR="/tmp/ir_verify"
rm -rf "$TMPDIR"
mkdir -p "$TMPDIR"

# 2. Compile sqlite3_functest.c through AST path and emit IR graph
ZCC_EMIT_IR=1 "$ZCC" --emit-ir-graph "$TMPDIR/sqlite3_functest.json" sqlite3_functest.c -o "$TMPDIR/ast.s" 2>/dev/null

# 3. Replay the serialized IR graph directly to assembly
"$ZCC" --replay-ir "$TMPDIR/sqlite3_functest.json" -o "$TMPDIR/ir.s" 2>/dev/null

# 4. Compare AST and IR assembly output
if diff -u "$TMPDIR/ast.s" "$TMPDIR/ir.s" > "$TMPDIR/diff.log"; then
    echo -e "${GRN}[PASS] IR backend parity verified${RST}"
else
    echo -e "${RED}[FAIL] IR backend parity diverged${RST}"
    cat "$TMPDIR/diff.log" | head -n 50
    exit 1
fi

# --- DCE coverage target (test_cond74.c — DCE deletes 6 nodes) ---
ZCC_EMIT_IR=1 ./zcc2 tests/test_cond74.c --emit-ir-graph /tmp/cond74.ir.json -o /tmp/cond74_ast.s 2>/dev/null
./zcc2 --replay-ir /tmp/cond74.ir.json -o /tmp/cond74_replay.s 2>/dev/null
diff /tmp/cond74_ast.s /tmp/cond74_replay.s && echo "[PASS] IR backend parity verified (DCE target)" || (echo "[FAIL] IR backend parity diverged (DCE target)"; exit 1)
