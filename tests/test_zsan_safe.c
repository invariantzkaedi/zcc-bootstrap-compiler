#include <stdio.h>

int safe_function(int a, int b) {
    char buf[16];
    for (int i = 0; i < 15; i++) {
        buf[i] = 'x';
    }
    buf[15] = 0;
    return a + b + (int)buf[0];
}

int main(void) {
    int res = safe_function(40, 2);
    if (res == 42 + 'x') {
        printf("PASS: ZSAN_SAFE_EXECUTION\n");
        return 0;
    }
    printf("FAIL: unexpected res=%d\n", res);
    return 1;
}
