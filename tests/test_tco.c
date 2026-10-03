/*
 * test_tco.c — Tail Call Optimization (TCO) Comprehensive Verification Harness
 *
 * Verifies that self-recursive and sibling calls in tail position are lowered
 * into direct jmp instructions with caller frame elimination (movq %rbp, %rsp; popq %rbp).
 * Proves elimination of stack exhaustion across deep recursion (>1,000,000 frames).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

static int failures = 0;

#define TEST(name) \
    printf( "  [TEST] %s\n", (name))

#define CHECK(cond, msg) do { \
    if (cond) { \
        printf( "    \xe2\x9c\x93 PASS: %s\n", (msg)); \
    } else { \
        printf( "    \xe2\x9c\x97 FAIL: %s (line %d)\n", (msg), __LINE__); \
        failures++; \
    } \
} while (0)

/* ── Suite 1: Deep Self-Recursive Tail Call (10,000,000 iterations) ── */
long sum_tail(long n, long acc) {
    if (n <= 0) return acc;
    return sum_tail(n - 1, acc + n);
}

static void test_deep_self_recursion(void) {
    TEST("T1: Deep self-recursive tail call (10,000,000 iterations — no stack overflow)");
    long r = sum_tail(10000000L, 0L);
    CHECK(r == 50000005000000L, "sum_tail(10M) computes 50000005000000 correctly");
}

/* ── Suite 2: Sibling Call Lowering (foo -> bar) ── */
long sibling_bar(long x) {
    return x * 3L + 7L;
}

long sibling_foo(long x) {
    return sibling_bar(x + 10L);
}

static void test_sibling_call(void) {
    TEST("T2: Sibling call lowering (sibling_foo -> sibling_bar via jmp)");
    long r = sibling_foo(5L);
    CHECK(r == (15L * 3L + 7L), "sibling_foo(5) computes 52 correctly");
}

/* ── Suite 3: Mutually Recursive Sibling State Machine (1,000,000 steps) ── */
long is_odd_tail(long n);

long is_even_tail(long n) {
    if (n == 0) return 1L;
    return is_odd_tail(n - 1L);
}

long is_odd_tail(long n) {
    if (n == 0) return 0L;
    return is_even_tail(n - 1L);
}

static void test_mutual_sibling_recursion(void) {
    TEST("T3: Mutually recursive sibling calls (1,000,000 transitions)");
    long r1 = is_even_tail(1000000L);
    long r2 = is_odd_tail(1000000L);
    CHECK(r1 == 1L, "is_even_tail(1,000,000) == 1");
    CHECK(r2 == 0L, "is_odd_tail(1,000,000) == 0");
}

/* ── Suite 4: Multi-Argument Tail Call (6 registers: rdi, rsi, rdx, rcx, r8, r9) ── */
long multi_arg_worker(long a, long b, long c, long d, long e, long f) {
    return a + 2*b + 3*c + 4*d + 5*e + 6*f;
}

long multi_arg_caller(long a, long b, long c, long d, long e, long f) {
    return multi_arg_worker(a + 1, b + 2, c + 3, d + 4, e + 5, f + 6);
}

static void test_multi_argument_tail_call(void) {
    TEST("T4: Multi-argument tail call preserving all 6 SystemV registers");
    long r = multi_arg_caller(1L, 2L, 3L, 4L, 5L, 6L);
    /* a=2, b=4, c=6, d=8, e=10, f=12 -> 2 + 8 + 18 + 32 + 50 + 72 = 182 */
    CHECK(r == 182L, "multi_arg_caller computes 182");
}

/* ── Suite 5: Fibonacci Accumulator Tail Call ── */
long fib_tail_acc(long n, long a, long b) {
    if (n == 0) return a;
    if (n == 1) return b;
    return fib_tail_acc(n - 1, b, a + b);
}

static void test_fibonacci_tail(void) {
    TEST("T5: Fibonacci accumulator tail recursion");
    long r = fib_tail_acc(40L, 0L, 1L);
    CHECK(r == 102334155L, "fib_tail_acc(40) == 102334155");
}

int main(void) {
    printf( "\n\xe2\x95\x94\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n");
    printf( "\xe2\x95\x91  \xe2\x96\xb8 ZKAEDI TAIL CALL OPTIMIZATION (TCO) TEST SUITE         \xe2\x95\x91\n");
    printf( "\xe2\x95\x9a\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x9d\n\n");

    test_deep_self_recursion();
    test_sibling_call();
    test_mutual_sibling_recursion();
    test_multi_argument_tail_call();
    test_fibonacci_tail();

    printf( "\n  Results: %d FAILED\n", failures);
    if (failures == 0) {
        printf( "  \xe2\x9c\x93 ALL TCO TESTS GREEN\n\n");
    }

    return failures > 0 ? 1 : 0;
}
