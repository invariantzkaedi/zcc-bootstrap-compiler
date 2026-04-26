#!/usr/bin/env bash
set -euo pipefail

required=(
  GITHUB_SHA
  TARGET
  FORTIFY_SIGNER
)

for name in "${required[@]}"; do
  if [ -z "${!name:-}" ]; then
    echo "$name is required for production fortify"
    exit 1
  fi
done

if [ "${FORTIFY_REQUIRE_SIGNATURE:-}" != "1" ]; then
  echo "FORTIFY_REQUIRE_SIGNATURE must be 1 in production"
  exit 1
fi

if [ ! -f fortify-verify-policy.json ]; then
  echo "fortify-verify-policy.json is required in production"
  exit 1
fi

echo "[PRODUCTION-ENV] ok"