#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload

# 1. Run 1: Harness-first baseline build (no rules registered) -> instcombine_baseline.log (Exit 2)
gcc -O2 -DTEST_BASELINE_BUILD tests/test_instcombine_oracle.c -o /tmp/test_instcombine_oracle_base
/tmp/test_instcombine_oracle_base > /tmp/instcombine_baseline.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/instcombine_baseline.log

# 2. Run 2: Fault-injected rule (-DFAULT_INJECT_INSTCOMBINE) -> instcombine_fault.log (Exit 1)
gcc -O2 -DFAULT_INJECT_INSTCOMBINE tests/test_instcombine_oracle.c -o /tmp/test_instcombine_oracle_corrupt
/tmp/test_instcombine_oracle_corrupt > /tmp/instcombine_fault.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/instcombine_fault.log

# 3. Run 3: Real InstCombine rules clean build -> instcombine_green.log (Exit 0)
gcc -O2 tests/test_instcombine_oracle.c -o /tmp/test_instcombine_oracle
/tmp/test_instcombine_oracle > /tmp/instcombine_green.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/instcombine_green.log
