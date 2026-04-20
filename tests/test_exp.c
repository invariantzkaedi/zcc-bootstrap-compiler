#include <stdio.h>
#include <math.h>

int main(void) {
    double x = 1.0;
    double result = exp(x);
    printf("exp(1.0) = %f (expected: 2.718282)\n", result);
    
    double y = -1.0;
    double result2 = exp(y);
    printf("exp(-1.0) = %f (expected: 0.367879)\n", result2);
    
    // Test sigmoid
    double sig = 1.0 / (1.0 + exp(-2.0));
    printf("sigmoid(2.0) = %f (expected: 0.880797)\n", sig);
    
    return 0;
}
