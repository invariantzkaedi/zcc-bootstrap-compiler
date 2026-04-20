#include <stdio.h>
typedef struct { float x, y, z; } vec3f;
void foo(vec3f a) {}
int main() {
    foo((vec3f){1.0f, 2.0f, 3.0f});
    return 0;
}
