/* test_fp_casts.c — Hardened FP-to-integer conversion test */
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

int main(void) {
    volatile float f_pos = 12345.67f;
    volatile float f_neg = -12345.67f;
    volatile double d_pos = 2147483647.0;
    volatile double d_neg = -98765.4321;
    volatile double d_u32 = 4294967295.0;
    volatile double d_u64 = 9223372036854775808.0; /* 2^63, exact */

    int32_t i32_pos = (int32_t)f_pos;
    int32_t i32_neg = (int32_t)f_neg;
    int64_t i64 = (int64_t)d_neg;
    uint32_t u32 = (uint32_t)d_u32;
    uint64_t u64 = (uint64_t)d_u64;

    assert(i32_pos == 12345);
    assert(i32_neg == -12345);
    assert(i64 == -98765);
    assert(u32 == UINT32_MAX);
    assert(u64 == UINT64_C(9223372036854775808));

    puts("[OK] FP-to-integer conversion tests passed!");
    return 0;
}
