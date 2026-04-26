
#include <stddef.h>
#include <stdio.h>


struct Basic {
    char c;
    int i;
    char tail;
};


int main(void) {
    printf("sizeof=%zu\n", sizeof(struct Basic));
    printf("alignof=%zu\n", _Alignof(struct Basic));
    printf("offsetof.c=%zu\n", offsetof(struct Basic, c));
    printf("offsetof.i=%zu\n", offsetof(struct Basic, i));
    printf("offsetof.tail=%zu\n", offsetof(struct Basic, tail));

    return 0;
}
