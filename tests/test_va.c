#include <stdio.h>

void test_varargs(int a, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, a);
    printf("done\n");
}

int main() {
    test_varargs(1, 2, 3);
    return 0;
}
