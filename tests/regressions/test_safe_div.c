/*
 * test_safe_div.c — Regression test for ZCC --safe-div zero-guard runtime
 */
#include <stdio.h>

int main(void) {
    volatile int a = 42;
    volatile int b = 0;
    int c = a / b; /* Division by dynamic zero */
    /* Under --safe-div, result is folded to 0 without hardware SIGFPE trap */
    if (c != 0) {
        return 1;
    }
    return 0;
}
