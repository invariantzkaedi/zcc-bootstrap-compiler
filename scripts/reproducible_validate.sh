#!/usr/bin/env bash
# ==============================================================================
#   ZCC INDEPENDENT REPRODUCIBLE VALIDATION RUNNER
# ==============================================================================
# Usage:
#   ./scripts/reproducible_validate.sh [--differential-count 100] [--skip-selfhost]
# ==============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PYTHONPATH="${REPO_ROOT}/tools:${PYTHONPATH:-}"

python3 "${REPO_ROOT}/tools/reproducible_validation_runner.py" "$@"
