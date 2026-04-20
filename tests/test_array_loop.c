#include <stdio.h>
const double arr[4] = {1.5, 2.5, 3.5, 4.5};

int main(void) {
    // Manual unrolled
    double sum1 = 0.0;
    sum1 = sum1 + arr[0];
    sum1 = sum1 + arr[1];
    printf("Manual unrolled sum: %f (expected: 4.0)\n", sum1);
    
    // In a loop
    double sum2 = 0.0;
    for (int i = 0; i < 2; i++) {
        sum2 = sum2 + arr[i];
    }
    printf("Loop sum: %f (expected: 4.0)\n", sum2);
    return 0;
}
