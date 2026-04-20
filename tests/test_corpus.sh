#!/usr/bin/env bash
# Test 1: ZCC Corpus Regression (fixed: check ASM file has content, success msg on stderr)
ZCC=/mnt/h/__DOWNLOADS/selforglinux
PASS=0; FAIL=0; TOTAL=0
FAIL_LIST=""

for f in "$ZCC"/test_*.c; do
  TOTAL=$((TOTAL+1))
  name=$(basename "$f")
  out_s="/tmp/_zcc_corp_${name%.c}.s"
  
  # zcc2 writes progress to stdout/stderr, ASM to -o file
  if timeout 8 "$ZCC/zcc2" "$f" -o "$out_s" 2>&1 | grep -q "ZCC Engine Compilation Terminated Successfully"; then
    # Double-check: output ASM has actual content
    if [ -s "$out_s" ] && grep -q ".section" "$out_s" 2>/dev/null; then
      PASS=$((PASS+1))
    else
      FAIL=$((FAIL+1))
      FAIL_LIST="$FAIL_LIST $name"
    fi
  else
    FAIL=$((FAIL+1))
    FAIL_LIST="$FAIL_LIST $name"
  fi
  rm -f "$out_s"
done

PCT=$(awk "BEGIN{printf \"%.1f\", $PASS*100/$TOTAL}")
echo "CORPUS_TOTAL=$TOTAL"
echo "CORPUS_PASS=$PASS"
echo "CORPUS_FAIL=$FAIL"
echo "CORPUS_PCT=$PCT%"

if [ $FAIL -gt 0 ]; then
  echo "FAIL_LIST=[ $(echo $FAIL_LIST | tr ' ' '\n' | head -10 | tr '\n' ' ')]"
fi

[ $FAIL -eq 0 ] && echo "STATUS=CLEAN" || echo "STATUS=DEGRADED ($FAIL failures)"
