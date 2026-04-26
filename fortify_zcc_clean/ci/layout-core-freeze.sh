#!/usr/bin/env bash
set -euo pipefail

python3 tools/check_layout_core_freeze.py \
  --root . \
  --report artifacts/layout-core-freeze.json