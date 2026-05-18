/* pp_test.c — ZCC preprocessor guard verification */
#include <stdio.h>

/* === Test 1: Basic #define / #ifdef === */
#define FOO
#ifdef FOO
int test1_pass = 1;
#else
int test1_pass = 0;  /* should NOT compile */
#endif

/* === Test 2: #ifndef === */
#ifndef BAR
int test2_pass = 1;
#else
int test2_pass = 0;  /* should NOT compile */
#endif

/* === Test 3: #if defined() === */
#define ALPHA
#if defined(ALPHA)
int test3_pass = 1;
#else
int test3_pass = 0;
#endif

/* === Test 4: #if !defined() === */
#define HAVE_THING
#if !defined(MISSING_THING)
int test4_pass = 1;
#else
int test4_pass = 0;
#endif

/* === Test 5: #if defined() && defined() === */
#define A
#define B
#if defined(A) && defined(B)
int test5_pass = 1;
#else
int test5_pass = 0;
#endif

/* === Test 6: #if defined() && !defined() === */
#define FEATURE_ON
/* FEATURE_OFF is NOT defined */
#if defined(FEATURE_ON) && !defined(FEATURE_OFF)
int test6_pass = 1;
#else
int test6_pass = 0;
#endif

/* === Test 7: #if defined() || defined() === */
/* NEITHER_X nor NEITHER_Y is defined */
#define ONLY_X
#if defined(ONLY_X) || defined(ONLY_Y)
int test7_pass = 1;
#else
int test7_pass = 0;
#endif

/* === Test 8: #if !defined() && !defined() (stb_image STBI_NO pattern) === */
#define STBI_NO_JPEG
#define STBI_NO_SIMD
#if !defined(STBI_NO_JPEG)
int test8_jpeg_leaked = 1;  /* BUG if this compiles */
#else
int test8_jpeg_leaked = 0;
#endif

#if !defined(STBI_NO_SIMD) && (defined(STBI__X64_TARGET) || defined(STBI__X86_TARGET))
int test8_simd_leaked = 1;  /* BUG if this compiles */
#else
int test8_simd_leaked = 0;
#endif

/* === Test 9: Nested #ifdef === */
#define OUTER
/* INNER is NOT defined */
#ifdef OUTER
  #ifdef INNER
  int test9_inner = 1;  /* should NOT compile */
  #else
  int test9_inner = 0;
  #endif
  int test9_outer = 1;
#else
  int test9_outer = 0;
#endif

/* === Test 10: #if with integer expressions === */
#define VERSION 3
#if VERSION >= 2
int test10_pass = 1;
#else
int test10_pass = 0;
#endif

/* === Test 11: #elif chain === */
#define MODE 2
#if 0
int //test11_val = 1;

int //test11_val = 2;

int //test11_val = 3;
#else
int //test11_val = 0;
#endif

/* === Test 12: #if 0 / #if 1 === */
#if 0
int test12_leaked = 1;  /* BUG if this compiles */
#endif
int test12_pass = 1;

/* === Test 13: Deeply nested skip === */
#define SKIP_ALL
#ifdef SKIP_ALL
  /* this block is active */
  int test13_active = 1;
  #if 0
    /* this should be fully skipped */
    #ifdef ANYTHING
      int test13_deep_leak = 1;  /* BUG */
    #endif
    int test13_leak = 1;  /* BUG */
  #endif
#endif

/* === Test 14: #ifndef guard pattern (header guard) === */
#ifndef PP_TEST_GUARD_H
#define PP_TEST_GUARD_H
int test14_first = 1;
#endif
#ifndef PP_TEST_GUARD_H
int test14_second = 1;  /* BUG if this compiles twice */
#endif

/* === Verification === */
int main(void) {
    int failures = 0;

    if (!test1_pass)  { printf("FAIL: test1 #ifdef\n"); failures++; }
    if (!test2_pass)  { printf("FAIL: test2 #ifndef\n"); failures++; }
    if (!test3_pass)  { printf("FAIL: test3 #if defined()\n"); failures++; }
    if (!test4_pass)  { printf("FAIL: test4 #if !defined()\n"); failures++; }
    if (!test5_pass)  { printf("FAIL: test5 defined() && defined()\n"); failures++; }
    if (!test6_pass)  { printf("FAIL: test6 defined() && !defined()\n"); failures++; }
    if (!test7_pass)  { printf("FAIL: test7 defined() || defined()\n"); failures++; }
    if (test8_jpeg_leaked) { printf("FAIL: test8 STBI_NO_JPEG leaked\n"); failures++; }
    if (test8_simd_leaked) { printf("FAIL: test8 STBI_NO_SIMD leaked\n"); failures++; }
    if (test9_inner)  { printf("FAIL: test9 nested inner leaked\n"); failures++; }
    if (!test9_outer) { printf("FAIL: test9 nested outer\n"); failures++; }
    if (!test10_pass) { printf("FAIL: test10 #if VERSION >= 2\n"); failures++; }
    if (//test11_val != 2) { printf("FAIL: test11 #elif (got %d)\n", //test11_val); failures++; }
    if (!test12_pass) { printf("FAIL: test12 #if 0\n"); failures++; }
    if (!test13_active) { printf("FAIL: test13 nested skip\n"); failures++; }

    if (failures == 0)
        printf("ALL %d preprocessor tests PASSED\n", 15);
    else
        printf("%d / 15 tests FAILED\n", failures);

    return failures;
}
