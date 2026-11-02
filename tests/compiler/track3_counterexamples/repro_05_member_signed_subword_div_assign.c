#include <stdio.h>
#include <stdint.h>

struct S { int8_t a; int16_t b; };

int main(void) {
    struct S s = { -8, -9 };
    s.a /= 2;
    s.b /= 2;
    printf("%d %d\n", (int)s.a, (int)s.b);
    return 0;
}

