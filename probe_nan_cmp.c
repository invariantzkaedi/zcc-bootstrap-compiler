#include <stdio.h>

static float make_nanf(void) {
    volatile float zero = 0.0f;
    return zero / zero;
}

static double make_nand(void) {
    volatile double zero = 0.0;
    return zero / zero;
}

/* Static folded NaNs */
static float get_static_nanf(void) {
    return 0.0f / 0.0f;
}

static int f_eq(float a, float b) { return a == b; }
static int f_ne(float a, float b) { return a != b; }
static int f_lt(float a, float b) { return a < b; }
static int f_le(float a, float b) { return a <= b; }
static int f_gt(float a, float b) { return a > b; }
static int f_ge(float a, float b) { return a >= b; }

static int d_eq(double a, double b) { return a == b; }
static int d_ne(double a, double b) { return a != b; }
static int d_lt(double a, double b) { return a < b; }
static int d_le(double a, double b) { return a <= b; }
static int d_gt(double a, double b) { return a > b; }
static int d_ge(double a, double b) { return a >= b; }

static void chk(int got, int expected, const char *label, int *pass, int *fail) {
    if (got == expected) {
        printf("[PASS] %s\n", label);
        (*pass)++;
    } else {
        printf("[FAIL] %s  expected=%d got=%d\n", label, expected, got);
        (*fail)++;
    }
}

int main(void) {
    float nanf = make_nanf();
    double nand = make_nand();
    float static_nanf = get_static_nanf();
    int pass = 0;
    int fail = 0;

    /* Standard NaN properties:
       - any comparison with NaN (==, <, <=, >, >=) is false (0).
       - != with NaN is true (1).
    */

    /* Dynamic Float NaN */
    chk(f_eq(nanf, 1.0f), 0, "f_eq(nan, 1.0)", &pass, &fail);
    chk(f_eq(nanf, nanf), 0, "f_eq(nan, nan)", &pass, &fail);
    chk(f_ne(nanf, 1.0f), 1, "f_ne(nan, 1.0)", &pass, &fail);
    chk(f_ne(nanf, nanf), 1, "f_ne(nan, nan)", &pass, &fail);
    chk(f_lt(nanf, 1.0f), 0, "f_lt(nan, 1.0)", &pass, &fail);
    chk(f_lt(1.0f, nanf), 0, "f_lt(1.0, nan)", &pass, &fail);
    chk(f_le(nanf, 1.0f), 0, "f_le(nan, 1.0)", &pass, &fail);
    chk(f_gt(nanf, 1.0f), 0, "f_gt(nan, 1.0)", &pass, &fail);
    chk(f_ge(nanf, 1.0f), 0, "f_ge(nan, 1.0)", &pass, &fail);

    /* Dynamic Double NaN */
    chk(d_eq(nand, 1.0), 0, "d_eq(nan, 1.0)", &pass, &fail);
    chk(d_eq(nand, nand), 0, "d_eq(nan, nan)", &pass, &fail);
    chk(d_ne(nand, 1.0), 1, "d_ne(nan, 1.0)", &pass, &fail);
    chk(d_ne(nand, nand), 1, "d_ne(nan, nan)", &pass, &fail);
    chk(d_lt(nand, 1.0), 0, "d_lt(nan, 1.0)", &pass, &fail);
    chk(d_lt(1.0, nand), 0, "d_lt(1.0, nan)", &pass, &fail);
    chk(d_le(nand, 1.0), 0, "d_le(nan, 1.0)", &pass, &fail);
    chk(d_gt(nand, 1.0), 0, "d_gt(nan, 1.0)", &pass, &fail);
    chk(d_ge(nand, 1.0), 0, "d_ge(nan, 1.0)", &pass, &fail);

    /* Static folded NaNs check */
    chk(static_nanf == 1.0f, 0, "static nan == 1.0", &pass, &fail);
    chk(static_nanf != 1.0f, 1, "static nan != 1.0", &pass, &fail);
    chk(static_nanf < 1.0f, 0, "static nan < 1.0", &pass, &fail);

    printf("=== NaN COMPARISON TEST: %d passed, %d failed ===\n", pass, fail);
    return fail;
}
