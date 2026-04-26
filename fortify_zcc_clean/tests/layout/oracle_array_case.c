
#include <stddef.h>
#include <stdio.h>


struct ArrayCase {
    char a[3];
    int b[2];
};


int main(void) {
    printf("sizeof=%zu\n", sizeof(struct ArrayCase));
    printf("alignof=%zu\n", _Alignof(struct ArrayCase));
    printf("offsetof.a=%zu\n", offsetof(struct ArrayCase, a));
    printf("offsetof.b=%zu\n", offsetof(struct ArrayCase, b));

    return 0;
}
