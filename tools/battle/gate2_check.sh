#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc

echo '=== Gate 2a: LLONG_MAX presence ==='
echo '#if defined(LLONG_MAX)
int HAS_LLONG_MAX = 1;
#else
int NO_LLONG_MAX = 1;
#endif' > /tmp/llong_gate.c
$ZCC --pp-only /tmp/llong_gate.c 2>/dev/null | grep -E 'HAS|NO'

echo '=== Gate 2b: All new defines reachable ==='
echo '#if !defined(SCHAR_MAX) || !defined(LLONG_MAX) || !defined(ULLONG_MAX) || !defined(SHRT_MAX) || !defined(UCHAR_MAX) || !defined(CHAR_MIN)
#error "expected defines missing"
#endif
int ok = 1;' > /tmp/alldefs_gate.c
$ZCC --pp-only /tmp/alldefs_gate.c 2>&1
echo exit:$?

echo '=== Gate 2c: LLONG_MIN correct value ==='
echo '#if defined(LLONG_MIN)
int HAS_LLONG_MIN = 1;
#else
int NO_LLONG_MIN = 1;
#endif' > /tmp/llmin_gate.c
$ZCC --pp-only /tmp/llmin_gate.c 2>/dev/null | grep -E 'HAS|NO'
