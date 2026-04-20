#include <stdio.h>
const double arr[4] = {1.5, 2.5, 3.5, 4.5};
int main(void) {
    double x = arr[2];
    printf("arr[2] = %f (expected: 3.5)\n", x);
    return 0;
}
