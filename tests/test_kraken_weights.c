#include <stdio.h>
#include "kraken_v2_2_weights_double.h"

int main(void) {
    printf("First 8 reentrancy_w1 weights:\n");
    for (int i = 0; i < 8; i++) {
        printf("  w1[%d] = %f\n", i, reentrancy_w1[i]);
    }
    
    printf("\nreentrancy_b2[0] = %f\n", reentrancy_b2[0]);
    
    // Compute sum
    double sum = 0.0;
    for (int i = 0; i < 8; i++) {
        sum = sum + reentrancy_w1[i];
    }
    printf("Sum of first 8: %f\n", sum);
    
    return 0;
}
