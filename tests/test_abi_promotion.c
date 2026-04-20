#include <stdio.h>
double check_promotion(double x, int y) {
    return x + y;
}
int main() {
    double result = check_promotion(42, 10);
    printf("Result: %.1f\n", result);
    return (result == 52.0) ? 0 : 1;
}
