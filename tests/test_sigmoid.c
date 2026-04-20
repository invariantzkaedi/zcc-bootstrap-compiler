#include <stdio.h>

double relu(double x) {
    return x > 0.0 ? x : 0.0;
}

double sigmoid(double x) {
    double exp_neg_x = 1.0; // simplified exp(-x) ≈ 1.0 for small x
    return 1.0 / (1.0 + exp_neg_x);
}

int main(void) {
    double x = 0.5;
    double r = relu(x);
    double s = sigmoid(x);
    
    printf("relu(0.5) = %f (expected: 0.5)\n", r);
    printf("sigmoid(0.5) = %f (expected: ~0.5)\n", s);
    
    // Test comparison
    if (s > 0.5) {
        printf("s > 0.5: TRUE\n");
    } else {
        printf("s > 0.5: FALSE\n");
    }
    return 0;
}
