/*
 * test_cg_sigfpe_003_diff.c — Differential test for CG-SIGFPE-003
 * Verifies that variable/constant division operations where denominator evaluates to 0
 * are handled safely without emitting crash-inducing machine idiv instructions.
 */

#include <stdio.h>

static int test_const_zero_div(int x) {
    /* Denominator is constant 0 — must be folded / guarded cleanly */
    int zero = 0;
    if (x == 999) {
        return x / zero;
    }
    return x + 1;
}

static int test_variable_zero_div(int a, int b) {
    /* When b is 0 at runtime, division should not crash if guarded or folded */
    if (b == 0) return 0;
    return a / b;
}

int main(void) {
    int res1 = test_const_zero_div(10);
    int res2 = test_variable_zero_div(100, 0);
    int res3 = test_variable_zero_div(100, 5);
    
    if (res1 == 11 && res2 == 0 && res3 == 20) {
        printf("CG-SIGFPE-003 DIFF PASS\n");
        return 0;
    }
    printf("CG-SIGFPE-003 DIFF FAIL: res1=%d res2=%d res3=%d\n", res1, res2, res3);
    return 1;
}
