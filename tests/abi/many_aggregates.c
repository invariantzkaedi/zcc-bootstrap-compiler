#include <stdio.h>

struct S16 { long a, b; };

void sink(struct S16 s1, struct S16 s2, struct S16 s3, struct S16 s4) {
    printf("s1: 0x%lx 0x%lx\n", s1.a, s1.b);
    printf("s2: 0x%lx 0x%lx\n", s2.a, s2.b);
    printf("s3: 0x%lx 0x%lx\n", s3.a, s3.b);
    printf("s4: 0x%lx 0x%lx\n", s4.a, s4.b);
}

int main() {
    struct S16 s1 = { 0x1a, 0x1b };
    struct S16 s2 = { 0x2a, 0x2b };
    struct S16 s3 = { 0x3a, 0x3b };
    struct S16 s4 = { 0x4a, 0x4b };
    sink(s1, s2, s3, s4);
    return 0;
}
