/*
 * Regression test for DEFECT-CORE05-03: __builtin_bswap16/32/64
 */
#include <stdio.h>

static unsigned short g_s16 = __builtin_bswap16(0x1234);
static unsigned int g_s32 = __builtin_bswap32(0x12345678U);
static unsigned long long g_s64 = __builtin_bswap64(0x1122334455667788ULL);

int main(void) {
    /* Test static constant evaluation */
    if (g_s16 != 0x3412) {
        printf("FAIL: static __builtin_bswap16 expected 0x3412, got 0x%x\n", (unsigned int)g_s16);
        return 1;
    }
    if (g_s32 != 0x78563412U) {
        printf("FAIL: static __builtin_bswap32 expected 0x78563412, got 0x%x\n", g_s32);
        return 2;
    }
    if (g_s64 != 0x8877665544332211ULL) {
        printf("FAIL: static __builtin_bswap64 expected 0x8877665544332211, got 0x%llx\n", g_s64);
        return 3;
    }

    /* Test runtime evaluation with dynamic variables */
    volatile unsigned short v16 = 0xABCD;
    volatile unsigned int v32 = 0xDEADBEEFU;
    volatile unsigned long long v64 = 0x0123456789ABCDEFULL;

    unsigned short r16 = __builtin_bswap16(v16);
    unsigned int r32 = __builtin_bswap32(v32);
    unsigned long long r64 = __builtin_bswap64(v64);

    if (r16 != 0xCDAB) {
        printf("FAIL: dynamic __builtin_bswap16 expected 0xCDAB, got 0x%x\n", (unsigned int)r16);
        return 4;
    }
    if (r32 != 0xEFBEADDEU) {
        printf("FAIL: dynamic __builtin_bswap32 expected 0xEFBEADDE, got 0x%x\n", r32);
        return 5;
    }
    if (r64 != 0xEFCDAB8967452301ULL) {
        printf("FAIL: dynamic __builtin_bswap64 expected 0xEFCDAB8967452301, got 0x%llx\n", r64);
        return 6;
    }

    /* Test boundary/edge cases */
    if (__builtin_bswap16(0) != 0) return 7;
    if (__builtin_bswap32(0) != 0) return 8;
    if (__builtin_bswap64(0) != 0) return 9;

    if (__builtin_bswap16(0xFFFF) != 0xFFFF) return 10;
    if (__builtin_bswap32(0xFFFFFFFFU) != 0xFFFFFFFFU) return 11;
    if (__builtin_bswap64(0xFFFFFFFFFFFFFFFFULL) != 0xFFFFFFFFFFFFFFFFULL) return 12;

    printf("SUCCESS: __builtin_bswap16/32/64 verified byte-exact\n");
    return 0;
}
