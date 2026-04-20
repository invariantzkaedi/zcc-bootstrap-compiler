#include <stdio.h>
const double weights[4] = {1.5, 2.5, 3.5, 4.5};
int main(void) {
    double sum = weights[0] + weights[1];
    double prod = weights[2] * weights[3];
    printf("weights[0]=%f weights[1]=%f\n", weights[0], weights[1]);
    printf("sum=%f prod=%f\n", sum, prod);
    printf("Expected: sum=4.0 prod=15.75\n");
    return 0;
}
