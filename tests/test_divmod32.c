#include <stdio.h>

int main() {
    int a = 17;
    int b = 4;
    int q = a / b;
    int r = a % b;

    printf("q=%d r=%d\n", q, r);
    return (q == 4 && r == 1) ? 0 : 1;
}
