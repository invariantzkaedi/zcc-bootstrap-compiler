#include <stdio.h>

typedef struct {
    long x;
    long y;
} Vec2;

typedef struct {
    Vec2 v;
    double scale;
    int depth;
} Frame;

typedef Vec2 (*Transform)(Frame f, long k);

Vec2 twist(Frame f, long k);

Vec2 bounce(Frame f, long k) {
    Vec2 r;

    r.x = f.v.x + k + f.depth;
    r.y = f.v.y ^ ((long)(f.scale * 1000.0));

    if (f.depth > 0) {
        Frame next = f;
        next.depth--;

        Vec2 t = twist(next, k + 3);

        r.x ^= t.x;
        r.y += t.y;
    }

    return r;
}

Vec2 twist(Frame f, long k) {
    Vec2 r;

    r.x = (f.v.x * 3) + k;
    r.y = (f.v.y * 7) ^ k;

    if (f.depth > 0) {
        Frame next = f;
        next.depth--;

        Vec2 b = bounce(next, k + 5);

        r.x += b.x;
        r.y ^= b.y;
    }

    return r;
}

long drive(Transform fn, Frame f) {
    Vec2 r = fn(f, 11);
    return (r.x * 13) ^ (r.y * 17);
}

int main(void) {
    Frame f;

    f.v.x = 12345;
    f.v.y = 0x12345678L;
    f.scale = 4.75;
    f.depth = 4;

    long out = drive(bounce, f);

    printf("out=%ld\n", out);

    return 0;
}
