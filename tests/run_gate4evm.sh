#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload

# 1. Run 1: Unmodified Baseline Build -> gate4evm_baseline.log
gcc -O2 -DTEST_BASELINE_BUILD -Iinclude -I. tests/test_evm_weaver_diff.c src/evm/yul_weaver.c src/evm/yul_fixed_point.c -o /tmp/test_evm_weaver_diff_base
/tmp/test_evm_weaver_diff_base > /tmp/gate4evm_baseline.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/gate4evm_baseline.log

# 2. Run 2: Real Fault Injection Build (-DFAULT_INJECT_BAD_SWAP) -> gate4evm_fault.log
gcc -O2 -DFAULT_INJECT_BAD_SWAP -Iinclude -I. tests/test_evm_weaver_diff.c src/evm/yul_weaver.c src/evm/yul_fixed_point.c -o /tmp/test_evm_weaver_diff_corrupt
/tmp/test_evm_weaver_diff_corrupt > /tmp/gate4evm_fault.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/gate4evm_fault.log

# 3. Run 3: Clean Optimized Build -> gate4evm_green.log
gcc -O2 -Iinclude -I. tests/test_evm_weaver_diff.c src/evm/yul_weaver.c src/evm/yul_fixed_point.c -o /tmp/test_evm_weaver_diff
/tmp/test_evm_weaver_diff > /tmp/gate4evm_green.log 2>&1
ret=$?
echo "EXIT:$ret" >> /tmp/gate4evm_green.log
