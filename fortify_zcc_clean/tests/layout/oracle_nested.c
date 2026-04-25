
#include <stddef.h>
#include <stdio.h>


struct Nested {
    char c;
    struct {
        int x;
        char y;
    } inner;
    double d;
};


int main(void) {
    printf("sizeof=%zu\n", sizeof(struct Nested));
    printf("alignof=%zu\n", _Alignof(struct Nested));
    printf("offsetof.c=%zu\n", offsetof(struct Nested, c));
    printf("offsetof.inner=%zu\n", offsetof(struct Nested, inner));
    printf("offsetof.d=%zu\n", offsetof(struct Nested, d));

    return 0;
}
