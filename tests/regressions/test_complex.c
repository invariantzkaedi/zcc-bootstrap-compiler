/* test_complex.c — Step 6 regression suite for ZCC _Complex / <complex.h> */
#include <stdio.h>
#include <assert.h>
#include <complex.h>

_Static_assert(sizeof(float _Complex) == 8, "float _Complex size must be 8");
_Static_assert(sizeof(double _Complex) == 16, "double _Complex size must be 16");
_Static_assert(_Alignof(float _Complex) == 4, "float _Complex alignment must be 4");
_Static_assert(_Alignof(double _Complex) == 8, "double _Complex alignment must be 8");

int main(void) {
    printf("Testing Complex Floating-Point Types (_Complex / <complex.h>)...\n");

    double _Complex c1 = 3.0 + 4.0 * I;
    double _Complex c2 = conj(c1);

    double r1 = creal(c1);
    double i1 = cimag(c1);

    double r2 = creal(c2);
    double i2 = cimag(c2);

    printf("c1 = %.1f + %.1fi\n", r1, i1);
    printf("c2 = conj(c1) = %.1f + %.1fi\n", r2, i2);

    assert(r1 == 3.0);
    assert(i1 == 4.0);
    assert(r2 == 3.0);
    assert(i2 == -4.0);

    /* Test float _Complex precisions */
    float _Complex fc = 1.5f + 2.5f * I;
    assert(crealf(fc) == 1.5f);
    assert(cimagf(fc) == 2.5f);

    printf("[OK] All _Complex verification tests passed cleanly!\n");
    return 0;
}
