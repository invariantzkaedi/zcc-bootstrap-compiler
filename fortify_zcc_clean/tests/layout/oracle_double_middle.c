
#include <stddef.h>
#include <stdio.h>


struct DoubleMiddle {
    char c;
    double d;
    int i;
};


int main(void) {
    printf("sizeof=%zu\n", sizeof(struct DoubleMiddle));
    printf("alignof=%zu\n", _Alignof(struct DoubleMiddle));
    printf("offsetof.c=%zu\n", offsetof(struct DoubleMiddle, c));
    printf("offsetof.d=%zu\n", offsetof(struct DoubleMiddle, d));
    printf("offsetof.i=%zu\n", offsetof(struct DoubleMiddle, i));

    return 0;
}
