#!/bin/sh
cd /mnt/h/__DOWNLOADS/zcc_github_upload
echo "=== Gate 5: ABI suite still passes ==="
for t in ret_int_int ret_sse_sse ret_sse_int ret_int_sse ret_tvalue; do
    ./zcc -c tests/abi/${t}_lib.c -o ${t}.o 2>/dev/null
    gcc tests/abi/${t}_main.c ${t}.o -o ${t}_gate5 2>/dev/null
    ./${t}_gate5 > /dev/null 2>&1 && echo "PASS: $t" || echo "FAIL: $t"
done

echo "=== Gate 6: Peak RSS under bound ==="
# Compile zcc.c under memory-monitored invocation
/usr/bin/time -v ./zcc zcc.c -o /tmp/zcc_test 2>&1 | grep "Maximum resident"
