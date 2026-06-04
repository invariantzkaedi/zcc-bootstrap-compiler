#include <stdio.h>
#include <math.h>
#include <float.h>

int main(void) {
    double p_inf = INFINITY;
    double p_nan = NAN;
    double p_zero = 0.0;
    double p_nzero = -0.0;
    double p_flt_max = FLT_MAX;
    double p_dbl_max = DBL_MAX;

    /* %f formatting */
    printf("f_inf: %f\n", p_inf);
    printf("f_nan: %f\n", p_nan);
    printf("f_zero: %f\n", p_zero);
    printf("f_nzero: %f\n", p_nzero);
    printf("f_flt_max: %f\n", p_flt_max);

    /* %e formatting */
    printf("e_inf: %e\n", p_inf);
    printf("e_nan: %e\n", p_nan);
    printf("e_zero: %e\n", p_zero);
    printf("e_nzero: %e\n", p_nzero);
    printf("e_flt_max: %e\n", p_flt_max);
    printf("e_dbl_max: %e\n", p_dbl_max);

    /* %g formatting */
    printf("g_inf: %g\n", p_inf);
    printf("g_nan: %g\n", p_nan);
    printf("g_zero: %g\n", p_zero);
    printf("g_nzero: %g\n", p_nzero);
    printf("g_flt_max: %g\n", p_flt_max);
    printf("g_dbl_max: %g\n", p_dbl_max);

    /* %a formatting */
    printf("a_zero: %a\n", p_zero);
    printf("a_nzero: %a\n", p_nzero);

    /* printf of float vs double (varargs promotion verification) */
    float val_f = 1.234f;
    double val_d = 1.234;
    printf("printf_f: %f\n", val_f);
    printf("printf_d: %f\n", val_d);

    return 0;
}
