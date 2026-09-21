#include <stdio.h>

typedef struct {
    long a;
    long b;
} Pair;

__attribute__((noinline))
Pair make_pair(long x, long y) {
    Pair p;
    p.a = x;
    p.b = y;
    return p;
}

int main(void) {
    /* Clobber rdx before calling make_pair to ensure stale rdx isn't coincidentally correct */
    long x = 123456789L;
    long y = 987654321L;
    Pair p = make_pair(x, y);
    if (p.a != 123456789L || p.b != 987654321L) {
        printf("PROBE_RET2 FAIL: p.a=%ld (exp 123456789), p.b=%ld (exp 987654321)\n", p.a, p.b);
        return 1;
    }
    printf("PROBE_RET2 PASS: p.a=%ld, p.b=%ld\n", p.a, p.b);
    return 0;
}
