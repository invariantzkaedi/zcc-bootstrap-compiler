#!/usr/bin/env bash
set -euo pipefail

make all

export PATH="$PWD/build:$PATH"

make -C tests/verify test-negative
make -C tests/verify-positive test-positive
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp test-normalized

bash scripts/ci/collect_ir_artifacts.sh out/ir-artifacts
echo "Local IR CI complete."
