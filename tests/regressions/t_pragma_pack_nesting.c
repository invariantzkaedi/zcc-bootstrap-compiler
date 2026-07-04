#include <stdio.h>
#include <stddef.h>

#pragma pack(push, 2)
struct S1 {
    char a;
    int b;
};

#pragma pack(push, 1)
struct S2 {
    char a;
    int b;
};
#pragma pack(pop)

struct S3 {
    char a;
    int b;
};
#pragma pack(pop)

struct S4 {
    char a;
    int b;
};

int main(void) {
    printf("S1: sizeof=%zu, off_b=%zu\n", sizeof(struct S1), offsetof(struct S1, b));
    printf("S2: sizeof=%zu, off_b=%zu\n", sizeof(struct S2), offsetof(struct S2, b));
    printf("S3: sizeof=%zu, off_b=%zu\n", sizeof(struct S3), offsetof(struct S3, b));
    printf("S4: sizeof=%zu, off_b=%zu\n", sizeof(struct S4), offsetof(struct S4, b));
    return 0;
}
