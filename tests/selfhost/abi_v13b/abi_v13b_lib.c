#include <stdio.h>
#include "abi_v13b_types.h"

void zcc_callee_align_test(void) {
    long misalign = check_stack_align();
    printf("CALLEE_ALIGN_MISALIGN:%ld\n", misalign);
}

long zcc_workload(long a, long b, long c, long d, long e, long f, long g, long h) {
    long sum = 0;
    long i;
    for (i = 0; i < 10; i++) {
        sum += (a * i) ^ (b + i) ^ (c * 3) ^ (d * 5) ^ (e * 7) ^ (f * 11) ^ (g * 13) ^ (h * 17);
    }
    return sum;
}
