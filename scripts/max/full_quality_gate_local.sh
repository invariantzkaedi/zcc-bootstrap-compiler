#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

echo "[A] Build with Coverage"
make all OUT=build/cand CFLAGS="$CFLAGS_COV" LDFLAGS="$LDFLAGS_COV"

echo "[B] expose tools"
mkdir -p "$CI_BIN_DIR"
cp -f build/cand/zcc-opt   "$CI_BIN_DIR/zcc-opt"
cp -f build/cand/zcc-verify "$CI_BIN_DIR/zcc-verify"
chmod +x "$CI_BIN_DIR/zcc-opt" "$CI_BIN_DIR/zcc-verify"
export PATH="$PWD/$CI_BIN_DIR:$PATH"

echo "[C] correctness suites"
make -C tests/verify          test-negative
make -C tests/verify-positive test-positive
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp        test-normalized

echo "[D] coverage metrics verification"
chmod +x "$CHECK_COVERAGE"
bash "$CHECK_COVERAGE"

echo "[E] collect artifacts"
bash "$COLLECT_IR" "$IR_ARTIFACTS_DIR"

echo "local quality gate PASS"
