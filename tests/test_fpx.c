/*
 * test_fpx.c — Validation suite for zkaedi_fpx library
 *
 * Tests: conversion, arithmetic, rounding, special values, cross-format.
 * Must produce identical output when compiled with GCC and ZCC.
 *
 * (c) 2026 ZKAEDI Systems — v0.10
 */

#include "zkaedi_fpx.h"
#include <stdio.h>
#include <math.h>

static int g_pass = 0;
static int g_fail = 0;

static void check_f24(const char *name, fp24_t got, float expected) {
    float gf = fp24_to_float(got);
    float diff = gf - expected;
    if (diff < 0) diff = -diff;

    /* fp24 has ~4.5 decimal digits — allow 0.01% relative error */
    float tol = expected;
    if (tol < 0) tol = -tol;
    tol = tol * 0.0001f;
    if (tol < 1e-10f) tol = 1e-10f;

    if (diff <= tol) {
        g_pass++;
    } else {
        printf("FAIL %s: got %.7g, expected %.7g (diff=%.7g)\n",
               name, gf, expected, diff);
        g_fail++;
    }
}

static void check_f48(const char *name, fp48_t got, double expected) {
    double gd = fp48_to_double(got);
    double diff = gd - expected;
    if (diff < 0) diff = -diff;

    double tol = expected;
    if (tol < 0) tol = -tol;
    tol = tol * 1e-10;
    if (tol < 1e-20) tol = 1e-20;

    if (diff <= tol) {
        g_pass++;
    } else {
        printf("FAIL %s: got %.15g, expected %.15g (diff=%.15g)\n",
               name, gd, expected, diff);
        g_fail++;
    }
}

static void check_int(const char *name, int got, int expected) {
    if (got == expected) {
        g_pass++;
    } else {
        printf("FAIL %s: got %d, expected %d\n", name, got, expected);
        g_fail++;
    }
}

static void check_bits24(const char *name, fp24_t got, unsigned int expected) {
    unsigned int gb = fp24_raw(got);
    if (gb == expected) {
        g_pass++;
    } else {
        printf("FAIL %s: bits=0x%06X, expected=0x%06X\n", name, gb, expected);
        g_fail++;
    }
}

int main(void) {
    fp24_t a24, b24, c24;
    fp48_t a48, b48, c48;

    printf("=== ZKAEDI FPX Test Suite ===\n\n");

    /* ------------------------------------------------------------ */
    /* fp24 Conversion Tests                                         */
    /* ------------------------------------------------------------ */
    printf("[fp24] Conversion tests\n");

    a24 = fp24_from_float(1.0f);
    check_f24("fp24(1.0)", a24, 1.0f);

    a24 = fp24_from_float(-1.0f);
    check_f24("fp24(-1.0)", a24, -1.0f);

    a24 = fp24_from_float(0.0f);
    check_bits24("fp24(0.0) bits", a24, 0x000000);

    a24 = fp24_from_float(3.14159f);
    check_f24("fp24(pi)", a24, 3.14159f);

    a24 = fp24_from_float(1e10f);
    check_f24("fp24(1e10)", a24, 1e10f);

    a24 = fp24_from_float(1e-10f);
    check_f24("fp24(1e-10)", a24, 1e-10f);

    /* ------------------------------------------------------------ */
    /* fp24 Arithmetic Tests                                         */
    /* ------------------------------------------------------------ */
    printf("[fp24] Arithmetic tests\n");

    a24 = fp24_from_float(2.5f);
    b24 = fp24_from_float(3.5f);
    c24 = fp24_add(a24, b24);
    check_f24("2.5 + 3.5", c24, 6.0f);

    c24 = fp24_sub(a24, b24);
    check_f24("2.5 - 3.5", c24, -1.0f);

    c24 = fp24_mul(a24, b24);
    check_f24("2.5 * 3.5", c24, 8.75f);

    c24 = fp24_div(b24, a24);
    check_f24("3.5 / 2.5", c24, 1.4f);

    c24 = fp24_neg(a24);
    check_f24("neg(2.5)", c24, -2.5f);

    c24 = fp24_abs(fp24_from_float(-7.0f));
    check_f24("abs(-7.0)", c24, 7.0f);

    /* ------------------------------------------------------------ */
    /* fp24 Special Values                                           */
    /* ------------------------------------------------------------ */
    printf("[fp24] Special value tests\n");

    check_int("isnan(NaN)", fp24_isnan(FP24_NAN), 1);
    check_int("isnan(1.0)", fp24_isnan(fp24_from_float(1.0f)), 0);
    check_int("isinf(+Inf)", fp24_isinf(FP24_INF), 1);
    check_int("isinf(-Inf)", fp24_isinf(FP24_NEG_INF), 1);
    check_int("isinf(1.0)", fp24_isinf(fp24_from_float(1.0f)), 0);
    check_int("iszero(0.0)", fp24_iszero(FP24_ZERO), 1);
    check_int("iszero(-0.0)", fp24_iszero(FP24_NEG_ZERO), 1);
    check_int("+0 == -0", fp24_eq(FP24_ZERO, FP24_NEG_ZERO), 1);
    check_int("NaN != NaN", fp24_eq(FP24_NAN, FP24_NAN), 0);

    /* ------------------------------------------------------------ */
    /* fp24 Comparison Tests                                         */
    /* ------------------------------------------------------------ */
    printf("[fp24] Comparison tests\n");

    a24 = fp24_from_float(1.0f);
    b24 = fp24_from_float(2.0f);
    check_int("1.0 < 2.0", fp24_lt(a24, b24), 1);
    check_int("2.0 < 1.0", fp24_lt(b24, a24), 0);
    check_int("1.0 <= 1.0", fp24_le(a24, a24), 1);
    check_int("2.0 > 1.0", fp24_gt(b24, a24), 1);

    /* ------------------------------------------------------------ */
    /* fp48 Conversion Tests                                         */
    /* ------------------------------------------------------------ */
    printf("[fp48] Conversion tests\n");

    a48 = fp48_from_double(1.0);
    check_f48("fp48(1.0)", a48, 1.0);

    a48 = fp48_from_double(-1.0);
    check_f48("fp48(-1.0)", a48, -1.0);

    a48 = fp48_from_double(3.141592653589793);
    check_f48("fp48(pi)", a48, 3.141592653589793);

    a48 = fp48_from_double(1e100);
    check_f48("fp48(1e100)", a48, 1e100);

    a48 = fp48_from_double(1e-100);
    check_f48("fp48(1e-100)", a48, 1e-100);

    /* ------------------------------------------------------------ */
    /* fp48 Arithmetic Tests                                         */
    /* ------------------------------------------------------------ */
    printf("[fp48] Arithmetic tests\n");

    a48 = fp48_from_double(2.5);
    b48 = fp48_from_double(3.5);
    c48 = fp48_add(a48, b48);
    check_f48("2.5 + 3.5", c48, 6.0);

    c48 = fp48_sub(a48, b48);
    check_f48("2.5 - 3.5", c48, -1.0);

    c48 = fp48_mul(a48, b48);
    check_f48("2.5 * 3.5", c48, 8.75);

    c48 = fp48_div(b48, a48);
    check_f48("3.5 / 2.5", c48, 1.4);

    c48 = fp48_neg(a48);
    check_f48("neg(2.5)", c48, -2.5);

    c48 = fp48_abs(fp48_from_double(-7.0));
    check_f48("abs(-7.0)", c48, 7.0);

    /* ------------------------------------------------------------ */
    /* fp48 Special Values                                           */
    /* ------------------------------------------------------------ */
    printf("[fp48] Special value tests\n");

    check_int("isnan(NaN)", fp48_isnan(FP48_NAN), 1);
    check_int("isinf(+Inf)", fp48_isinf(FP48_INF), 1);
    check_int("iszero(0)", fp48_iszero(FP48_ZERO), 1);
    check_int("+0 == -0", fp48_eq(FP48_ZERO, FP48_NEG_ZERO), 1);
    check_int("NaN != NaN", fp48_eq(FP48_NAN, FP48_NAN), 0);

    /* ------------------------------------------------------------ */
    /* Cross-format Conversion                                       */
    /* ------------------------------------------------------------ */
    printf("[cross] Format conversion tests\n");

    a24 = fp24_from_float(2.71828f);
    a48 = fp24_to_fp48(a24);
    check_f48("fp24->fp48(e)", a48, (double)fp24_to_float(a24));

    a48 = fp48_from_double(2.718281828459045);
    a24 = fp48_to_fp24(a48);
    check_f24("fp48->fp24(e)", a24, (float)fp48_to_double(a48));

    /* ------------------------------------------------------------ */
    /* Precision boundary: demonstrate fp24 truncation                */
    /* ------------------------------------------------------------ */
    printf("[precision] fp24 truncation demo\n");

    a24 = fp24_from_float(1.0f);
    b24 = fp24_from_float(0.000030517578125f);  /* 2^-15, smallest fp24 mantissa bit */
    c24 = fp24_add(a24, b24);
    /* 1.0 + 2^-15 should be representable */
    check_f24("1.0 + 2^-15", c24, 1.000030517578125f);

    /* ------------------------------------------------------------ */
    /* Print examples                                                */
    /* ------------------------------------------------------------ */
    printf("\n[print] Format display:\n  ");
    fp24_print(fp24_from_float(3.14159f));
    printf("\n  ");
    fp48_print(fp48_from_double(3.141592653589793));
    printf("\n  ");
    fp24_print(FP24_NAN);
    printf("\n  ");
    fp24_print(FP24_INF);
    printf("\n  ");
    fp48_print(FP48_NEG_INF);
    printf("\n");

    /* ------------------------------------------------------------ */
    /* Summary                                                       */
    /* ------------------------------------------------------------ */
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("[OK] ZKAEDI FPX: all tests passed\n");
    } else {
        printf("[FAIL] ZKAEDI FPX: %d test(s) failed\n", g_fail);
    }

    return g_fail ? 1 : 0;
}
