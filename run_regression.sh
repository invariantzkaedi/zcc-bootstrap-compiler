#!/bin/bash
cd /mnt/h/__DOWNLOADS/zcc_github_upload
FAIL=0
for p in probe_neg_zero_truth.c probe_final_four.c probe_float_cmp_v2.c probe_unproved_quick.c probe_narrow_family.c probe_narrow_int_family.c probe_static_init.c probe_static_init_edges.c probe_static_init_alldouble.c; do
    echo "=== $p ==="
    gcc -O0 -w "$p" -o /tmp/pg 2>/dev/null
    if ./zcc2 "$p" -o /tmp/pz.s 2>/dev/null && gcc -fno-pie -no-pie -O0 -w /tmp/pz.s -o /tmp/pz 2>/dev/null; then
        if diff <(/tmp/pg) <(/tmp/pz) > /dev/null; then
            echo "IDENTICAL"
        else
            echo "DIFF:"
            diff <(/tmp/pg) <(/tmp/pz)
            FAIL=1
        fi
    else
        echo "COMPILE FAILED"
        FAIL=1
    fi
done
echo "=== Regression result: $FAIL ==="
exit $FAIL
