#include <stdio.h>
int main(void) {
    double a = 2.0;
    double b = 4.0;
    
    double div = a / b;
    printf("2.0 / 4.0 = %f (expected: 0.5)\n", div);
    
    double neg = -a;
    printf("-2.0 = %f (expected: -2.0)\n", neg);
    
    double expr = 1.0 / (1.0 + (-a));
    printf("1.0 / (1.0 + (-2.0)) = %f (expected: -1.0)\n", expr);
    
    return 0;
}
