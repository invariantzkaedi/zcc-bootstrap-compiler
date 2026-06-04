#include <stdio.h>
#include <math.h>
#include <float.h>

int main(void) {
    float f1 = 12.5f;
    float f2 = 4.0f;
    double d1 = 123.456;
    double d2 = 0.5;

    /* Basic add, sub, mul, div in float and double */
    printf("f_add: %f\n", f1 + f2);
    printf("f_sub: %f\n", f1 - f2);
    printf("f_mul: %f\n", f1 * f2);
    printf("f_div: %f\n", f1 / f2);

    printf("d_add: %f\n", d1 + d2);
    printf("d_sub: %f\n", d1 - d2);
    printf("d_mul: %f\n", d1 * d2);
    printf("d_div: %f\n", d1 / d2);

    /* Mixed float/double promotion */
    printf("mixed_add: %f\n", f1 + d1);
    printf("mixed_sub: %f\n", d1 - f2);
    printf("mixed_mul: %f\n", f1 * d2);
    printf("mixed_div: %f\n", d1 / f2);

    /* Integer-to-float and float-to-integer casts */
    int i1 = 42;
    float f_from_i = (float)i1;
    double d_from_i = (double)i1;
    int i_from_f = (int)f1;
    int i_from_d = (int)d1;
    printf("f_from_i: %f\n", f_from_i);
    printf("d_from_i: %f\n", d_from_i);
    printf("i_from_f: %d\n", i_from_f);
    printf("i_from_d: %d\n", i_from_d);

    /* float-to-double and double-to-float casts */
    double d_from_f = (double)f1;
    float f_from_d = (float)d1;
    printf("d_from_f: %f\n", d_from_f);
    printf("f_from_d: %f\n", f_from_d);

    /* Overflow to INFINITY */
    float f_overflow = FLT_MAX * 10.0f;
    double d_overflow = DBL_MAX * 10.0;
    printf("f_overflow: %f\n", f_overflow);
    printf("d_overflow: %f\n", d_overflow);

    /* Underflow to 0/denormal */
    float f_underflow = FLT_MIN / 1e10f;
    double d_underflow = DBL_MIN / 1e20;
    printf("f_underflow: %e\n", f_underflow);
    printf("d_underflow: %e\n", d_underflow);

    /* NaN propagation */
    float nan_add = NAN + 1.0f;
    float nan_mul = NAN * 0.0f;
    printf("nan_add: %f\n", nan_add);
    printf("nan_mul: %f\n", nan_mul);

    /* Signed zero comparisons */
    float pzero = 0.0f;
    float nzero = -0.0f;
    printf("pzero == nzero: %d\n", pzero == nzero);
    printf("pzero != nzero: %d\n", pzero != nzero);

    return 0;
}
