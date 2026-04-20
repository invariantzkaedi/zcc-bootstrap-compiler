#include <stdio.h>
int main(void) {
    double sum = 0.0;
    sum = sum + 1.5;
    sum = sum + 2.5;
    printf("sum after 2 adds: %f\n", sum);
    
    // Now in a loop
    double sum2 = 0.0;
    for (int i = 0; i < 2; i++) {
        sum2 = sum2 + 1.5;
    }
    printf("sum2 in loop: %f\n", sum2);
    return 0;
}
