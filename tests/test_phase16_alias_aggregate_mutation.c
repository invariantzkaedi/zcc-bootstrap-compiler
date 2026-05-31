#include <stdio.h>

typedef struct { long a, b, c; } Triple;

void mutate(Triple *x, long *alias) {
    x->a += 7;
    *alias += 11;
    x->c = x->a ^ x->b ^ *alias;
}

int main(void) {
    Triple t = {1, 2, 3};
    mutate(&t, &t.b);
    printf("out=%ld\n", t.a + t.b + t.c);
    return 0;
}
