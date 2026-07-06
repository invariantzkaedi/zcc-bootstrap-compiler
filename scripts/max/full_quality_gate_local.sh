#!/usr/bin/env bash
set -euo pipefail

echo "[A] Build"
make all OUT=build/cand

echo "[B] expose tools"
mkdir -p .ci/bin
cp -f build/cand/zcc-opt .ci/bin/zcc-opt
cp -f build/cand/zcc-verify .ci/bin/zcc-verify
chmod +x .ci/bin/zcc-opt .ci/bin/zcc-verify
export PATH="$PWD/.ci/bin:$PATH"

echo "[C] correctness suites"
make -C tests/verify test-negative
make -C tests/verify-positive test-positive
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp test-normalized

echo "[D] collect artifacts"
bash scripts/ci/collect_ir_artifacts.sh out/ir-artifacts || true

echo "local quality gate PASS"
