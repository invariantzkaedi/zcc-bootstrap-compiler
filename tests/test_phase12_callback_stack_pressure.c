#include <stdio.h>

typedef struct { long a, b; double d; } Big;
typedef long (*Fn)(Big, long,long,long,long,long,long);

long target(Big x, long a,long b,long c,long d,long e,long f) {
    return x.a + x.b + (long)(x.d * 1000.0) + a+b+c+d+e+f;
}

long drive(Fn fn, Big x) {
    return fn(x, 1,2,3,4,5,6);
}

int main(void) {
    Big x = {100, 200, 3.5};
    printf("out=%ld\n", drive(target, x));
    return 0;
}
