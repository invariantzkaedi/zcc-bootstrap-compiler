/*
 * runtime_probe_test.c — D-28 gate validation binary
 *
 * A minimal C program that exercises a known, predictable call-graph
 * so the verify-runtime-probe gate can validate:
 *   1. The probe records all functions
 *   2. Peak depth is correct (>= 5)
 *   3. Total call count is nonzero
 *   4. zcc_behavioral_diff produces a valid drift report
 *
 * Link with: gcc -finstrument-functions zcc_runtime_probe.c runtime_probe_test.c
 *
 * Function names are prefixed fn_* to avoid collisions with libc/math.h
 * symbols (gamma, beta, alpha, epsilon, delta are reserved in math.h).
 *
 * Expected runtime genome values (deterministic):
 *   observed_functions: >= 6   (main + fn_a..fn_e + fn_hot)
 *   peak_call_depth:    >= 5   (main->fn_a->fn_b->fn_c->fn_d->fn_e)
 *   total_calls:        > 0
 */
#include <stdio.h>

/* Forward declarations — fn_* prefix avoids all libc/math.h conflicts */
static int fn_e(int x);
static int fn_d(int x);
static int fn_c(int x);
static int fn_b(int x);
static int fn_a(int x);
static int fn_hot(int n);

/* ── Call chain: depth 5 ─────────────────────────────────────────────── */

static int fn_e(int x) {
    return x + 1;
}

static int fn_d(int x) {
    return fn_e(x) + fn_e(x + 1); /* 2 calls to fn_e */
}

static int fn_c(int x) {
    return fn_d(x) + 1;
}

static int fn_b(int x) {
    return fn_c(x) * 2;
}

static int fn_a(int x) {
    return fn_b(x) + fn_b(x + 1); /* 2 calls to fn_b; depth = main(1)+fn_a(2)+fn_b(3)+fn_c(4)+fn_d(5)+fn_e(6) */
}

/* ── Leaf exercise: called many times to create hot-function signal ───── */

static int fn_hot(int n) {
    int acc = 0;
    for (int i = 0; i < n; i++) acc += i;
    return acc;
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(void) {
    int result = 0;

    /* Chain traversal — exercises depth-6 path (main is depth 1) */
    result += fn_a(1);
    result += fn_a(2);

    /* Hot leaf — called 50 times to dominate top-N call list */
    for (int i = 0; i < 50; i++)
        result += fn_hot(i);

    /* Prevent optimizer from eliminating computation */
    if (result < 0) {
        fprintf(stderr, "unexpected negative result\n");
        return 1;
    }

    fprintf(stderr, "[probe_test] Computation complete. result=%d\n", result);
    fprintf(stderr, "[probe_test] Expected: 7 distinct functions, peak depth >= 6\n");

    return 0;
}

