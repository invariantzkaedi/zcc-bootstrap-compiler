#include <stdio.h>
float add_floats(float a, float b) { return a + b; }
float mul_floats(float a, float b) { return a * b; }
int float_to_int(float f) { return (int)f; }

int main() {
    float x = 3.14f;
    float y = 2.0f;
    float sum = add_floats(x, y);   /* expected: 5.14 */
    float prod = mul_floats(x, y);  /* expected: 6.28 */
    int n = float_to_int(prod);
    printf("sum=%f prod=%f n=%d\n", sum, prod, n);
    return n - 6;  /* rc=0 = PASS */
}
