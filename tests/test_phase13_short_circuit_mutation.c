#include <stdio.h>

long f(long *p, int a, int b) {
    long x = 0;

    if ((a && ((*p += 3) > 0)) || (b && ((*p += 7) > 0)))
        x += *p;

    if ((!a || ((*p += 11) > 0)) && ((*p += 13) > 0))
        x ^= *p;

    return x + *p;
}

int main(void) {
    long s = 5;
    printf("out=%ld\n", f(&s, 1, 0));
    return 0;
}
