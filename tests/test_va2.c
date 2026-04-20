#include <stdio.h>

int test_config(int, ...);

int test_config(int op, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, op);
    return 0;
}

int main() {
    test_config(1, 2, 3);
    return 0;
}
