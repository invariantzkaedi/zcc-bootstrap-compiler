#include <stdio.h>
#include <math.h>
#include <float.h>

struct Cat2Struct {
    float x;
    double y;
    float z;
};

int main(void) {
    /* float/double with literal values */
    float g_lit_f1 = 3.14f;
    float g_lit_f2 = -0.0f;
    double g_lit_d1 = 2.718281828459;
    double g_lit_d2 = -1.2345e-10;

    /* float/double with arithmetic expressions */
    float g_expr_f1 = 1.0f / 3.0f;
    float g_expr_f2 = -2.5f * 4.0f;
    double g_expr_d1 = 2.0 * 3.141592653589793;
    double g_expr_d2 = 100.0 - 0.005;

    /* INFINITY, NAN, -INFINITY, -NAN */
    float g_lim_inf = INFINITY;
    float g_lim_ninf = -INFINITY;
    float g_lim_nan = NAN;
    float g_lim_nnan = -NAN;

    /* FLT_MAX, DBL_MAX, FLT_MIN, DBL_MIN */
    float g_max_f = FLT_MAX;
    float g_min_f = FLT_MIN;
    double g_max_d = DBL_MAX;
    double g_min_d = DBL_MIN;

    /* Arrays of float with mixed literal/expr initializers */
    float g_arr_f[4] = { 1.5f, 1.0f / 4.0f, -3.0f, 0.0f / 1.0f };

    /* Struct members initialized to float expressions */
    struct Cat2Struct g_str = { 1.0f / 5.0f, 1.0 + 2.0, -0.0f };

    printf("g_lit_f1: %f\n", g_lit_f1);
    printf("g_lit_f2: %f\n", g_lit_f2);
    printf("g_lit_d1: %f\n", g_lit_d1);
    printf("g_lit_d2: %e\n", g_lit_d2);

    printf("g_expr_f1: %f\n", g_expr_f1);
    printf("g_expr_f2: %f\n", g_expr_f2);
    printf("g_expr_d1: %f\n", g_expr_d1);
    printf("g_expr_d2: %f\n", g_expr_d2);

    printf("g_lim_inf: %f\n", g_lim_inf);
    printf("g_lim_ninf: %f\n", g_lim_ninf);
    printf("g_lim_nan: %f\n", g_lim_nan);
    printf("g_lim_nnan: %f\n", g_lim_nnan);

    printf("g_max_f: %e\n", g_max_f);
    printf("g_min_f: %e\n", g_min_f);
    printf("g_max_d: %e\n", g_max_d);
    printf("g_min_d: %e\n", g_min_d);

    printf("g_arr_f: %f, %f, %f, %f\n", g_arr_f[0], g_arr_f[1], g_arr_f[2], g_arr_f[3]);
    printf("g_str: %f, %f, %f\n", g_str.x, g_str.y, g_str.z);

    return 0;
}
