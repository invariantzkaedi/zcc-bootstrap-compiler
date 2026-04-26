
#include <stddef.h>
#include <stdio.h>


union U {
    char c;
    int i;
    double d;
};


int main(void) {
    printf("sizeof=%zu\n", sizeof(union U));
    printf("alignof=%zu\n", _Alignof(union U));
    printf("offsetof.c=%zu\n", offsetof(union U, c));
    printf("offsetof.i=%zu\n", offsetof(union U, i));
    printf("offsetof.d=%zu\n", offsetof(union U, d));

    return 0;
}
