#include <stdio.h>

double sqrt(double);
float sqrtf(float);
double fabs(double);
float fabsf(float);

int main(void) {
    double d = -9.0;
    float f = -16.0f;
    printf("sqrt=%.6f\n", sqrt(9.0));
    printf("sqrtf=%.6f\n", (double)sqrtf(16.0f));
    printf("fabs=%.6f\n", fabs(d));
    printf("fabsf=%.6f\n", (double)fabsf(f));
    return 0;
}
