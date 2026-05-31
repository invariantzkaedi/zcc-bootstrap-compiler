// tests/test_phase3_nested_aggregate_return.c
#include <stdio.h>

typedef struct {
    long x;
    double y;
} Pair;

typedef struct {
    Pair a;
    Pair b;
    int tag;
} Big;

Big make_big(long seed, double scale, int tag) {
    Big r;

    r.a.x = seed + 11;
    r.a.y = scale * 2.0;

    r.b.x = seed ^ 0x5a5a5a5aL;
    r.b.y = scale + 7.25;

    r.tag = tag + 3;

    return r;
}

long consume(Big v, int n) {
    long acc = v.a.x + v.b.x + v.tag;

    for (int i = 0; i < n; i++) {
        acc += (long)(v.a.y * 100.0);
        acc ^= (long)(v.b.y * 1000.0);
        acc = (acc << 1) ^ (acc >> 3) ^ i;
    }

    return acc;
}

int main(void) {
    Big v = make_big(1234567L, 3.5, 9);
    long r = consume(v, 17);

    printf("a.x=%ld\n", v.a.x);
    printf("a.y=%ld\n", (long)(v.a.y * 1000.0));
    printf("b.x=%ld\n", v.b.x);
    printf("b.y=%ld\n", (long)(v.b.y * 1000.0));
    printf("tag=%d\n", v.tag);
    printf("result=%ld\n", r);

    return 0;
}
