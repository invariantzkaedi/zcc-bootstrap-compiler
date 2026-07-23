#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload
gcc --version | head -1
sha256sum src/opt/pointer_ssa.c tests/test_pointer_ssa_intervals.c tests/run_gate_pointer_ssa.sh
./tests/run_gate_pointer_ssa.sh
echo "SCRIPT_EXIT:$?"
for f in baseline fault green; do
    echo "=== $f ==="
    cat /tmp/pointer_ssa_$f.log
done
