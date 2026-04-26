#include <stdio.h>

struct S16 { long a; long b; };

void sink(int a1, int a2, int a3, int a4, int a5, struct S16 s) {
    printf("a: %d %d %d %d %d\n", a1, a2, a3, a4, a5);
    printf("s: 0x%lx 0x%lx\n", s.a, s.b);
}

int main() {
    struct S16 s = { 0x1111111122222222L, 0x3333333344444444L };
    sink(1, 2, 3, 4, 5, s);
    return 0;
}
