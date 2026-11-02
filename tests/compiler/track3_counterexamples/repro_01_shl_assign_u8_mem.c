#include <stdio.h>
#include <stdint.h>

struct S { uint8_t b; };

int main(void) {
    struct S s = { 0x11u };
    s.b <<= 2;
    printf("%u\n", (unsigned)s.b);
    return 0;
}

