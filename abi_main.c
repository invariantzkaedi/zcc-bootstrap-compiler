#include <stdio.h>
typedef union { void *p; long i; double n; } Value;
typedef struct { Value v; unsigned char tt; } TValue;
TValue make_num(double d);
TValue make_int(long i);
int main() {
    TValue a = make_num(3.14159);
    TValue b = make_int(1234567890L);
    printf("tag a: %d, num a: %f\n", a.tt, a.v.n);
    printf("tag b: %d, int b: %ld\n", b.tt, b.v.i);
    return (a.tt == 3 && b.tt == 2) ? 0 : 1;
}
