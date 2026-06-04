#!/bin/bash
cd /mnt/g/zccMAIN/zcc
mkdir -p probes/out_zcc

for i in 1 2 3 4 5 6 7 8; do
    echo "=== Processing Cat $i ==="
    # Clean up old artifacts
    rm -f probes/probe_fp_cat${i}_zcc.s /tmp/probe_fp_cat${i}_zcc_bin probes/out_zcc/cat${i}.txt probes/out_zcc/cat${i}_stderr.txt
    
    # Run ZCC2 compiler, redirecting stderr to a temp file
    ./zcc2 -I. -Izcc_sys_includes -Izcc-libc probes/probe_fp_cat${i}.c -o probes/probe_fp_cat${i}_zcc.s 2> /tmp/zcc_err.txt
    ZCC_STATUS=$?
    
    # Check if compilation generated an error message (even if exit code was 0, or check exit code)
    # Note: ZCC sometimes outputs "error: ..." but returns exit code 0 or continues. Let's inspect /tmp/zcc_err.txt.
    if [ $ZCC_STATUS -ne 0 ] || grep -q "error:" /tmp/zcc_err.txt; then
        echo "Cat $i ZCC compile failed."
        cp /tmp/zcc_err.txt probes/out_zcc/cat${i}_stderr.txt
    else
        # Try linking
        gcc -no-pie -w probes/probe_fp_cat${i}_zcc.s -lm -o /tmp/probe_fp_cat${i}_zcc_bin 2> /tmp/link_err.txt
        LINK_STATUS=$?
        if [ $LINK_STATUS -ne 0 ]; then
            echo "Cat $i linking failed."
            cp /tmp/link_err.txt probes/out_zcc/cat${i}_stderr.txt
        else
            # Try running
            /tmp/probe_fp_cat${i}_zcc_bin > probes/out_zcc/cat${i}.txt 2> /tmp/run_err.txt
            RUN_STATUS=$?
            if [ $RUN_STATUS -ne 0 ]; then
                echo "Cat $i run failed."
                cp /tmp/run_err.txt probes/out_zcc/cat${i}_stderr.txt
            fi
        fi
    fi
done
echo "ZCC RUN COMPLETE"
