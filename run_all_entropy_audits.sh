#!/usr/bin/env bash
# run_all_entropy_audits.sh
set -e

compiler="${1:-./zcc}"

echo "=== RUNNING ENTROPY AUDITS ON ALL 12 SENTINELS WITH $compiler ==="

for f in \
  tests/test_phase7_varargs_callback_abi.c \
  tests/test_phase8_varargs_fp_lanes.c \
  tests/test_phase9_bitfield_double_layout.c \
  tests/test_phase10_struct_return_chain.c \
  tests/test_phase11_stack_arg_spill.c \
  tests/test_phase12_callback_stack_pressure.c \
  tests/test_phase13_short_circuit_mutation.c \
  tests/test_phase14_nested_union_payload.c \
  tests/test_phase15_recursive_struct_return.c \
  tests/test_phase16_alias_aggregate_mutation.c \
  tests/test_phase17_fp_integer_pressure.c \
  tests/test_phase18_bitfield_layout_abi_gauntlet.c
do
  ./run_entropy_audit.sh "$f" "$compiler"
done

echo "=== ALL 12 SENTINELS PASS ENTROPY RESISTANCE AUDIT PERFECTLY WITH $compiler ==="
