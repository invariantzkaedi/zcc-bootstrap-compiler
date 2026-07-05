#!/bin/bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="${REPO_DIR:-$(cd "$script_dir/../../.." && pwd)}"
spec_dir="${SPEC_DIR:-$(cd "$script_dir/../.." && pwd)}"

for test_script in "$script_dir"/[0-9]*.sh; do
  [ -f "$test_script" ] || continue
  bash "$test_script" "$repo_dir" "$spec_dir"
done
