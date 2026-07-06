#!/usr/bin/env bash
set -euo pipefail

echo "[guard] running correctness quick-gate..."
make -C tests/verify test-negative
make -C tests/verify-positive test-positive
make -C tests/opt/instcombine test-normalized
make -C tests/opt/sccp test-normalized

echo "[guard] all green."
