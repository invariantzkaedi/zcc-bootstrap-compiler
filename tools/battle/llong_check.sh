#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc
echo '#if defined(LLONG_MAX)
int HAS_LLONG_MAX = 1;
#else
int NO_LLONG_MAX = 1;
#endif' > /tmp/llong_check.c
$ZCC --pp-only /tmp/llong_check.c 2>/dev/null
