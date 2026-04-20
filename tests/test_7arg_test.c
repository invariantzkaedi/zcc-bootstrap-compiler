#include <stdio.h>

int test_7_args(int a, int b, int c, int d, int e, int f, int g) {
    return a + b + c + d + e + f + g;
}

int main() {
    int result = test_7_args(1, 2, 3, 4, 5, 6, 7);
    if (result == 28) {
        printf("SUCCESS: 7-arg passing works (%d)\n", result);
        return 0;
    } else {
        printf("FAILURE: Got %d\n", result);
        return 1;
    }
}
