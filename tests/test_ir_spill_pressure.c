#include <stdio.h>
#include <stdint.h>

/* High-pressure register allocator test (pressure > 7 physical registers) */
int64_t compute_high_pressure(int64_t seed, int iterations) {
    int64_t a0 = seed + 1;
    int64_t a1 = seed ^ 0x123456789ABCDEF0LL;
    int64_t a2 = seed * 3 + 7;
    int64_t a3 = (seed << 5) | (seed >> 59);
    int64_t a4 = seed - 0x9E3779B9LL;
    int64_t a5 = ~seed + 0x85EBCA6BLL;
    int64_t a6 = seed * 0xC2B2AE35LL;
    int64_t a7 = (seed >> 13) ^ (seed << 51);
    int64_t b0 = a0 + a1;
    int64_t b1 = a1 * a2;
    int64_t b2 = a2 ^ a3;
    int64_t b3 = a3 - a4;
    int64_t b4 = a4 + a5 * 2;
    int64_t b5 = a5 ^ (a6 + 1);
    int64_t b6 = a6 * 5 - a7;
    int64_t b7 = a7 + (a0 ^ a3);

    for (int i = 0; i < iterations; i++) {
        int64_t t0 = a0 + b0 + (int64_t)i;
        int64_t t1 = a1 ^ b1;
        int64_t t2 = a2 * b2 + 3;
        int64_t t3 = a3 - b3;
        int64_t t4 = a4 + b4 * (int64_t)(i + 1);
        int64_t t5 = a5 ^ b5;
        int64_t t6 = a6 + b6;
        int64_t t7 = a7 ^ b7;

        a0 = (t0 << 3) | (t0 >> 61);
        a1 = t1 * 0x9E3779B9LL;
        a2 = t2 + a0;
        a3 = t3 ^ a1;
        a4 = t4 - a2;
        a5 = t5 + a3;
        a6 = t6 ^ a4;
        a7 = t7 * 0x85EBCA6BLL;

        b0 = a0 + b7;
        b1 = a1 ^ b6;
        b2 = a2 * 3;
        b3 = a3 + b4;
        b4 = a4 ^ b3;
        b5 = a5 - b2;
        b6 = a6 + b1;
        b7 = a7 ^ b0;
    }

    return a0 ^ a1 ^ a2 ^ a3 ^ a4 ^ a5 ^ a6 ^ a7 ^
           b0 ^ b1 ^ b2 ^ b3 ^ b4 ^ b5 ^ b6 ^ b7;
}

int main(void) {
    int64_t res = compute_high_pressure(0xDEADBEEFCAFE0001LL, 50);
    printf("HIGH_PRESSURE_HASH=0x%016llx\n", (unsigned long long)res);
    return (res != 0) ? 0 : 1;
}
