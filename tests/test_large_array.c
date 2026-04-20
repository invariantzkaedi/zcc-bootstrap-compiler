#include <stdio.h>

// Small version: 8 weights instead of 1024
const double w1[8] = {0.08780748, -0.02444191, 0.11449624, 0.26923618, 
                      -0.04155278, -0.04138996, 0.27916801, 0.13539492};
const double b1[2] = {-0.02656925, -0.06242898};
const double w2[2] = {0.41999254, 0.05005646};
const double b2[1] = {0.00302616};

double sigmoid(double x) {
    return 1.0 / (1.0 + (-x));  // simplified
}

int main(void) {
    double sum = 0.0;
    for (int i = 0; i < 8; i++) {
        sum = sum + w1[i];
    }
    printf("sum of w1 = %f (expected: 0.71615876)\n", sum);
    
    // Test actual computation
    double h = w1[0] * 2.0 + b1[0];
    double out = sigmoid(h);
    printf("h=%f out=%f\n", h, out);
    return 0;
}
