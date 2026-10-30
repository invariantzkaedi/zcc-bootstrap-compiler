#!/usr/bin/env bash
# Regression & Negative Control Suite for E-LEARN-024 / BUG-CORE05-001
# Verifies that ZCC natively compiles preprocessed .i files and never silently
# delegates .i inputs or compile-only (-S/-c) invocations without source to host GCC.
set -euo pipefail

ZCC_BIN="${1:-./zcc}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "=== [E-LEARN-024] Testing ZCC Preprocessed .i Input & Compile-Only Fail-Closed Guards ==="

# 1. Positive Control: valid preprocessed .i file compiled with -S and full binary
cat << 'EOF' > "$TMP_DIR/valid_tu.i"
# 1 "valid_tu.c"
int compute_answer(int a, int b) {
    return a * 6 + b;
}
int main(void) {
    return compute_answer(6, 6) == 42 ? 0 : 1;
}
EOF

"$ZCC_BIN" -S "$TMP_DIR/valid_tu.i" -o "$TMP_DIR/valid_tu.s"
if ! head -n 5 "$TMP_DIR/valid_tu.s" | grep -q "ZCC\|.text\|.globl"; then
    echo "[FAIL] Positive control .s output is not assembly!" >&2
    exit 1
fi
if head -c 4 "$TMP_DIR/valid_tu.s" | grep -q $'\x7fELF'; then
    echo "[FAIL] Positive control -S emitted an ELF binary instead of assembly!" >&2
    exit 1
fi
echo "[PASS] Positive Control 1A: ./zcc -S valid_tu.i emitted genuine assembly"

"$ZCC_BIN" "$TMP_DIR/valid_tu.i" -o "$TMP_DIR/valid_tu_bin"
"$TMP_DIR/valid_tu_bin"
echo "[PASS] Positive Control 1B: ./zcc valid_tu.i compiled and executed cleanly (exit 0)"

# 2. Negative Control 1: GCC-only construct in .i file MUST be rejected by ZCC frontend
cat << 'EOF' > "$TMP_DIR/gcc_only.i"
int x = __builtin_choose_expr(1, 42, 0);
int main(void) { return x; }
EOF

set +e
"$ZCC_BIN" "$TMP_DIR/gcc_only.i" -o "$TMP_DIR/gcc_only_bin" > "$TMP_DIR/gcc_only.log" 2>&1
GCC_ONLY_RC=$?
set -e
if [ "$GCC_ONLY_RC" -eq 0 ]; then
    echo "[FAIL] Negative Control 1 failed-open: .i file with GCC-only builtin silently succeeded!" >&2
    exit 1
fi
if ! grep -q "__builtin_choose_expr" "$TMP_DIR/gcc_only.log"; then
    echo "[FAIL] Negative Control 1 rejected for unexpected reason:" >&2
    cat "$TMP_DIR/gcc_only.log" >&2
    exit 1
fi
echo "[PASS] Negative Control 1: GCC-only .i file rejected by ZCC native parser (rc=$GCC_ONLY_RC)"

# 3. Negative Control 2: -S without a source file MUST fail closed (never invoke gcc linker)
touch "$TMP_DIR/dummy.o"
set +e
"$ZCC_BIN" -S "$TMP_DIR/dummy.o" -o "$TMP_DIR/dummy.s" > "$TMP_DIR/dummy.log" 2>&1
DUMMY_RC=$?
set -e
if [ "$DUMMY_RC" -eq 0 ]; then
    echo "[FAIL] Negative Control 2 failed-open: -S without source input exited 0!" >&2
    exit 1
fi
if ! grep -q "no source input file specified for compilation" "$TMP_DIR/dummy.log"; then
    echo "[FAIL] Negative Control 2 rejected for unexpected reason:" >&2
    cat "$TMP_DIR/dummy.log" >&2
    exit 1
fi
echo "[PASS] Negative Control 2: -S without source input failed closed (rc=$DUMMY_RC)"

# 4. Negative Control 3: Unknown file extension MUST fail closed (never fall through to gcc)
cp "$TMP_DIR/valid_tu.i" "$TMP_DIR/sneaky_source.tu"
set +e
"$ZCC_BIN" "$TMP_DIR/sneaky_source.tu" -o "$TMP_DIR/sneaky_bin" > "$TMP_DIR/sneaky.log" 2>&1
SNEAKY_RC=$?
set -e
if [ "$SNEAKY_RC" -eq 0 ]; then
    echo "[FAIL] Negative Control 3 failed-open: unknown extension .tu exited 0!" >&2
    exit 1
fi
if ! grep -q "unrecognized input file extension" "$TMP_DIR/sneaky.log"; then
    echo "[FAIL] Negative Control 3 rejected for unexpected reason:" >&2
    cat "$TMP_DIR/sneaky.log" >&2
    exit 1
fi
echo "[PASS] Negative Control 3: Unknown extension (.tu) rejected fail-closed (rc=$SNEAKY_RC)"

echo "=== [E-LEARN-024] ALL POSITIVE & NEGATIVE CONTROLS PASSED ==="

