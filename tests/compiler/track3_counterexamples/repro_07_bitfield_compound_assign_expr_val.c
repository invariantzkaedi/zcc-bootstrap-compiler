#include <stdio.h>
#include <stdint.h>

struct BF {
    signed int s5 : 5;
    unsigned int u3 : 3;
};

int main(void) {
    struct BF bf = { -8, 5 };
    bf.s5 += 3;
    bf.s5 /= 2;
    int r = (bf.u3 += 6);
    printf("%d %u %d\n", (int)bf.s5, (unsigned)bf.u3, r);
    return 0;
}

