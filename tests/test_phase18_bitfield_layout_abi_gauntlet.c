// tests/test_phase18_bitfield_layout_abi_gauntlet.c
#include <stdio.h>
#include <stddef.h>
#include <string.h>

typedef union {
    long i;
    double d;
    unsigned char raw[8];
} Cell;

typedef struct {
    unsigned a:3;
    unsigned b:11;
    unsigned c:18;
    double d;
    unsigned e:5;
    unsigned f:27;
    Cell u;
    long tail;
} Weird;

typedef struct {
    Weird w;
    int tag;
    double scale;
} Envelope;

typedef Envelope (*Transformer)(Envelope x, long salt);

static long checksum_bytes(const void *ptr, unsigned long n) {
    const unsigned char *p = (const unsigned char *)ptr;
    long acc = 1469598103934665603L;

    for (unsigned long i = 0; i < n; i++) {
        acc ^= (long)p[i];
        acc *= 1099511628211L;
    }

    return acc;
}

Envelope transform(Envelope x, long salt) {
    Envelope r = x;

    r.w.a = (r.w.a + 1) & 7;
    r.w.b = (r.w.b ^ 0x155) & 0x7ff;
    r.w.c = (r.w.c + 0x12345) & 0x3ffff;

    r.w.e = (r.w.e + 3) & 0x1f;
    r.w.f = (r.w.f ^ 0x1234567) & 0x7ffffff;

    r.w.u.i ^= salt;
    r.w.tail += r.w.a + r.w.b + r.w.c + r.w.e + r.w.f;

    r.tag ^= (int)(salt & 255);
    r.scale += (double)(r.tag & 15) * 0.25;

    return r;
}

long consume(Transformer fn, Envelope x) {
    Envelope y = fn(x, 0x55aa33ccL);

    long acc = 0;
    acc += (long)sizeof(Weird) * 3;
    acc += (long)sizeof(Envelope) * 5;

    acc += (long)offsetof(Weird, d) * 7;
    acc += (long)offsetof(Weird, u) * 11;
    acc += (long)offsetof(Weird, tail) * 13;
    acc += (long)offsetof(Envelope, tag) * 17;
    acc += (long)offsetof(Envelope, scale) * 19;

    acc ^= y.w.a;
    acc += y.w.b;
    acc ^= y.w.c;
    acc += (long)(y.w.d * 1000.0);
    acc ^= y.w.e;
    acc += y.w.f;
    acc ^= y.w.u.i;
    acc += y.w.tail;
    acc ^= y.tag;
    acc += (long)(y.scale * 1000.0);

    acc ^= checksum_bytes(&y, sizeof(y));

    return acc;
}

int main(void) {
    Envelope x;
    memset(&x, 0, sizeof(x));

    x.w.a = 5;
    x.w.b = 0x321;
    x.w.c = 0x2aaaa;
    x.w.d = 6.75;

    x.w.e = 17;
    x.w.f = 0x654321;
    x.w.u.i = 0x1122334455667788L;
    x.w.tail = 99999;

    x.tag = 42;
    x.scale = 3.5;

    printf("sizeof_Weird=%ld\n", (long)sizeof(Weird));
    printf("sizeof_Envelope=%ld\n", (long)sizeof(Envelope));

    printf("off_d=%ld\n", (long)offsetof(Weird, d));
    printf("off_u=%ld\n", (long)offsetof(Weird, u));
    printf("off_tail=%ld\n", (long)offsetof(Weird, tail));
    printf("off_tag=%ld\n", (long)offsetof(Envelope, tag));
    printf("off_scale=%ld\n", (long)offsetof(Envelope, scale));

    printf("checksum=%ld\n", checksum_bytes(&x, sizeof(x)));
    printf("out=%ld\n", consume(transform, x));

    return 0;
}
