#include <stdio.h>

const double w1[8] = {0.08780748, -0.02444191, 0.11449624, 0.26923618, 
                      -0.04155278, -0.04138996, 0.27916801, 0.13539492};

int main(void) {
    printf("w1[0] = %f\n", w1[0]);
    printf("w1[4] = %f\n", w1[4]);
    
    double sum = 0.0;
    for (int i = 0; i < 8; i++) {
        sum = sum + w1[i];
    }
    printf("sum = %f\n", sum);
    return 0;
}
