#!/usr/bin/env bash
set -euo pipefail

python3 tools/check_no_placeholders.py \
  --root . \
  --policy .zcc-fortify-policy.json \
  --report artifacts/no-placeholders.json