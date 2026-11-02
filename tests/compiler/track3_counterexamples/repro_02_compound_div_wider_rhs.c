#include <stdio.h>
#include <stdint.h>

int main(void) {
    int32_t s32 = 100;
    int64_t d64 = 0x100000002LL;
    s32 /= d64;
    printf("%d\n", s32);
    return 0;
}

