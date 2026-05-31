/* tests/regressions/repro_callee_saved_pressure.c */
#include <stdio.h>

/* Deep recursive driver to force nested variable caching */
long complex_leaf(long a, long b, long c, long d, long e, long f) {
    return a + b + c + d + e + f;
}

long force_register_pressure(long count) {
    /* Declare 8 distinct active variables to exceed physical register limit */
    long r1 = count * 2;
    long r2 = count + 3;
    long r3 = count - 4;
    long r4 = count ^ 5;
    long r5 = count | 6;
    long r6 = count & 7;
    long r7 = count + r1;
    long r8 = count - r2;

    for (long i = 0; i < count; i++) {
        /* Force function call which clobbers registers if caller didn't preserve */
        r1 += complex_leaf(r1, r2, r3, r4, r5, r6);
        r2 ^= r7 - r8;
    }

    return r1 ^ r2 ^ r3 ^ r4 ^ r5 ^ r6 ^ r7 ^ r8;
}

int main(void) {
    long result = force_register_pressure(10);
    printf("Pressure Result: %ld\n", result);
    return 0;
}
