#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

typedef struct {
    long x;
    long y;
} Point16;

Point16 make_point(long x, long y) {
    Point16 p;
    p.x = x;
    p.y = y;
    return p;
}

int test_va_arg_struct(int count, ...) {
    va_list ap;
    va_start(ap, count);

    Point16 p1 = va_arg(ap, Point16);
    int extra = va_arg(ap, int);

    va_end(ap);

    printf("Extracted p1: x=%ld, y=%ld | extra=%d\n", p1.x, p1.y, extra);
    if (p1.x != 100 || p1.y != 200 || extra != 999) {
        printf("BUG CONFIRMED: va_arg(ap, Point16) dropped field y or desynced stream!\n");
        return 1;
    }
    printf("SUCCESS: va_arg struct16 works!\n");
    return 0;
}

int main(void) {
    Point16 p = make_point(100, 200);
    int r = test_va_arg_struct(2, p, 999);
    return r;
}
