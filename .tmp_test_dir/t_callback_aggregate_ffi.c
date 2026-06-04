#include <stdio.h>

typedef struct {
    long a;
    long b;
} Pair64;

typedef struct {
    Pair64 p;
    double scale;
    int tag;
} Payload;

typedef Pair64 (*Combiner)(Payload v, long x, long y, long z, long w, long q);

Pair64 combine(Payload v, long x, long y, long z, long w, long q) {
    Pair64 r;
    r.a = v.p.a + x + z + q + v.tag;
    r.b = v.p.b ^ y ^ w ^ (long)(v.scale * 1000.0);
    return r;
}

long invoke(Combiner fn, Payload v) {
    Pair64 r = fn(v, 11, 22, 33, 44, 55);
    return (r.a * 3) ^ (r.b * 7);
}

int main(void) {
    Payload v;
    v.p.a = 1234;
    v.p.b = 0x55aa55aaL;
    v.scale = 6.25;
    v.tag = 9;

    long out = invoke(combine, v);

    printf("out=%ld\n", out);
    return 0;
}
