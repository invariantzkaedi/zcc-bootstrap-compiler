#include "test_bitfield_abi.h"

void mutate_s1(struct S1 *s) {
    s->a = 6;
    s->b = 25;
}

void mutate_s2(struct S2 *s) {
    s->a = 'Z';
    s->b = 99;
    s->c = -250;
}

void mutate_s3(struct S3 *s) {
    s->a = 5000000000UL; /* fits in 33 bits */
    s->b = 6;
}

void mutate_s4(struct S4 *s) {
    s->a = -7;
    s->b = 2049;
    s->c = 'K';
}
