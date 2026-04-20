#include <stdio.h>
#include <stdarg.h>

int sum_args(int count, ...) {
    va_list ap;
    int total = 0;
    int i;
    va_start(ap, count);
    for (i = 0; i < count; i++) {
        total += va_arg(ap, int);
    }
    va_end(ap);
    return total;
}

char *get_str(int dummy, ...) {
    va_list ap;
    va_start(ap, dummy);
    char *s = va_arg(ap, char*);
    va_end(ap);
    return s;
}

int main() {
    int s = sum_args(3, 10, 20, 30);
    printf("sum = %d\n", s);
    char *msg = get_str(0, "hello world");
    printf("str = %s\n", msg);
    return 0;
}