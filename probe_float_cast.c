#include <stdio.h>

static float get_float(float val) {
    volatile float v = val;
    return v;
}

static double get_double(double val) {
    volatile double v = val;
    return v;
}

/* Static folded casts */
static double get_static_extended(void) {
    return (double)3.14f;
}

static float get_static_truncated(void) {
    return (float)3.141592653589793;
}

static double extend_float(float f) {
    return (double)f;
}

static float truncate_double(double d) {
    return (float)d;
}

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
    int pass = 0;
    int fail = 0;

    /* Dynamic conversion checks */
    float f_val = get_float(1.5f);
    double d_val = get_double(2.25);

    double ext = extend_float(f_val);
    float trunc = truncate_double(d_val);

    chk(ext == 1.5, 1, "extend_float(1.5f) == 1.5", &pass, &fail);
    chk(trunc == 2.25f, 1, "truncate_double(2.25) == 2.25f", &pass, &fail);

    /* Precision rounding verification */
    double d_pi = 3.141592653589793;
    float f_pi = truncate_double(d_pi);
    chk(f_pi == (float)3.141592653589793, 1, "precise truncation rounded correctly", &pass, &fail);

    /* Static compile-time folding checks */
    double folded_ext = get_static_extended();
    float folded_trunc = get_static_truncated();

    chk(folded_ext == (double)3.14f, 1, "static extended fold == (double)3.14f", &pass, &fail);
    chk(folded_trunc == (float)3.141592653589793, 1, "static truncated fold rounded correctly", &pass, &fail);

    printf("=== FLOAT CAST TEST: %d passed, %d failed ===\n", pass, fail);
    return fail;
}
