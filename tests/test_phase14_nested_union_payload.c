#include <stdio.h>

typedef union {
    long i;
    double d;
} U;

typedef struct {
    int tag;
    U a;
    U b;
} Packet;

long consume(Packet p) {
    if (p.tag)
        return p.a.i + (long)(p.b.d * 1000.0);
    return p.b.i;
}

int main(void) {
    Packet p;
    p.tag = 1;
    p.a.i = 12345;
    p.b.d = 6.75;
    printf("out=%ld\n", consume(p));
    return 0;
}
