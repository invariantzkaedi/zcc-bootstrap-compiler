#include <stdio.h>
#include <stdarg.h>

typedef struct { long a; double b; int tag; } Frame;
typedef double (*Fn)(Frame f, int n, ...);

double mix(Frame f, int n, ...) {
    va_list ap;
    va_start(ap, n);
    long x = va_arg(ap, long);
    double y = va_arg(ap, double);
    long z = va_arg(ap, long);
    double q = va_arg(ap, double);
    va_end(ap);
    return f.b + y + q + (double)(f.a + x + z + f.tag + n);
}

double drive(Fn fn, Frame f) {
    return fn(f, 4, 11L, 2.5, 33L, 7.25);
}

int main(void) {
    Frame f = {1000, 3.75, 9};
    printf("out=%ld\n", (long)(drive(mix, f) * 1000.0));
    return 0;
}
