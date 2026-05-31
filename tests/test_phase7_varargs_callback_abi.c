#include <stdio.h>
#include <stdarg.h>

typedef struct {
    long x;
    double y;
    int tag;
} Frame;

typedef double (*VarMixer)(Frame f, int n, ...);

double mix(Frame f, int n, ...) {
    va_list ap;
    va_start(ap, n);

    long a = va_arg(ap, long);
    double b = va_arg(ap, double);
    long c = va_arg(ap, long);
    double d = va_arg(ap, double);

    va_end(ap);

    return f.y + b + d + (double)(f.x + a + c + f.tag + n);
}

double drive(VarMixer fn, Frame f) {
    return fn(f, 4, 11L, 2.5, 33L, 7.25);
}

int main(void) {
    Frame f;
    f.x = 1000;
    f.y = 3.75;
    f.tag = 9;

    double out = drive(mix, f);

    printf("out=%ld\n", (long)(out * 1000.0));
    return 0;
}
