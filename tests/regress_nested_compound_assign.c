#include <stdio.h>
#include <stdint.h>

/* Regression test for nested compound assignment stack-isolation */
static uint16_t g_48 = 3;

int main(void) {
    int32_t l_47 = -1;
    g_48 &= ((( (l_47 &= 0) , 0x2A ), l_47));
    if (g_48 != 0 || l_47 != 0) {
        printf("FAIL: g_48=%u, l_47=%d (expected 0, 0)\n", g_48, l_47);
        return 1;
    }
    printf("PASS: nested compound assignment evaluated cleanly with zero clobbering\n");
    return 0;
}
