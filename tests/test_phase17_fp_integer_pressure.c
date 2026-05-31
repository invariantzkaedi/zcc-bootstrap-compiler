#include <stdio.h>

typedef struct {
    double a;
    long b;
    double c;
    long d;
} Mix;

Mix make(double x, long y) {
    Mix m;
    m.a = x + 1.25;
    m.b = y + 11;
    m.c = x * 3.5;
    m.d = y ^ 0xabcdefL;
    return m;
}

long consume(Mix m) {
    return (long)(m.a * 1000.0) + m.b + (long)(m.c * 1000.0) + m.d;
}

int main(void) {
    printf("out=%ld\n", consume(make(2.5, 12345)));
    return 0;
}
