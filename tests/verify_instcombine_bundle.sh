#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload
gcc --version | head -1
sha256sum tests/test_instcombine_oracle.c tests/run_gate_instcombine.sh
./tests/run_gate_instcombine.sh
echo "SCRIPT_EXIT:$?"
for f in baseline fault green; do
    echo "=== $f ==="
    cat /tmp/instcombine_$f.log
done
