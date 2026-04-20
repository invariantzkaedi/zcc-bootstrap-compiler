#include <stdio.h>
static float relu(float x) { 
    return (float)((x > 0.0f) ? x : 0.0f); 
}
int main() {
    float x = 1.0f;
    float h = relu(x);
    printf("relu(1.0)*1000 = %d\n", (int)(h*1000.0f));
    return 0;
}
