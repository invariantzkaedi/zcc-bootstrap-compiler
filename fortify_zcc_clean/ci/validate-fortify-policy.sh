#!/usr/bin/env bash
set -euo pipefail

python3 tools/validate_fortify_policy.py \
  --root . \
  --policy .zcc-fortify-policy.json