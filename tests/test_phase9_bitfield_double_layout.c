#include <stdio.h>

typedef struct {
    unsigned a:3;
    unsigned b:29;
    double d;
    unsigned c:5;
    long x;
} Weird;

long consume(Weird w) {
    return w.a + w.b + w.c + w.x + (long)(w.d * 1000.0);
}

int main(void) {
    Weird w;
    w.a = 5;
    w.b = 123456;
    w.d = 6.25;
    w.c = 17;
    w.x = 999;
    printf("out=%ld\n", consume(w));
    return 0;
}
