#include <stdio.h>
#include <stdint.h>

int main(void) {
    int32_t s32 = -10;
    uint32_t u32 = 2u;
    s32 /= u32;
    printf("%d\n", s32);
    return 0;
}

