#include <stdio.h>

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, ty) __builtin_va_arg(ap, ty)
#define va_end(ap) __builtin_va_end(ap)

static double sum3(int n, ...) {
    va_list ap;
    double sum = 0.0;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++) sum += va_arg(ap, double);
    va_end(ap);
    return sum;
}

int main(void) {
    float a = 1.25f;
    float b = 2.5f;
    double c = 3.75;
    printf("sum=%.6f\n", sum3(3, a, b, c));
    return 0;
}
