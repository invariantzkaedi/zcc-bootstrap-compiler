#!/bin/bash
# verify-report.sh
# Tests the reporting logic of emit-evidence.ts

set -e

SPEC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$SPEC_DIR"

export RUN_ID="test-report-run-123"

# Clean up any previous test artifacts
rm -rf "artifacts/evidence/runs/$RUN_ID"

echo "Running reporting tests..."

# 1. Emit a normal pass
./node_modules/.bin/tsx scripts/emit-evidence.ts \
  --stage "verify" --rules "ZCC-FE-001" --target "x86_64-sysv" \
  --result "pass" --test "test1" --run-id "$RUN_ID" >/dev/null

# 2. Emit an unexpired waived fail
./node_modules/.bin/tsx scripts/emit-evidence.ts \
  --stage "verify" --rules "ZCC-FE-002" --target "x86_64-sysv" \
  --result "waived_fail" --test "test2" --run-id "$RUN_ID" \
  --waiver-id "W-TEST-001" --waiver-owner "test" --waiver-expiry "2030-01-01T00:00:00Z" >/dev/null

# 3. Verify report passes
if ./node_modules/.bin/tsx scripts/emit-evidence.ts --report --run-id "$RUN_ID" >/dev/null; then
  echo "✅ Report correctly passed with active waiver"
else
  echo "❌ Report failed unexpectedly with active waiver"
  exit 1
fi

# 4. Emit an expired waived fail
./node_modules/.bin/tsx scripts/emit-evidence.ts \
  --stage "verify" --rules "ZCC-FE-003" --target "x86_64-sysv" \
  --result "waived_fail" --test "test3" --run-id "$RUN_ID" \
  --waiver-id "W-TEST-002" --waiver-owner "test" --waiver-expiry "2020-01-01T00:00:00Z" >/dev/null

# 5. Verify report fails
if ./node_modules/.bin/tsx scripts/emit-evidence.ts --report --run-id "$RUN_ID" >/dev/null 2>&1; then
  echo "❌ Report passed unexpectedly with expired waiver"
  exit 1
else
  echo "✅ Report correctly failed with expired waiver"
fi

rm -rf "artifacts/evidence/runs/$RUN_ID"
echo "All reporting logic tests passed!"
