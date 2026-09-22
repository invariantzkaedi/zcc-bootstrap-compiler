/* tests/test_glibc_dual_headers.c */
/* Verification test for LIMIT-005: Raw glibc stdio.h + stdlib.h ingestion */
#include </usr/include/stdio.h>
#include </usr/include/stdlib.h>

int main(void) {
    char *buf = (char *)malloc(64);
    if (!buf) return 1;
    snprintf(buf, 64, "GLIBC DUAL INGESTION: stdio.h + stdlib.h VERIFIED");
    printf("%s\n", buf);
    free(buf);
    return 0;
}
