#include <stdio.h>

int probe_calc(int x, int y) {
    int a = 0;
    a = x * 8;
    int b = y * 16;
    int c = a + 0;
    int d = b * 1;
    int e = x * 32;
    int f = y * 4;
    return a + b + c + d + e + f;
}

int main(void) {
    int res = probe_calc(3, 5);
    // 3*8 + 5*16 + 24 + 80 + 3*32 + 5*4
    // 24 + 80 + 24 + 80 + 96 + 20 = 324
    printf("PEEPHOLE_PROBE_RESULT: %d\n", res);
    if (res == 324) {
        return 0;
    }
    return 1;
}
