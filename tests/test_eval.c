#include <stdio.h>
static float relu(float x) { return (x > 0.0f) ? x : 0.0f; }
int main() {
    float features[2];
    features[0] = 1.5f; features[1] = -2.5f;
    float w[2];
    w[0] = 2.0f; w[1] = 3.0f;
    float sum = 0.5f;
    int i;
    for (i = 0; i < 2; i++) sum = sum + features[i] * w[i];
    float h = relu(sum);
    printf("sum*1000 = %d, h*1000 = %d\n", (int)(sum*1000.0f), (int)(h*1000.0f));
    return 0;
}
