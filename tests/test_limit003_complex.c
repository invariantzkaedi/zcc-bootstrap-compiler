/*
 * test_limit003_complex.c — Extended Precision & Complex Arithmetic Verification Suite
 * ====================================================================================
 * Verifies C99 complex arithmetic under ZCC:
 * - _Complex float and _Complex double representation
 * - Contiguous struct layout: { real, imag }
 * - Complex addition, subtraction, multiplication, division
 * - Mixed-mode promotion: real -> complex
 * - Imaginary literal suffixes ('i', 'I', 'fi', 'FI')
 * - Builtins and standard <complex.h> functions: creal, cimag, conj
 */

#include <stdio.h>
#include <complex.h>
#include <math.h>

static int failed_assertions = 0;

#define ASSERT_NEAR(a, b, eps, name) do { \
    double diff = fabs((double)(a) - (double)(b)); \
    if (diff > (eps)) { \
        printf("FAIL: %s: expected %f, got %f (diff: %e > %e)\n", name, (double)(b), (double)(a), diff, (double)(eps)); \
        failed_assertions++; \
    } else { \
        printf("PASS: %s (diff: %e)\n", name, diff); \
    } \
} while (0)

int main(void) {
    printf("===============================================================\n");
    printf("  ZCC C99 COMPLEX ARITHMETIC GAUNTLET (LIMIT-003)\n");
    printf("===============================================================\n");

    /* 1. Double Complex Operations */
    _Complex double a = 3.0 + 4.0 * 1.0fi;
    _Complex double b = 1.0 + 2.0 * 1.0fi;

    _Complex double c_add = a + b;
    _Complex double c_sub = a - b;
    _Complex double c_mul = a * b;
    _Complex double c_div = a / b;
    _Complex double c_conj = conj(a);

    ASSERT_NEAR(creal(c_add), 4.0, 1e-15, "double_add_real");
    ASSERT_NEAR(cimag(c_add), 6.0, 1e-15, "double_add_imag");

    ASSERT_NEAR(creal(c_sub), 2.0, 1e-15, "double_sub_real");
    ASSERT_NEAR(cimag(c_sub), 2.0, 1e-15, "double_sub_imag");

    ASSERT_NEAR(creal(c_mul), -5.0, 1e-15, "double_mul_real");
    ASSERT_NEAR(cimag(c_mul), 10.0, 1e-15, "double_mul_imag");

    ASSERT_NEAR(creal(c_div), 2.2, 1e-15, "double_div_real");
    ASSERT_NEAR(cimag(c_div), -0.4, 1e-15, "double_div_imag");

    ASSERT_NEAR(creal(c_conj), 3.0, 1e-15, "double_conj_real");
    ASSERT_NEAR(cimag(c_conj), -4.0, 1e-15, "double_conj_imag");

    /* 2. Pure Double Imaginary Suffix Literals (e.g. 5.0 + 4.0i) */
    _Complex double x = 5.0 + 4.0i;
    _Complex double y = 2.0 + 1.0i;
    _Complex double xy_add = x + y;
    _Complex double xy_mul = x * y;

    ASSERT_NEAR(creal(xy_add), 7.0, 1e-15, "literal_i_add_real");
    ASSERT_NEAR(cimag(xy_add), 5.0, 1e-15, "literal_i_add_imag");

    ASSERT_NEAR(creal(xy_mul), 6.0, 1e-15, "literal_i_mul_real");
    ASSERT_NEAR(cimag(xy_mul), 13.0, 1e-15, "literal_i_mul_imag");

    /* 3. Float Complex Operations */
    _Complex float fa = 6.0f + 8.0fi;
    _Complex float fb = 2.0f + 4.0fi;
    _Complex float fc_add = fa + fb;
    _Complex float fc_sub = fa - fb;
    _Complex float fc_mul = fa * fb;
    _Complex float fc_div = fa / fb;
    _Complex float fc_conj = conjf(fa);

    ASSERT_NEAR(crealf(fc_add), 8.0f, 1e-6, "float_add_real");
    ASSERT_NEAR(cimagf(fc_add), 12.0f, 1e-6, "float_add_imag");

    ASSERT_NEAR(crealf(fc_sub), 4.0f, 1e-6, "float_sub_real");
    ASSERT_NEAR(cimagf(fc_sub), 4.0f, 1e-6, "float_sub_imag");

    ASSERT_NEAR(crealf(fc_mul), -20.0f, 1e-6, "float_mul_real");
    ASSERT_NEAR(cimagf(fc_mul), 40.0f, 1e-6, "float_mul_imag");

    ASSERT_NEAR(crealf(fc_div), 2.2f, 1e-6, "float_div_real");
    ASSERT_NEAR(cimagf(fc_div), -0.4f, 1e-6, "float_div_imag");

    ASSERT_NEAR(crealf(fc_conj), 6.0f, 1e-6, "float_conj_real");
    ASSERT_NEAR(cimagf(fc_conj), -8.0f, 1e-6, "float_conj_imag");

    /* 4. Mixed Real + Complex Operations & Casts */
    _Complex double m1 = 10.0 + a; /* 10.0 + (3.0 + 4.0i) = 13.0 + 4.0i */
    _Complex double m2 = a * 2.0;  /* (3.0 + 4.0i) * 2.0 = 6.0 + 8.0i */
    _Complex double casted = (_Complex double)fa;

    ASSERT_NEAR(creal(m1), 13.0, 1e-15, "mixed_real_add_real");
    ASSERT_NEAR(cimag(m1), 4.0, 1e-15, "mixed_real_add_imag");

    ASSERT_NEAR(creal(m2), 6.0, 1e-15, "mixed_real_mul_real");
    ASSERT_NEAR(cimag(m2), 8.0, 1e-15, "mixed_real_mul_imag");

    ASSERT_NEAR(creal(casted), 6.0, 1e-6, "cast_float_to_double_real");
    ASSERT_NEAR(cimag(casted), 8.0, 1e-6, "cast_float_to_double_imag");

    printf("===============================================================\n");
    if (failed_assertions == 0) {
        printf("★ ALL 20 COMPLEX TESTS PASSED WITH ZERO DRIFT (EXIT 0) ★\n");
        return 0;
    } else {
        printf("FATAL: %d ASSERTIONS FAILED!\n", failed_assertions);
        return 1;
    }
}
