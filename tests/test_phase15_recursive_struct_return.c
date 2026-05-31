#include <stdio.h>

typedef struct { long x, y; } Pair;

Pair rec(long n, long acc) {
    Pair p;
    if (n == 0) {
        p.x = acc;
        p.y = acc ^ 0x55aa55aaL;
        return p;
    }

    Pair q = rec(n - 1, acc + n);
    p.x = q.x + n;
    p.y = q.y ^ n;
    return p;
}

int main(void) {
    Pair r = rec(12, 3);
    printf("out=%ld\n", r.x ^ r.y);
    return 0;
}
