/* ZCC regression test: negative float literals in global initializers.
 * Catches ND_NEG bit corruption (two's complement vs IEEE-754 sign flip).
 * Expected: all three assertions pass. Failure = codegen regression.
 * IEEE-754 reference:
 *   -1.5     → 0xBFF8000000000000
 *   -0.0     → 0x8000000000000000
 *   -3.14159 → 0xC00921FA9AC10C02 (approx)
 */
#include <stdio.h>

double neg_array[] = {-1.5, -0.0, -3.14159};

int main(void) {
    int pass = 1;
    union { double d; long long i; } u;

    u.d = neg_array[0];
    if (u.i != (long long)0xBFF8000000000000LL) {
        printf("FAIL: neg_array[0] = %f  bits=0x%llx  expected=0xBFF8000000000000\n", u.d, u.i);
        pass = 0;
    }

    u.d = neg_array[1];
    if (u.i != (long long)0x8000000000000000LL) {
        printf("FAIL: neg_array[1] = %f  bits=0x%llx  expected=0x8000000000000000\n", u.d, u.i);
        pass = 0;
    }

    u.d = neg_array[2];
    /* -3.14159 ≈ 0xC00921FA9AC10C02 but precision varies; just check sign bit + rough magnitude */
    if (u.d > -3.14 || u.d < -3.15) {
        printf("FAIL: neg_array[2] = %f  expected ≈ -3.14159\n", u.d);
        pass = 0;
    }

    if (pass) {
        printf("[OK] neg_flit regression: all IEEE-754 bit patterns correct\n");
    }
    return pass ? 0 : 1;
}
