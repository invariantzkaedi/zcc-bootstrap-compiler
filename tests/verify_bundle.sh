#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload
gcc --version | head -1
sha256sum src/evm/yul_weaver.c tests/test_evm_weaver_diff.c tests/run_gate4evm.sh
./tests/run_gate4evm.sh
echo "SCRIPT_EXIT:$?"
for f in baseline fault green; do
    echo "=== $f ==="
    cat /tmp/gate4evm_$f.log
done
