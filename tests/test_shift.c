#include <stdio.h>
int main() {
    unsigned long long v = 0x40091EB851EB851FULL;
    printf("%llx\n", v);
    v >>= 8; printf("%llx\n", v);
    v >>= 8; printf("%llx\n", v);
    return 0;
}