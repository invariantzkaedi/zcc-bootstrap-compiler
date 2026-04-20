/* test_float_array_bug.c — CG-FLOAT-001 minimal reproducer */

#include <stdio.h>

static const float arr[10] = {
    1.5f, 2.5f, 3.5f, 4.5f, 5.5f,
    6.5f, 7.5f, 8.5f, 9.5f, 10.5f
};

int main(void) {
    float x;
    float y;
    float sum;
    int i;

    /* Test 1: simple indexed access */
    x = arr[2];
    printf("arr[2]*1000 = %d (expected: 3500)\n", (int)(x * 1000.0f));

    /* Test 2: computed index */
    i = 3;
    y = arr[i];
    printf("arr[3]*1000 = %d (expected: 4500)\n", (int)(y * 1000.0f));

    /* Test 3: loop accumulation */
    sum = 0.0f;
    for (i = 0; i < 4; i++) {
        sum = sum + arr[i];
    }
    printf("sum*1000 = %d (expected: 16000)\n", (int)(sum * 1000.0f));

    return 0;
}
