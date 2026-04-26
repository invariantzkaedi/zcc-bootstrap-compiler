#!/usr/bin/env bash
set -euo pipefail

rm -rf artifacts
rm -rf tests/layout/random
mkdir -p artifacts

echo "[FORTIFY-CLEAN] cleaned generated fortify artifacts"