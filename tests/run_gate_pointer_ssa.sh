#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload

# 1. Run 1: Harness-first baseline build -> pointer_ssa_baseline.log (Exit 2)
gcc -O2 -DTEST_BASELINE_BUILD -Iinclude -I. tests/test_pointer_ssa_intervals.c -o /tmp/test_pointer_ssa_base
/tmp/test_pointer_ssa_base > /tmp/pointer_ssa_baseline.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/pointer_ssa_baseline.log

# 2. Run 2: Fault-injected build (-DFAULT_INJECT_POINTER_SSA) -> pointer_ssa_fault.log (Exit 1)
gcc -O2 -DFAULT_INJECT_POINTER_SSA -Iinclude -I. tests/test_pointer_ssa_intervals.c -o /tmp/test_pointer_ssa_corrupt
/tmp/test_pointer_ssa_corrupt > /tmp/pointer_ssa_fault.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/pointer_ssa_fault.log

# 3. Run 3: Clean production build -> pointer_ssa_green.log (Exit 0)
gcc -O2 -Iinclude -I. tests/test_pointer_ssa_intervals.c -o /tmp/test_pointer_ssa
/tmp/test_pointer_ssa > /tmp/pointer_ssa_green.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/pointer_ssa_green.log
