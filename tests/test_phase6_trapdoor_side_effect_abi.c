#include <stdio.h>

typedef struct {
    long a;
    long b;
} Pair;

typedef struct {
    Pair p;
    int flag;
    double scale;
} Box;

typedef Box (*Mixer)(Box x, long *state, int gate);

Box mutate(Box x, long *state, int gate) {
    Box r = x;

    if (gate && ((*state += 7) > 0)) {
        r.p.a += *state;
        r.flag ^= gate;
    } else {
        r.p.b -= 13;
    }

    if ((r.flag || ((*state += 11) == 0)) && r.scale > 1.0) {
        r.p.b ^= (long)(r.scale * 1000.0);
    }

    return r;
}

long drive(Mixer fn, Box b) {
    long state = 5;

    Box r = fn(b, &state, 3);

    return (r.p.a * 31) ^ (r.p.b * 17) ^ state ^ r.flag;
}

int main(void) {
    Box b;

    b.p.a = 100;
    b.p.b = 0xabcdefL;
    b.flag = 2;
    b.scale = 2.5;

    printf("out=%ld\n", drive(mutate, b));
    return 0;
}
