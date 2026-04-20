#include <stdio.h>

int test_pow2(int x) {
    return x * 8; // Should become x << 3
}

int test_double_neg(int x) {
    return -(-x); // Should become copy x
}

int test_double_not(int x) {
    return ~(~x); // Should become copy x
}

int main() {
    printf("pow2: %d\n", test_pow2(5));
    printf("dneg: %d\n", test_double_neg(42));
    printf("dnot: %d\n", test_double_not(100));
    return 0;
}
