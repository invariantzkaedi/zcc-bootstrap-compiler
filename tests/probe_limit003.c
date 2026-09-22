#include <stdio.h>
#include <complex.h>

int main(void) {
    /* Double complex */
    _Complex double a = 3.0 + 4.0 * 1.0fi;
    _Complex double b = 1.0 + 2.0 * 1.0fi;
    _Complex double c_add = a + b;
    _Complex double c_sub = a - b;
    _Complex double c_mul = a * b;
    _Complex double c_div = a / b;
    _Complex double c_conj = conj(a);

    /* Pure double imaginary literals (4.0i, 2.0i) */
    _Complex double x = 5.0 + 4.0i;
    _Complex double y = 2.0 + 1.0i;
    _Complex double xy_add = x + y;
    _Complex double xy_mul = x * y;

    /* Float complex */
    _Complex float fa = 6.0f + 8.0fi;
    _Complex float fb = 2.0f + 4.0fi;
    _Complex float fc_add = fa + fb;
    _Complex float fc_sub = fa - fb;
    _Complex float fc_mul = fa * fb;
    _Complex float fc_div = fa / fb;
    _Complex float fc_conj = conjf(fa);

    printf("add: %f + %fi\n", creal(c_add), cimag(c_add));
    printf("sub: %f + %fi\n", creal(c_sub), cimag(c_sub));
    printf("mul: %f + %fi\n", creal(c_mul), cimag(c_mul));
    printf("div: %f + %fi\n", creal(c_div), cimag(c_div));
    printf("conj: %f + %fi\n", creal(c_conj), cimag(c_conj));

    printf("xy_add: %f + %fi\n", creal(xy_add), cimag(xy_add));
    printf("xy_mul: %f + %fi\n", creal(xy_mul), cimag(xy_mul));

    printf("f_add: %f + %fi\n", (double)crealf(fc_add), (double)cimagf(fc_add));
    printf("f_sub: %f + %fi\n", (double)crealf(fc_sub), (double)cimagf(fc_sub));
    printf("f_mul: %f + %fi\n", (double)crealf(fc_mul), (double)cimagf(fc_mul));
    printf("f_div: %f + %fi\n", (double)crealf(fc_div), (double)cimagf(fc_div));
    printf("f_conj: %f + %fi\n", (double)crealf(fc_conj), (double)cimagf(fc_conj));
    return 0;
}
