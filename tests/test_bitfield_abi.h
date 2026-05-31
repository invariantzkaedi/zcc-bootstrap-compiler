#ifndef TEST_BITFIELD_ABI_H
#define TEST_BITFIELD_ABI_H

struct S1 {
    unsigned int a : 3;
    unsigned int b : 5;
};

struct S2 {
    char a;
    unsigned int b : 7;
    long c : 9;
};

struct S3 {
    unsigned long a : 33;
    unsigned int b : 3;
};

struct S4 {
    signed int a : 4;
    unsigned int b : 12;
    char c;
};

void mutate_s1(struct S1 *s);
void mutate_s2(struct S2 *s);
void mutate_s3(struct S3 *s);
void mutate_s4(struct S4 *s);

#endif
