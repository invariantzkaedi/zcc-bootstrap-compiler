#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc

# Does LLONG_MAX resolve when injected via a system include?
echo '#include <stdio.h>
#if defined(LLONG_MAX)
int HAS_LLONG_MAX = 1;
#else
int NO_LLONG_MAX = 1;
#endif' > /tmp/via_include.c
echo '=== Via system include ==='
$ZCC --pp-only /tmp/via_include.c 2>/dev/null | grep -E 'HAS|NO'

# Does it resolve as a bare define (no include)?
echo '#if defined(LLONG_MAX)
int HAS_LLONG_MAX = 1;
#else
int NO_LLONG_MAX = 1;
#endif' > /tmp/bare_check.c
echo '=== Bare (no include) ==='
$ZCC --pp-only /tmp/bare_check.c 2>/dev/null | grep -E 'HAS|NO'

# Show what LLONG define looks like in the stub text actually emitted
echo '=== Raw stub output ==='
echo '#include <stddef.h>' > /tmp/stub_emit.c
$ZCC --pp-only /tmp/stub_emit.c 2>/dev/null | grep -E 'LLONG|SHRT|UCHAR' | head -10
