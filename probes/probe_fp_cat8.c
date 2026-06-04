#include <stdio.h>

static const float SCALE = 1.0f / 255.0f;

int main(void) {
    /* Compile-time ternary with float condition */
    double ternary_val1 = (1.5f > 0.5f) ? 10.0 : 20.0;
    double ternary_val2 = (0.5f > 1.5f) ? 10.0 : 20.0;

    /* Static const float used in initializer of another variable (local) */
    const float VAL_A = 1.25f;
    float val_b = VAL_A * 2.0f;

    printf("ternary_val1: %f\n", ternary_val1);
    printf("ternary_val2: %f\n", ternary_val2);
    printf("SCALE: %e\n", SCALE);
    printf("val_b: %f\n", val_b);

    return 0;
}
