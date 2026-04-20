/*
 * zcc_bitcast.h -- Bit-exact comparator library for ZCC regression tests
 * =========================================================================
 * Provides memcmp-based equality and printing macros for float, double,
 * fp24_t, and fp48_t types.  All comparisons are independent of
 * floating-point arithmetic on the type under test — a helper that calls
 * zcc_f32_bits(x) cannot be wrong in the same way it would if it computed
 * (float)(x + 0.0f).
 *
 * Design constraint: C89/C90 compatible (no _Generic, no stdbool.h,
 * no designated initialisers).  ZCC's own test infrastructure uses C89.
 *
 * Usage pattern:
 *
 *   #include "tools/zcc_bitcast.h"
 *
 *   float a = 1.5f;
 *   float b = compute_something();
 *   ZCC_CHECK_F(a, b, "my test");       // fails with bit-exact diff report
 *   ZCC_CHECK_D(1.5, some_double, "d"); // double version
 *
 * For fp24_t / fp48_t (typedef unsigned int / unsigned long long):
 *   fp24_t u = fp24_from_double(1.5);
 *   ZCC_CHECK_U32(u, reference, "fp24 round-trip");
 *
 * Integration with a test runner:
 *
 *   ZCC_BITCAST_COUNTERS  // declare global pass/fail counters (once per TU)
 *   ...
 *   ZCC_FINAL_REPORT()    // print summary and return exit code
 *
 * Anti-pattern replaced:
 *   OLD: if (fabs(a - b) < 1e-6) { pass++; }          // ERR-0031
 *   NEW: ZCC_CHECK_D(a, b, "description");             // bit-exact
 *
 * Compile guard: ZCC_BITCAST_EPSILON_OK
 *   If you legitimately need epsilon tolerance (e.g. accumulation tests),
 *   define ZCC_BITCAST_EPSILON_OK before including this header to suppress
 *   the compile-time reminder comment.
 */

#ifndef ZCC_BITCAST_H
#define ZCC_BITCAST_H

#include <stdio.h>
#include <string.h>

/* ── Global counters ────────────────────────────────────────────────────── */

/* Declare in exactly one translation unit via ZCC_BITCAST_COUNTERS */
extern int zcc_bc_pass;
extern int zcc_bc_fail;

#define ZCC_BITCAST_COUNTERS  int zcc_bc_pass = 0;  int zcc_bc_fail = 0;

/* ── Bit-extraction helpers ─────────────────────────────────────────────── */

/*
 * zcc_f32_bits(f) -- extract IEEE-754 bit pattern of a float as uint32_t
 * Uses memcpy to avoid strict-aliasing UB and to be ZCC-safe.
 */
static unsigned int zcc_f32_bits(float f) {
    unsigned int u;
    memcpy(&u, &f, 4);
    return u;
}

/*
 * zcc_f64_bits(d) -- extract IEEE-754 bit pattern of a double as uint64_t
 * Uses unsigned long (64-bit on LP64 / x86-64 SysV ABI).
 */
static unsigned long zcc_f64_bits(double d) {
    unsigned long u;
    memcpy(&u, &d, 8);
    return u;
}

/*
 * zcc_f32_from_bits(u) -- reconstruct float from bit pattern
 */
static float zcc_f32_from_bits(unsigned int u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

/*
 * zcc_f64_from_bits(u) -- reconstruct double from bit pattern
 */
static double zcc_f64_from_bits(unsigned long u) {
    double d;
    memcpy(&d, &u, 8);
    return d;
}

/* ── Bit-exact comparison macros ────────────────────────────────────────── */

/*
 * ZCC_CHECK_F(expected_float, got_float, label)
 * Compares two floats bit-exactly.  Correctly distinguishes +0.0 from -0.0
 * and catches 1-ULP codegen errors that epsilon tests miss.
 */
#define ZCC_CHECK_F(expected, got, label)                                     \
    do {                                                                       \
        unsigned int _zcc_e = zcc_f32_bits((float)(expected));                \
        unsigned int _zcc_g = zcc_f32_bits((float)(got));                     \
        if (_zcc_e == _zcc_g) {                                               \
            printf("[PASS] %s\n", (label));                                   \
            zcc_bc_pass++;                                                     \
        } else {                                                               \
            printf("[FAIL] %s  expected=0x%08X (%g)  got=0x%08X (%g)\n",    \
                   (label), _zcc_e, (double)zcc_f32_from_bits(_zcc_e),       \
                   _zcc_g, (double)zcc_f32_from_bits(_zcc_g));               \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)

/*
 * ZCC_CHECK_D(expected_double, got_double, label)
 * Bit-exact double comparison.
 */
#define ZCC_CHECK_D(expected, got, label)                                     \
    do {                                                                       \
        unsigned long _zcc_e = zcc_f64_bits((double)(expected));              \
        unsigned long _zcc_g = zcc_f64_bits((double)(got));                   \
        if (_zcc_e == _zcc_g) {                                               \
            printf("[PASS] %s\n", (label));                                   \
            zcc_bc_pass++;                                                     \
        } else {                                                               \
            printf("[FAIL] %s  expected=0x%016lX (%.17g)  got=0x%016lX (%.17g)\n", \
                   (label), _zcc_e, zcc_f64_from_bits(_zcc_e),               \
                   _zcc_g, zcc_f64_from_bits(_zcc_g));                        \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)

/*
 * ZCC_CHECK_U32(expected_u32, got_u32, label)
 * Bit-exact comparison for fp24_t (typedef unsigned int) or any uint32.
 */
#define ZCC_CHECK_U32(expected, got, label)                                   \
    do {                                                                       \
        unsigned int _zcc_e = (unsigned int)(expected);                       \
        unsigned int _zcc_g = (unsigned int)(got);                            \
        if (_zcc_e == _zcc_g) {                                               \
            printf("[PASS] %s\n", (label));                                   \
            zcc_bc_pass++;                                                     \
        } else {                                                               \
            printf("[FAIL] %s  expected=0x%08X  got=0x%08X\n",               \
                   (label), _zcc_e, _zcc_g);                                  \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)

/*
 * ZCC_CHECK_U64(expected_u64, got_u64, label)
 * Bit-exact comparison for fp48_t (typedef unsigned long long) or any uint64.
 */
#define ZCC_CHECK_U64(expected, got, label)                                   \
    do {                                                                       \
        unsigned long long _zcc_e = (unsigned long long)(expected);           \
        unsigned long long _zcc_g = (unsigned long long)(got);                \
        if (_zcc_e == _zcc_g) {                                               \
            printf("[PASS] %s\n", (label));                                   \
            zcc_bc_pass++;                                                     \
        } else {                                                               \
            printf("[FAIL] %s  expected=0x%016llX  got=0x%016llX\n",         \
                   (label), _zcc_e, _zcc_g);                                  \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)

/*
 * ZCC_CHECK_INT(expected, got, label)
 * Integer check — for struct sizing, offset verification, etc.
 */
#define ZCC_CHECK_INT(expected, got, label)                                   \
    do {                                                                       \
        int _zcc_e = (int)(expected);                                         \
        int _zcc_g = (int)(got);                                              \
        if (_zcc_e == _zcc_g) {                                               \
            printf("[PASS] %s\n", (label));                                   \
            zcc_bc_pass++;                                                     \
        } else {                                                               \
            printf("[FAIL] %s  expected=%d  got=%d\n",                        \
                   (label), _zcc_e, _zcc_g);                                  \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)

/* ── ULP-tolerance helper (legitimate algorithmic use only) ─────────────── */

/*
 * ZCC_ULP_DIFF_F(a, b) -- absolute ULP distance between two floats.
 * Use only when testing accumulation drift or near-associativity,
 * NOT for codegen regression tests.  Define ZCC_BITCAST_EPSILON_OK to
 * suppress the audit flag on files that include this.
 */
#ifdef ZCC_BITCAST_EPSILON_OK
#define ZCC_ULP_DIFF_F(a, b)                                                  \
    (zcc_f32_bits((float)(a)) > zcc_f32_bits((float)(b))                     \
     ? zcc_f32_bits((float)(a)) - zcc_f32_bits((float)(b))                   \
     : zcc_f32_bits((float)(b)) - zcc_f32_bits((float)(a)))

#define ZCC_ULP_DIFF_D(a, b)                                                  \
    (zcc_f64_bits((double)(a)) > zcc_f64_bits((double)(b))                   \
     ? zcc_f64_bits((double)(a)) - zcc_f64_bits((double)(b))                 \
     : zcc_f64_bits((double)(b)) - zcc_f64_bits((double)(a)))

#define ZCC_CHECK_F_ULP(expected, got, max_ulp, label)                        \
    do {                                                                       \
        unsigned long _zcc_ulp = ZCC_ULP_DIFF_F((expected),(got));            \
        if (_zcc_ulp <= (unsigned long)(max_ulp)) {                           \
            printf("[PASS] %s  (ULP drift=%lu)\n", (label), _zcc_ulp);       \
            zcc_bc_pass++;                                                     \
        } else {                                                               \
            printf("[FAIL] %s  ULP drift=%lu > max=%lu  "                     \
                   "expected=0x%08X  got=0x%08X\n",                           \
                   (label), _zcc_ulp, (unsigned long)(max_ulp),              \
                   zcc_f32_bits((float)(expected)),                            \
                   zcc_f32_bits((float)(got)));                               \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)
#endif /* ZCC_BITCAST_EPSILON_OK */

/* ── Round-trip helpers for fp24/fp48 ──────────────────────────────────── */

/*
 * ZCC_FP24_ROUNDTRIP_CHECK(u24, label)
 * For every fp24_t bit pattern u, check that fp24_from_double(fp48_to_double(u))
 * == u OR that both sides are a FTZ-to-zero collapse.
 * Requires zkaedi_fpx.h to be included before this header.
 */
#ifdef ZCC_HAVE_FPX
#define ZCC_FP24_ROUNDTRIP_CHECK(u24, label)                                 \
    do {                                                                       \
        unsigned int _u = (unsigned int)(u24);                                \
        double _d = fp24_to_double(_u);                                       \
        unsigned int _rt = fp24_from_double(_d);                              \
        int _pass = (_rt == _u) ||                                            \
                    ((_rt == 0 || _rt == 0x80000000u) &&                      \
                     (_u  == 0 || _u  == 0x80000000u));                       \
        if (_pass) { zcc_bc_pass++; }                                         \
        else {                                                                 \
            printf("[FAIL] %s  u=0x%08X rt=0x%08X (%.17g)\n",               \
                   (label), _u, _rt, _d);                                     \
            zcc_bc_fail++;                                                     \
        }                                                                      \
    } while (0)
#endif

/* ── Oracle comparison against GCC-compiled reference ──────────────────── */

/*
 * ZCC_ORACLE_PATH defines the path to a GCC-compiled reference binary.
 * When a test is compiled by ZCC, it can exec the GCC oracle binary,
 * capture stdout, and diff against its own output.
 *
 * Convention: reference binary is always named <test_name>.gcc
 *             and lives alongside the test .c file.
 *
 * Workflow enforced by ZCC_TEST_STANDARDS.md §3:
 *   1. Build: gcc -O0 test_foo.c -o test_foo.gcc
 *   2. Build: ./zcc test_foo.c -o test_foo.s && gcc test_foo.s -o test_foo.zcc
 *   3. diff <(./test_foo.gcc) <(./test_foo.zcc)  → must be empty
 */

/* ── Final report ───────────────────────────────────────────────────────── */

/*
 * ZCC_FINAL_REPORT()
 * Print pass/fail summary and return appropriate exit code from main().
 * Usage: return ZCC_FINAL_REPORT();
 */
#define ZCC_FINAL_REPORT()                                                    \
    (printf("\n=== RESULTS: %d passed, %d failed ===\n",                     \
            zcc_bc_pass, zcc_bc_fail),                                        \
     (zcc_bc_fail > 0 ? 1 : 0))

#endif /* ZCC_BITCAST_H */
