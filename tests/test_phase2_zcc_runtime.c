// tests/test_phase2_zcc_runtime.c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    int tag;
    union {
        double d;
        long i;
    } payload;
} Packet;

Packet make_double(int tag, double x) {
    Packet p;
    p.tag = tag;
    p.payload.d = x;
    return p;
}

Packet make_int(int tag, long x) {
    Packet p;
    p.tag = tag;
    p.payload.i = x;
    return p;
}

long fold(Packet a, Packet b, int n) {
    long acc = 0;

    for (int i = 0; i < n; i++) {
        acc += a.tag * (i + 1);
        acc ^= b.payload.i;
        acc += (long)(a.payload.d * 1000.0);
        acc = (acc << 3) ^ (acc >> 2);
    }

    return acc;
}

int main(void) {
    Packet a = make_double(7, 3.14159);
    Packet b = make_int(2, 1234567890L);

    long r = fold(a, b, 32);

    printf("tag_a=%d\n", a.tag);
    printf("double_scaled=%ld\n", (long)(a.payload.d * 100000.0));
    printf("tag_b=%d\n", b.tag);
    printf("int_b=%ld\n", b.payload.i);
    printf("fold=%ld\n", r);

    return 0;
}
