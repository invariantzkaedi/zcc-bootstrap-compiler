#include <stdio.h>
#include <stdarg.h>

static int g_pass = 0;
static int g_fail = 0;

static double first_va(int n, ...) {
    va_list ap;
    double r;
    va_start(ap, n);
    r = va_arg(ap, double);
    va_end(ap);
    (void)n;
    return r;
}

int main(void) {
    double r;
    double ref;
    r = first_va(1, -1.5f);
    ref = -1.5;
    if (r == ref) { printf("PASS -1.5f\n"); g_pass++; }
    else { printf("FAIL -1.5f\n"); g_fail++; }
    printf("DONE: %d pass\n", g_pass);
    return g_fail;
}
