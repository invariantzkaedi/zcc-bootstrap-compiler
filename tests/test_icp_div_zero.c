#include <stdio.h>

static int divide(int num, int den) {
    return num / den;
}

int main() {
    int x = divide(42, 0);
    printf("res=%d\n", x);
    return 0;
}
