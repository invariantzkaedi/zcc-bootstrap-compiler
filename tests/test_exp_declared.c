#include <stdio.h>

double exp(double x);  // Explicit declaration!

int main(void) {
    double x = 1.0;
    double result = exp(x);
    printf("exp(1.0) = %f (expected: 2.718282)\n", result);
    
    double y = -1.0;
    double result2 = exp(y);
    printf("exp(-1.0) = %f (expected: 0.367879)\n", result2);
    
    return 0;
}
