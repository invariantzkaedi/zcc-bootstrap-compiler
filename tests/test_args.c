#include <stdio.h>
static float compute(const float *features, const float *w, const float *b) {
    float sum = b[0];
    int i;
    for (i = 0; i < 2; i++) sum = sum + features[i] * w[i];
    return sum;
}

int main() {
    float features[2];
    features[0] = 1.5f; features[1] = -2.5f;
    float w[2];
    w[0] = 2.0f; w[1] = 3.0f;
    float b[1];
    b[0] = 5.5f;
    float h = compute(features, w, b);
    printf("sum*1000 = %d\n", (int)(h*1000.0f));
    return 0;
}
