#!/usr/bin/env bash
# run_entropy_audit.sh
set -eu

file="${1:-tests/test_phase18_bitfield_layout_abi_gauntlet.c}"
name="$(basename "$file" .c)"
zcc="${2:-./zcc}"

echo "=== STARTING ENTROPY AUDIT ON $name (using $zcc) ==="

mkdir -p audit_scratch
rm -f audit_scratch/*

# Round 1: Baseline Sequential Runs
echo "Running baseline sequential compilations..."
for i in {1..5}; do
  "$zcc" "$file" -o "audit_scratch/baseline_$i.s" > /dev/null
done

# Round 2: Randomized Environment Variables Shuffling
echo "Running compilations with randomized environmental states..."
for i in {1..5}; do
  # Shuffling environment using env -i with custom, randomized vars
  env -i \
    PATH="$PATH" \
    RANDOM_ENTROPY_SEED="$RANDOM" \
    ITERATION_INDEX="$i" \
    LC_ALL="C" \
    LANG="en_US.UTF-8" \
    "$zcc" "$file" -o "audit_scratch/env_shuffled_$i.s" > /dev/null
done

# Round 3: Telemetry Enabled Runs
echo "Running compilations with full ZCC telemetry enabled..."
for i in {1..5}; do
  ZCC_REAL_TELEMETRY=1 \
  ZCC_EMIT_IR=1 \
  "$zcc" "$file" -o "audit_scratch/telemetry_$i.s" > /dev/null
done

# Round 4: Verification of Bitwise Parity across all runs
echo "Analyzing SHA256 hashes..."
sha256sum audit_scratch/*.s > audit_scratch/hashes.txt
cat audit_scratch/hashes.txt

unique_hashes=$(awk '{print $1}' audit_scratch/hashes.txt | sort -u | wc -l)

if [ "$unique_hashes" -eq 1 ]; then
  echo "=== [PASS] ENTROPY AUDIT PERFECTLY CONVERGED: All emitted assemblies are bitwise identical! ==="
  rm -rf audit_scratch
  exit 0
else
  echo "=== [FAIL] ENTROPY AUDIT DIVERGED: Detected $unique_hashes unique assembly variants! ==="
  exit 1
fi
