#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint8_t u8 = 250;
    uint16_t u16 = 60000;
    printf("%d %d\n", (-u8 < 0), (-u16 < 0));
    return 0;
}

