#include <stdio.h>
#include <stddef.h>
#include "test_bitfield_abi.h"

int main() {
    struct S1 s1 = {0};
    struct S2 s2 = {0};
    struct S3 s3 = {0};
    struct S4 s4 = {0};

    printf("--- S1 Layout ---\n");
    printf("sizeof(S1) = %zu\n", sizeof(struct S1));
    printf("alignof(S1) = %zu\n", _Alignof(struct S1));

    printf("--- S2 Layout ---\n");
    printf("sizeof(S2) = %zu\n", sizeof(struct S2));
    printf("alignof(S2) = %zu\n", _Alignof(struct S2));
    printf("offsetof(S2, a) = %zu\n", offsetof(struct S2, a));

    printf("--- S3 Layout ---\n");
    printf("sizeof(S3) = %zu\n", sizeof(struct S3));
    printf("alignof(S3) = %zu\n", _Alignof(struct S3));

    printf("--- S4 Layout ---\n");
    printf("sizeof(S4) = %zu\n", sizeof(struct S4));
    printf("alignof(S4) = %zu\n", _Alignof(struct S4));
    printf("offsetof(S4, c) = %zu\n", offsetof(struct S4, c));

    /* Test local read/writes */
    s1.a = 3;
    s1.b = 15;
    printf("s1 fields: %u, %u\n", s1.a, s1.b);

    s2.a = 'X';
    s2.b = 45;
    s2.c = 200;
    printf("s2 fields: %c, %u, %ld\n", s2.a, s2.b, (long)s2.c);

    s3.a = 4000000000UL;
    s3.b = 5;
    printf("s3 fields: %lu, %u\n", s3.a, s3.b);

    s4.a = -3;
    s4.b = 1000;
    s4.c = 'Y';
    printf("s4 fields: %d, %u, %c\n", s4.a, s4.b, s4.c);

    /* Test cross-object mutations */
    mutate_s1(&s1);
    printf("mutated s1: %u, %u\n", s1.a, s1.b);

    mutate_s2(&s2);
    printf("mutated s2: %c, %u, %ld\n", s2.a, s2.b, (long)s2.c);

    mutate_s3(&s3);
    printf("mutated s3: %lu, %u\n", s3.a, s3.b);

    mutate_s4(&s4);
    printf("mutated s4: %d, %u, %c\n", s4.a, s4.b, s4.c);

    return 0;
}
