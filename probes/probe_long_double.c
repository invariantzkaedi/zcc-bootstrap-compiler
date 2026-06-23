#include <stdio.h>

int main(void) {
    long double a = 1.25L;
    long double b = 2.75L;
    long double c = a * b + 0.5L;
    printf("sizeof(long double)=%d\n", (int)sizeof(long double));
    printf("value=%.6f\n", (double)c);
    return 0;
}
