#include <stdio.h>
#include <stdint.h>

int main(void) {
    int32_t x = 10;
    double scale = 2.5;
    x *= scale;
    printf("%d\n", x);
    return 0;
}

